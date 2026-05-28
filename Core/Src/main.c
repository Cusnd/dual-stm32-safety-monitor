/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
 * @brief          : Dual STM32F103C8T6 environment monitor firmware.
 *                   The same source is built as either the sensor node or
 *                   the monitor node by changing APP_NODE_ROLE in CMake.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "gpio.h"

#include <stdio.h>
#include <string.h>

/* Functions that belong to only one role are kept in the same file so the two
 * firmwares share one protocol definition.  This attribute prevents warnings
 * when a role-specific function is compiled but not called in the other role.
 * 只属于某一个节点的函数仍然放在同一个文件中，这样两个固件能共用同一套
 * 通信协议定义；该属性用于避免另一个节点没有调用这些函数时产生告警。
 */
#if defined(__GNUC__)
#define APP_MAYBE_UNUSED __attribute__((unused))
#else
#define APP_MAYBE_UNUSED
#endif

/* Build-time role selection.  CMake presets pass APP_NODE_ROLE=1 or 2, so the
 * sensor and monitor images are produced from the same firmware source.
 * 编译期角色选择：CMake 预设会传入 APP_NODE_ROLE=1 或 2，因此同一份源码
 * 可以分别生成采集节点和显示报警节点固件。
 */
#define APP_ROLE_SENSOR   1
#define APP_ROLE_MONITOR  2

#ifndef APP_NODE_ROLE
#define APP_NODE_ROLE APP_ROLE_MONITOR
#endif

/* Sensor-to-monitor frame:
 * AA 55 | LEN | TEMP HUMI MQ135_H MQ135_L MQ2_H MQ2_L FLAME SEQ STATUS | SUM
 * The checksum is the low 8 bits of LEN plus all payload bytes.
 * 采集节点到显示节点的数据帧：帧头为 AA 55，最后 1 字节为长度和负载累加
 * 后的低 8 位校验和，用于过滤串口噪声或错位数据。
 */
#define FRAME_HEAD0       0xAAu
#define FRAME_HEAD1       0x55u
#define FRAME_PAYLOAD_LEN 9u
#define FRAME_TOTAL_LEN   (2u + 1u + FRAME_PAYLOAD_LEN + 1u)
#define NODE_RX_BUF_SIZE  64u

#define STATUS_DHT_ERROR  0x01u

/* Timing values are expressed in milliseconds and compared with HAL_GetTick().
 * The code avoids long blocking delays in the monitor so serial reception,
 * buttons, display refresh, alarm output, and logging can cooperate.
 * 所有周期参数都用毫秒表示，并与 HAL_GetTick() 比较；显示节点尽量避免长时间
 * 阻塞，使串口接收、按键、刷新、报警和日志记录能在主循环中协作运行。
 */
#define USART_BAUDRATE    115200u
#define SENSOR_PERIOD_MS  1000u
#define UI_PERIOD_MS      300u
#define ALARM_PERIOD_MS   100u
#define NODE_TIMEOUT_MS   3000u
#define MUTE_TIME_MS      60000u

/* Sensor-node pins.  PA9/PA10 are intentionally not used here because this
 * board already routes them to the CH340C USB-to-UART bridge for USART1 debug.
 * 采集节点引脚：PA9/PA10 默认连接板载 CH340C，所以保留给 USART1 调试串口，
 * 不再分配给外部模块。
 */
#define DHT11_PORT        GPIOB
#define DHT11_PIN         GPIO_PIN_12
#define FLAME_PORT        GPIOB
#define FLAME_PIN         GPIO_PIN_13

/* Monitor-node pins.  OLED uses bit-banged I2C so PB6/PB7 stay as GPIO
 * open-drain outputs; the optional W25Q64 uses SPI2 when present.
 * 显示节点引脚：OLED 使用软件 I2C，因此 PB6/PB7 配置为 GPIO 开漏输出；
 * 可选 W25Q64 接入时使用 SPI2。
 */
#define OLED_PORT         GPIOB
#define OLED_SCL_PIN      GPIO_PIN_6
#define OLED_SDA_PIN      GPIO_PIN_7
#define BUZZER_PORT       GPIOB
#define BUZZER_PIN        GPIO_PIN_8
#define FLASH_CS_PORT     GPIOB
#define FLASH_CS_PIN      GPIO_PIN_12

typedef struct
{
  uint8_t temp;
  uint8_t humi;
  uint16_t mq135_adc;
  uint16_t mq2_adc;
  uint8_t flame;
  uint8_t seq;
  uint8_t status;
} SensorFrame;

/* Three threshold profiles make K2 long-press useful during demos: normal,
 * sensitive, and loose.  ADC values are raw 12-bit readings, not calibrated ppm.
 * 三组阈值用于演示 K2 长按切换灵敏度：普通、灵敏、宽松；这里的 ADC 数值
 * 是 12 位原始采样值，并不是经过标定的 ppm 浓度。
 */
typedef struct
{
  uint16_t air_warn;
  uint16_t smoke_warn;
  uint16_t smoke_danger;
} AlarmThresholds;

static const AlarmThresholds k_threshold_profiles[] =
{
  {2200u, 1800u, 2800u},
  {1800u, 1400u, 2400u},
  {2600u, 2200u, 3300u},
};

static SensorFrame g_latest_frame;
static uint32_t g_last_rx_ms = 0u;
static uint8_t g_page = 0u;
static uint8_t g_threshold_profile = 0u;
static uint32_t g_mute_until_ms = 0u;
static uint8_t g_flash_present = 0u;
static uint32_t g_flash_log_addr = 0u;
static volatile uint8_t g_node_rx_buf[NODE_RX_BUF_SIZE];
static volatile uint8_t g_node_rx_head = 0u;
static volatile uint8_t g_node_rx_tail = 0u;

void SystemClock_Config(void);
static void App_Init(void);
static void Sensor_App_Run(void);
static void Monitor_App_Run(void);
static void Debug_USART1_Init(void);
static void Node_USART3_Init(void);
static void USART_SendByte(USART_TypeDef *usart, uint8_t byte);
static void USART_SendBuffer(USART_TypeDef *usart, const uint8_t *data, uint16_t len);
static int USART_ReadByte(USART_TypeDef *usart);
static void Delay_Init(void);
static void Delay_Us(uint32_t us);
static void Sensor_GPIO_Init(void);
static void ADC1_Init_Custom(void);
static uint16_t ADC1_ReadChannel(uint8_t channel);
static uint8_t DHT11_Read(uint8_t *temp, uint8_t *humi);
static void Sensor_SendFrame(const SensorFrame *frame);
static uint8_t Frame_Checksum(const uint8_t *data, uint8_t len);
static uint8_t Frame_Encode(const SensorFrame *frame, uint8_t out[FRAME_TOTAL_LEN]);
static uint8_t Frame_Decode(const uint8_t in[FRAME_TOTAL_LEN], SensorFrame *frame);
static void Monitor_GPIO_Init(void);
static void Monitor_ProcessRx(void);
static void Monitor_UpdateButtons(void);
static void Monitor_UpdateAlarm(void);
static void Monitor_UpdateDisplay(void);
static uint8_t Monitor_NodeLost(void);
static uint8_t Monitor_Danger(void);
static uint8_t Monitor_Warn(void);
static void LED_Set(uint8_t red, uint8_t green, uint8_t blue);
static void Buzzer_Set(uint8_t on);
static void OLED_Init_Custom(void);
static void OLED_Clear(void);
static void OLED_SetCursor(uint8_t page, uint8_t col);
static void OLED_Puts(const char *text);
static void OLED_PrintLine(uint8_t page, const char *text);
static void Flash_Init_Custom(void);
static void Flash_LogFrame(const SensorFrame *frame, uint8_t state);

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  App_Init();

  /* Only one role loop is entered.  There is no RTOS here; each role runs a
   * cooperative super-loop for the lifetime of the board.
   * 程序只会进入其中一个角色主循环。本项目没有使用 RTOS，每个节点都通过
   * 裸机 super-loop 长期运行。
   */
#if APP_NODE_ROLE == APP_ROLE_SENSOR
  Sensor_App_Run();
#else
  Monitor_App_Run();
#endif
}

static void App_Init(void)
{
  Delay_Init();
  Debug_USART1_Init();
  Node_USART3_Init();

  /* Shared peripherals are initialized first, then role-specific peripherals.
   * USART1 is always kept as the board-side USB debug channel.
   * 先初始化两个节点都会用到的外设，再初始化角色专属外设；USART1 始终
   * 作为板载 USB 转串口调试通道。
   */
#if APP_NODE_ROLE == APP_ROLE_SENSOR
  Sensor_GPIO_Init();
  ADC1_Init_Custom();
  printf("\r\n[SENSOR] boot, USART1 debug ready, USART3 link ready\r\n");
#else
  Monitor_GPIO_Init();
  Flash_Init_Custom();
  OLED_Init_Custom();
  OLED_Clear();
  OLED_PrintLine(0, "MONITOR NODE");
  OLED_PrintLine(2, "WAIT SENSOR");
  printf("\r\n[MONITOR] boot, USART1 debug ready, USART3 link ready, flash=%s\r\n",
         g_flash_present ? "ok" : "none");
#endif
}

static APP_MAYBE_UNUSED void Sensor_App_Run(void)
{
  uint32_t last_sensor_ms = 0u;
  uint8_t seq = 0u;
  uint8_t temp = 0u;
  uint8_t humi = 0u;
  uint8_t avg_valid = 0u;
  uint16_t mq135_avg = 0u;
  uint16_t mq2_avg = 0u;

  while (1)
  {
    const uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - last_sensor_ms) >= SENSOR_PERIOD_MS)
    {
      SensorFrame frame;
      const uint16_t mq135_raw = ADC1_ReadChannel(4u);
      const uint16_t mq2_raw = ADC1_ReadChannel(5u);
      const uint8_t dht_ok = DHT11_Read(&temp, &humi);

      /* A lightweight exponential moving average smooths noisy MQ sensor ADC
       * values without storing a full sample window.
       * 使用轻量级指数滑动平均来平滑 MQ 传感器 ADC 噪声，不需要保存完整
       * 采样窗口，适合资源有限的单片机。
       */
      if (!avg_valid)
      {
        mq135_avg = mq135_raw;
        mq2_avg = mq2_raw;
        avg_valid = 1u;
      }
      else
      {
        mq135_avg = (uint16_t)(((uint32_t)mq135_avg * 3u + mq135_raw) / 4u);
        mq2_avg = (uint16_t)(((uint32_t)mq2_avg * 3u + mq2_raw) / 4u);
      }

      frame.temp = temp;
      frame.humi = humi;
      frame.mq135_adc = mq135_avg;
      frame.mq2_adc = mq2_avg;
      /* The flame module used by this project is treated as active-low.
       * 本项目按低电平有效处理火焰模块：读到 RESET 表示触发。
       */
      frame.flame = (HAL_GPIO_ReadPin(FLAME_PORT, FLAME_PIN) == GPIO_PIN_RESET) ? 1u : 0u;
      frame.seq = seq++;
      frame.status = dht_ok ? 0u : STATUS_DHT_ERROR;

      Sensor_SendFrame(&frame);
      printf("[SENSOR] seq=%u t=%u h=%u mq135=%u mq2=%u flame=%u status=0x%02X\r\n",
             frame.seq, frame.temp, frame.humi, frame.mq135_adc, frame.mq2_adc,
             frame.flame, frame.status);

      last_sensor_ms = now;
    }
  }
}

static APP_MAYBE_UNUSED void Monitor_App_Run(void)
{
  uint32_t last_ui_ms = 0u;
  uint32_t last_alarm_ms = 0u;
  uint32_t last_log_ms = 0u;

  /* Start in lost-node state so the OLED immediately says NODE LOST until the
   * first valid SENSOR frame arrives.
   * 上电后先进入节点丢失状态，直到收到第一帧合法 SENSOR 数据前，OLED 会
   * 显示 NODE LOST，便于检查通信线是否接好。
   */
  g_last_rx_ms = HAL_GetTick() - NODE_TIMEOUT_MS - 1u;

  while (1)
  {
    const uint32_t now = HAL_GetTick();

    /* Fast, frequent tasks are called every pass; slower tasks are gated by
     * elapsed time so the monitor stays responsive without an RTOS.
     * 高频任务每轮主循环都执行，低频任务用时间间隔限制；这样没有 RTOS 也能
     * 保持显示节点响应及时。
     */
    Monitor_ProcessRx();
    Monitor_UpdateButtons();

    if ((uint32_t)(now - last_alarm_ms) >= ALARM_PERIOD_MS)
    {
      Monitor_UpdateAlarm();
      last_alarm_ms = now;
    }

    if ((uint32_t)(now - last_ui_ms) >= UI_PERIOD_MS)
    {
      Monitor_UpdateDisplay();
      last_ui_ms = now;
    }

    if ((g_flash_present != 0u) && !Monitor_NodeLost() &&
        ((uint32_t)(now - last_log_ms) >= 10000u))
    {
      const uint8_t state = Monitor_Danger() ? 2u : (Monitor_Warn() ? 1u : 0u);
      Flash_LogFrame(&g_latest_frame, state);
      last_log_ms = now;
    }
  }
}

static void Debug_USART1_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();

  /* PA9/PA10 are wired to the on-board CH340C by default, so USART1 remains
   * the dedicated printf/debug channel for both boards.
   * PA9/PA10 默认连接板载 CH340C，因此两块板都把 USART1 固定作为 printf
   * 调试串口。
   */
  gpio.Pin = GPIO_PIN_9;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &gpio);

  gpio.Pin = GPIO_PIN_10;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &gpio);

  USART1->BRR = (uint16_t)((72000000u + (USART_BAUDRATE / 2u)) / USART_BAUDRATE);
  USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

static void Node_USART3_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_USART3_CLK_ENABLE();

  /* PB10/PB11 are free from board-level USB/SWD/RGB conflicts and form the
   * direct board-to-board link: TX must be crossed to the other board's RX.
   * PB10/PB11 不与 USB、SWD、板载 RGB 冲突，适合做双板直连通信；接线时
   * TX 必须交叉连接到对方 RX。
   */
  gpio.Pin = GPIO_PIN_10;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &gpio);

  gpio.Pin = GPIO_PIN_11;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &gpio);

  USART3->BRR = (uint16_t)((36000000u + (USART_BAUDRATE / 2u)) / USART_BAUDRATE);
  USART3->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

#if APP_NODE_ROLE == APP_ROLE_MONITOR
  /* The monitor refreshes the OLED with bit-banged I2C.  Interrupt-driven
   * USART3 reception prevents incoming frame bytes from being missed during
   * those display updates.
   * 显示节点用软件 I2C 刷 OLED，刷新期间 CPU 会忙一小段时间；USART3 用
   * 中断接收可以降低丢字节概率。
   */
  USART3->CR1 |= USART_CR1_RXNEIE;
  HAL_NVIC_SetPriority(USART3_IRQn, 1u, 0u);
  HAL_NVIC_EnableIRQ(USART3_IRQn);
#endif
}

static void USART_SendByte(USART_TypeDef *usart, uint8_t byte)
{
  while ((usart->SR & USART_SR_TXE) == 0u)
  {
  }
  usart->DR = byte;
}

static void USART_SendBuffer(USART_TypeDef *usart, const uint8_t *data, uint16_t len)
{
  for (uint16_t i = 0u; i < len; i++)
  {
    USART_SendByte(usart, data[i]);
  }
}

static int USART_ReadByte(USART_TypeDef *usart)
{
  if (usart == USART3)
  {
    uint8_t data;
    if (g_node_rx_head == g_node_rx_tail)
    {
      return -1;
    }
    data = g_node_rx_buf[g_node_rx_tail];
    g_node_rx_tail = (uint8_t)((g_node_rx_tail + 1u) % NODE_RX_BUF_SIZE);
    return (int)data;
  }

  if ((usart->SR & USART_SR_RXNE) == 0u)
  {
    return -1;
  }
  return (int)(usart->DR & 0xFFu);
}

void USART3_IRQHandler(void)
{
  if ((USART3->SR & (USART_SR_RXNE | USART_SR_ORE)) != 0u)
  {
    const uint8_t data = (uint8_t)(USART3->DR & 0xFFu);
    const uint8_t next = (uint8_t)((g_node_rx_head + 1u) % NODE_RX_BUF_SIZE);
    /* Drop the newest byte if the buffer is full.  Losing one frame is safer
     * than blocking inside an interrupt handler.
     * 如果环形缓冲满了就丢弃最新字节；相比在中断里等待，偶尔丢一帧更安全。
     */
    if (next != g_node_rx_tail)
    {
      g_node_rx_buf[g_node_rx_head] = data;
      g_node_rx_head = next;
    }
  }
}

int __io_putchar(int ch)
{
  if (ch == '\n')
  {
    USART_SendByte(USART1, (uint8_t)'\r');
  }
  USART_SendByte(USART1, (uint8_t)ch);
  return ch;
}

static void Delay_Init(void)
{
  /* DHT11 timing needs microsecond delays.  DWT CYCCNT is available on the
   * Cortex-M3 core and gives a simple cycle counter without using a timer.
   * DHT11 时序需要微秒级延时；Cortex-M3 内核的 DWT CYCCNT 可直接作为周期
   * 计数器使用，不额外占用定时器。
   */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0u;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void Delay_Us(uint32_t us)
{
  const uint32_t ticks = us * (SystemCoreClock / 1000000u);
  const uint32_t start = DWT->CYCCNT;
  while ((uint32_t)(DWT->CYCCNT - start) < ticks)
  {
  }
}

static APP_MAYBE_UNUSED void Sensor_GPIO_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* MQ135 and MQ2 are read from analog outputs, so PA4/PA5 must be analog
   * inputs before ADC1 samples channels 4 and 5.
   * MQ135 和 MQ2 读取模拟输出，因此 PA4/PA5 必须先配置成模拟输入，再由
   * ADC1 采样通道 4 和 5。
   */
  gpio.Pin = GPIO_PIN_4 | GPIO_PIN_5;
  gpio.Mode = GPIO_MODE_ANALOG;
  HAL_GPIO_Init(GPIOA, &gpio);

  gpio.Pin = FLAME_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(FLAME_PORT, &gpio);

  gpio.Pin = DHT11_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(DHT11_PORT, &gpio);
  /* DHT11 bus idles high; open-drain plus pull-up lets the sensor pull low.
   * DHT11 总线空闲为高电平；开漏输出配合上拉，允许传感器主动拉低数据线。
   */
  HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
}

static APP_MAYBE_UNUSED void ADC1_Init_Custom(void)
{
  __HAL_RCC_ADC1_CLK_ENABLE();

  /* ADC clock is 72 MHz / 6 = 12 MHz, safely below the STM32F1 ADC limit.
   * ADC 时钟为 72 MHz / 6 = 12 MHz，低于 STM32F1 ADC 允许上限。
   */
  RCC->CFGR &= ~RCC_CFGR_ADCPRE;
  RCC->CFGR |= RCC_CFGR_ADCPRE_DIV6;

  ADC1->CR1 = 0u;
  ADC1->CR2 = ADC_CR2_ADON | ADC_CR2_EXTTRIG | ADC_CR2_EXTSEL;
  ADC1->SMPR2 |= ADC_SMPR2_SMP4 | ADC_SMPR2_SMP5;
  Delay_Us(10u);

  ADC1->CR2 |= ADC_CR2_RSTCAL;
  while ((ADC1->CR2 & ADC_CR2_RSTCAL) != 0u)
  {
  }
  ADC1->CR2 |= ADC_CR2_CAL;
  while ((ADC1->CR2 & ADC_CR2_CAL) != 0u)
  {
  }
}

static uint16_t ADC1_ReadChannel(uint8_t channel)
{
  ADC1->SQR1 = 0u;
  ADC1->SQR3 = channel & ADC_SQR3_SQ1;
  ADC1->SR = 0u;
  ADC1->CR2 |= ADC_CR2_SWSTART;
  while ((ADC1->SR & ADC_SR_EOC) == 0u)
  {
  }
  return (uint16_t)(ADC1->DR & 0x0FFFu);
}

static void DHT11_SetOutput(void)
{
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = DHT11_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(DHT11_PORT, &gpio);
}

static void DHT11_SetInput(void)
{
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = DHT11_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(DHT11_PORT, &gpio);
}

static uint8_t DHT11_WaitLevel(GPIO_PinState level, uint32_t timeout_us)
{
  const uint32_t ticks = timeout_us * (SystemCoreClock / 1000000u);
  const uint32_t start = DWT->CYCCNT;
  while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) != level)
  {
    if ((uint32_t)(DWT->CYCCNT - start) > ticks)
    {
      return 0u;
    }
  }
  return 1u;
}

static uint8_t DHT11_Read(uint8_t *temp, uint8_t *humi)
{
  uint8_t data[5] = {0u, 0u, 0u, 0u, 0u};

  /* Start signal: the MCU holds the bus low long enough for DHT11 to detect a
   * request, then releases the line and waits for the sensor response pulses.
   * 起始信号：MCU 先把总线拉低足够长时间，让 DHT11 识别到读取请求，然后
   * 释放总线等待传感器回应脉冲。
   */
  DHT11_SetOutput();
  HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
  HAL_Delay(20u);
  HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
  Delay_Us(30u);
  DHT11_SetInput();

  if (!DHT11_WaitLevel(GPIO_PIN_RESET, 100u) ||
      !DHT11_WaitLevel(GPIO_PIN_SET, 100u) ||
      !DHT11_WaitLevel(GPIO_PIN_RESET, 100u))
  {
    return 0u;
  }

  for (uint8_t i = 0u; i < 40u; i++)
  {
    if (!DHT11_WaitLevel(GPIO_PIN_SET, 70u))
    {
      return 0u;
    }
    /* Around 40 us after the rising edge, a short pulse means 0 and a longer
     * pulse is still high and means 1.
     * 上升沿后约 40 us 读取电平：短高电平已结束表示 0，长高电平仍保持表示 1。
     */
    Delay_Us(40u);
    if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
    {
      data[i / 8u] |= (uint8_t)(1u << (7u - (i % 8u)));
      if (!DHT11_WaitLevel(GPIO_PIN_RESET, 80u))
      {
        return 0u;
      }
    }
  }

  if (((uint8_t)(data[0] + data[1] + data[2] + data[3])) != data[4])
  {
    return 0u;
  }

  *humi = data[0];
  *temp = data[2];
  return 1u;
}

static void Sensor_SendFrame(const SensorFrame *frame)
{
  uint8_t bytes[FRAME_TOTAL_LEN];
  const uint8_t len = Frame_Encode(frame, bytes);
  USART_SendBuffer(USART3, bytes, len);
}

static uint8_t Frame_Checksum(const uint8_t *data, uint8_t len)
{
  uint8_t sum = 0u;
  for (uint8_t i = 0u; i < len; i++)
  {
    sum = (uint8_t)(sum + data[i]);
  }
  return sum;
}

static uint8_t Frame_Encode(const SensorFrame *frame, uint8_t out[FRAME_TOTAL_LEN])
{
  /* Multi-byte ADC values are sent high byte first so the receiver can rebuild
   * the original uint16_t without depending on CPU endianness.
   * 多字节 ADC 值按高字节在前发送，接收端可稳定还原 uint16_t，不依赖 CPU
   * 的大小端存储方式。
   */
  out[0] = FRAME_HEAD0;
  out[1] = FRAME_HEAD1;
  out[2] = FRAME_PAYLOAD_LEN;
  out[3] = frame->temp;
  out[4] = frame->humi;
  out[5] = (uint8_t)(frame->mq135_adc >> 8);
  out[6] = (uint8_t)(frame->mq135_adc & 0xFFu);
  out[7] = (uint8_t)(frame->mq2_adc >> 8);
  out[8] = (uint8_t)(frame->mq2_adc & 0xFFu);
  out[9] = frame->flame;
  out[10] = frame->seq;
  out[11] = frame->status;
  out[12] = Frame_Checksum(&out[2], (uint8_t)(1u + FRAME_PAYLOAD_LEN));
  return FRAME_TOTAL_LEN;
}

static uint8_t Frame_Decode(const uint8_t in[FRAME_TOTAL_LEN], SensorFrame *frame)
{
  /* Header, fixed length, and checksum are checked before updating the latest
   * monitor data.  Bad frames are ignored so noise cannot corrupt the display.
   * 更新显示数据前先检查帧头、固定长度和校验和；错误帧会被丢弃，避免串口
   * 噪声污染 OLED 数据。
   */
  if ((in[0] != FRAME_HEAD0) || (in[1] != FRAME_HEAD1) || (in[2] != FRAME_PAYLOAD_LEN))
  {
    return 0u;
  }
  if (Frame_Checksum(&in[2], (uint8_t)(1u + FRAME_PAYLOAD_LEN)) != in[12])
  {
    return 0u;
  }

  frame->temp = in[3];
  frame->humi = in[4];
  frame->mq135_adc = (uint16_t)(((uint16_t)in[5] << 8) | in[6]);
  frame->mq2_adc = (uint16_t)(((uint16_t)in[7] << 8) | in[8]);
  frame->flame = in[9];
  frame->seq = in[10];
  frame->status = in[11];
  return 1u;
}

static APP_MAYBE_UNUSED void Monitor_GPIO_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();

  gpio.Pin = BUZZER_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BUZZER_PORT, &gpio);
  Buzzer_Set(0u);

  gpio.Pin = OLED_SCL_PIN | OLED_SDA_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(OLED_PORT, &gpio);
  /* Software I2C idles with both lines released high.
   * 软件 I2C 空闲时 SCL/SDA 都释放为高电平。
   */
  HAL_GPIO_WritePin(OLED_PORT, OLED_SCL_PIN | OLED_SDA_PIN, GPIO_PIN_SET);

  LED_Set(0u, 0u, 1u);
}

static void Monitor_ProcessRx(void)
{
  static uint8_t buf[FRAME_TOTAL_LEN];
  static uint8_t pos = 0u;
  int rx;

  /* USART3 ISR stores bytes in a ring buffer; the parser here resynchronizes
   * on AA 55 so it can recover after a dropped or noisy byte.
   * USART3 中断只负责把字节放入环形缓冲；这里的解析器通过 AA 55 帧头重新
   * 同步，因此遇到丢字节或噪声后也能恢复。
   */
  while ((rx = USART_ReadByte(USART3)) >= 0)
  {
    const uint8_t b = (uint8_t)rx;

    if (pos == 0u)
    {
      if (b != FRAME_HEAD0)
      {
        continue;
      }
    }
    else if ((pos == 1u) && (b != FRAME_HEAD1))
    {
      pos = (b == FRAME_HEAD0) ? 1u : 0u;
      if (pos == 1u)
      {
        buf[0] = FRAME_HEAD0;
      }
      continue;
    }

    buf[pos++] = b;
    if (pos >= FRAME_TOTAL_LEN)
    {
      SensorFrame frame;
      if (Frame_Decode(buf, &frame))
      {
        g_latest_frame = frame;
        g_last_rx_ms = HAL_GetTick();
        printf("[MONITOR] rx seq=%u t=%u h=%u mq135=%u mq2=%u flame=%u status=0x%02X\r\n",
               frame.seq, frame.temp, frame.humi, frame.mq135_adc, frame.mq2_adc,
               frame.flame, frame.status);
      }
      else
      {
        printf("[MONITOR] bad frame\r\n");
      }
      pos = 0u;
    }
  }
}

static void Monitor_UpdateButtons(void)
{
  static uint8_t k1_last = 0u;
  static uint8_t k2_last = 0u;
  static uint32_t k2_down_ms = 0u;

  const uint32_t now = HAL_GetTick();
  const uint8_t k1 = (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_SET) ? 1u : 0u;
  const uint8_t k2 = (HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_SET) ? 1u : 0u;

  /* The on-board keys are active-high.  This simple edge detector performs one
   * action per press instead of repeating while the key is held down.
   * 板载按键为高电平有效；简单边沿检测确保每次按下只触发一次动作，避免
   * 长按时不断重复。
   */
  if ((k1 != 0u) && (k1_last == 0u))
  {
    g_page ^= 1u;
  }

  if ((k2 != 0u) && (k2_last == 0u))
  {
    k2_down_ms = now;
  }
  else if ((k2 == 0u) && (k2_last != 0u))
  {
    const uint32_t held = now - k2_down_ms;
    if (held >= 1200u)
    {
      g_threshold_profile = (uint8_t)((g_threshold_profile + 1u) %
                                      (sizeof(k_threshold_profiles) / sizeof(k_threshold_profiles[0])));
      printf("[MONITOR] threshold profile=%u\r\n", g_threshold_profile);
    }
    else
    {
      g_mute_until_ms = now + MUTE_TIME_MS;
      printf("[MONITOR] buzzer muted for 60s\r\n");
    }
  }

  k1_last = k1;
  k2_last = k2;
}

static uint8_t Monitor_NodeLost(void)
{
  return ((uint32_t)(HAL_GetTick() - g_last_rx_ms) > NODE_TIMEOUT_MS) ? 1u : 0u;
}

static uint8_t Monitor_Danger(void)
{
  const AlarmThresholds *th = &k_threshold_profiles[g_threshold_profile];
  if (Monitor_NodeLost())
  {
    return 0u;
  }
  return ((g_latest_frame.flame != 0u) || (g_latest_frame.mq2_adc >= th->smoke_danger)) ? 1u : 0u;
}

static uint8_t Monitor_Warn(void)
{
  const AlarmThresholds *th = &k_threshold_profiles[g_threshold_profile];
  if (Monitor_NodeLost())
  {
    return 1u;
  }
  return (((g_latest_frame.status & STATUS_DHT_ERROR) != 0u) ||
          (g_latest_frame.mq135_adc >= th->air_warn) ||
          (g_latest_frame.mq2_adc >= th->smoke_warn)) ? 1u : 0u;
}

static void Monitor_UpdateAlarm(void)
{
  const uint32_t now = HAL_GetTick();
  const uint8_t muted = ((int32_t)(g_mute_until_ms - now) > 0) ? 1u : 0u;

  /* Alarm priority is important: real danger beats node-lost, node-lost beats
   * normal warning, and muted only suppresses the buzzer, not the LED color.
   * 报警优先级很重要：真实危险高于节点丢失，节点丢失高于普通预警；静音
   * 只关闭蜂鸣器，不改变 LED 状态颜色。
   */
  if (Monitor_Danger())
  {
    LED_Set(1u, 0u, 0u);
    Buzzer_Set((muted == 0u) && ((now / 150u) % 2u == 0u));
  }
  else if (Monitor_NodeLost())
  {
    LED_Set(0u, 0u, 1u);
    Buzzer_Set((muted == 0u) && ((now / 700u) % 2u == 0u));
  }
  else if (Monitor_Warn())
  {
    LED_Set(1u, 1u, 0u);
    Buzzer_Set(0u);
  }
  else
  {
    LED_Set(0u, 1u, 0u);
    Buzzer_Set(0u);
  }
}

static void Monitor_UpdateDisplay(void)
{
  char line[24];

  /* The screen is small, so each page is kept to four concise text rows.
   * OLED 屏幕较小，因此每页只显示四行短文本，保证演示时清楚可读。
   */
  OLED_Clear();

  if (g_page == 0u)
  {
    OLED_PrintLine(0, Monitor_NodeLost() ? "NODE LOST" :
                      (Monitor_Danger() ? "STATE DANGER" :
                       (Monitor_Warn() ? "STATE WARN" : "STATE NORMAL")));
    snprintf(line, sizeof(line), "T:%02uC H:%02u%%", g_latest_frame.temp, g_latest_frame.humi);
    OLED_PrintLine(1, line);
    snprintf(line, sizeof(line), "AIR:%04u", g_latest_frame.mq135_adc);
    OLED_PrintLine(2, line);
    snprintf(line, sizeof(line), "MQ2:%04u F:%s", g_latest_frame.mq2_adc,
             g_latest_frame.flame ? "YES" : "NO");
    OLED_PrintLine(3, line);
  }
  else
  {
    const AlarmThresholds *th = &k_threshold_profiles[g_threshold_profile];
    snprintf(line, sizeof(line), "PROFILE:%u", g_threshold_profile);
    OLED_PrintLine(0, line);
    snprintf(line, sizeof(line), "AIR TH:%04u", th->air_warn);
    OLED_PrintLine(1, line);
    snprintf(line, sizeof(line), "SMK TH:%04u/%04u", th->smoke_warn, th->smoke_danger);
    OLED_PrintLine(2, line);
    snprintf(line, sizeof(line), "SEQ:%03u F:%s", g_latest_frame.seq,
             g_flash_present ? "OK" : "NO");
    OLED_PrintLine(3, line);
  }
}

static void LED_Set(uint8_t red, uint8_t green, uint8_t blue)
{
  /* Board RGB LEDs are wired to 3V3 through resistors, so driving the MCU pin
   * low turns the selected LED on.
   * 板载 RGB LED 通过电阻接到 3V3，因此 MCU 输出低电平时对应颜色点亮。
   */
  HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, red ? GPIO_PIN_RESET : GPIO_PIN_SET);
  HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, green ? GPIO_PIN_RESET : GPIO_PIN_SET);
  HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, blue ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void Buzzer_Set(uint8_t on)
{
  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void I2C_Delay(void)
{
  Delay_Us(4u);
}

static void I2C_SDA(uint8_t high)
{
  HAL_GPIO_WritePin(OLED_PORT, OLED_SDA_PIN, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void I2C_SCL(uint8_t high)
{
  HAL_GPIO_WritePin(OLED_PORT, OLED_SCL_PIN, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void I2C_Start(void)
{
  I2C_SDA(1u);
  I2C_SCL(1u);
  I2C_Delay();
  I2C_SDA(0u);
  I2C_Delay();
  I2C_SCL(0u);
}

static void I2C_Stop(void)
{
  I2C_SDA(0u);
  I2C_SCL(1u);
  I2C_Delay();
  I2C_SDA(1u);
  I2C_Delay();
}

static void I2C_WriteByte(uint8_t byte)
{
  for (uint8_t i = 0u; i < 8u; i++)
  {
    I2C_SDA((byte & 0x80u) ? 1u : 0u);
    I2C_Delay();
    I2C_SCL(1u);
    I2C_Delay();
    I2C_SCL(0u);
    byte <<= 1;
  }

  /* ACK is intentionally ignored.  The OLED is display-only in this project,
   * and keeping this tiny software-I2C driver simple is enough for the demo.
   * 这里有意忽略 ACK：OLED 只用于显示，演示场景下保持软件 I2C 驱动简单即可。
   */
  I2C_SDA(1u);
  I2C_Delay();
  I2C_SCL(1u);
  I2C_Delay();
  I2C_SCL(0u);
}

static void OLED_Write(uint8_t control, uint8_t data)
{
  /* SSD1306 commonly uses 7-bit address 0x3C; shifted left with the write bit
   * it appears on the bus as 0x78.
   * SSD1306 常见 7 位地址是 0x3C，左移并附加写位后，总线上发送 0x78。
   */
  I2C_Start();
  I2C_WriteByte(0x78u);
  I2C_WriteByte(control);
  I2C_WriteByte(data);
  I2C_Stop();
}

static void OLED_Cmd(uint8_t cmd)
{
  OLED_Write(0x00u, cmd);
}

static void OLED_Data(uint8_t data)
{
  OLED_Write(0x40u, data);
}

static APP_MAYBE_UNUSED void OLED_Init_Custom(void)
{
  HAL_Delay(50u);
  OLED_Cmd(0xAEu);
  OLED_Cmd(0x20u);
  OLED_Cmd(0x02u);
  OLED_Cmd(0xB0u);
  OLED_Cmd(0xC8u);
  OLED_Cmd(0x00u);
  OLED_Cmd(0x10u);
  OLED_Cmd(0x40u);
  OLED_Cmd(0x81u);
  OLED_Cmd(0x7Fu);
  OLED_Cmd(0xA1u);
  OLED_Cmd(0xA6u);
  OLED_Cmd(0xA8u);
  OLED_Cmd(0x3Fu);
  OLED_Cmd(0xA4u);
  OLED_Cmd(0xD3u);
  OLED_Cmd(0x00u);
  OLED_Cmd(0xD5u);
  OLED_Cmd(0x80u);
  OLED_Cmd(0xD9u);
  OLED_Cmd(0xF1u);
  OLED_Cmd(0xDAu);
  OLED_Cmd(0x12u);
  OLED_Cmd(0xDBu);
  OLED_Cmd(0x40u);
  OLED_Cmd(0x8Du);
  OLED_Cmd(0x14u);
  OLED_Cmd(0xAFu);
}

static void OLED_Clear(void)
{
  for (uint8_t page = 0u; page < 8u; page++)
  {
    OLED_SetCursor(page, 0u);
    for (uint8_t col = 0u; col < 128u; col++)
    {
      OLED_Data(0x00u);
    }
  }
}

static void OLED_SetCursor(uint8_t page, uint8_t col)
{
  OLED_Cmd((uint8_t)(0xB0u | (page & 0x07u)));
  OLED_Cmd((uint8_t)(0x00u | (col & 0x0Fu)));
  OLED_Cmd((uint8_t)(0x10u | ((col >> 4) & 0x0Fu)));
}

static void Font5x7(char c, uint8_t out[5])
{
  memset(out, 0, 5u);
  if ((c >= 'a') && (c <= 'z'))
  {
    c = (char)(c - 'a' + 'A');
  }

  switch (c)
  {
    case '0': { const uint8_t v[5] = {0x3Eu,0x51u,0x49u,0x45u,0x3Eu}; memcpy(out,v,5u); break; }
    case '1': { const uint8_t v[5] = {0x00u,0x42u,0x7Fu,0x40u,0x00u}; memcpy(out,v,5u); break; }
    case '2': { const uint8_t v[5] = {0x42u,0x61u,0x51u,0x49u,0x46u}; memcpy(out,v,5u); break; }
    case '3': { const uint8_t v[5] = {0x21u,0x41u,0x45u,0x4Bu,0x31u}; memcpy(out,v,5u); break; }
    case '4': { const uint8_t v[5] = {0x18u,0x14u,0x12u,0x7Fu,0x10u}; memcpy(out,v,5u); break; }
    case '5': { const uint8_t v[5] = {0x27u,0x45u,0x45u,0x45u,0x39u}; memcpy(out,v,5u); break; }
    case '6': { const uint8_t v[5] = {0x3Cu,0x4Au,0x49u,0x49u,0x30u}; memcpy(out,v,5u); break; }
    case '7': { const uint8_t v[5] = {0x01u,0x71u,0x09u,0x05u,0x03u}; memcpy(out,v,5u); break; }
    case '8': { const uint8_t v[5] = {0x36u,0x49u,0x49u,0x49u,0x36u}; memcpy(out,v,5u); break; }
    case '9': { const uint8_t v[5] = {0x06u,0x49u,0x49u,0x29u,0x1Eu}; memcpy(out,v,5u); break; }
    case 'A': { const uint8_t v[5] = {0x7Eu,0x11u,0x11u,0x11u,0x7Eu}; memcpy(out,v,5u); break; }
    case 'B': { const uint8_t v[5] = {0x7Fu,0x49u,0x49u,0x49u,0x36u}; memcpy(out,v,5u); break; }
    case 'C': { const uint8_t v[5] = {0x3Eu,0x41u,0x41u,0x41u,0x22u}; memcpy(out,v,5u); break; }
    case 'D': { const uint8_t v[5] = {0x7Fu,0x41u,0x41u,0x22u,0x1Cu}; memcpy(out,v,5u); break; }
    case 'E': { const uint8_t v[5] = {0x7Fu,0x49u,0x49u,0x49u,0x41u}; memcpy(out,v,5u); break; }
    case 'F': { const uint8_t v[5] = {0x7Fu,0x09u,0x09u,0x09u,0x01u}; memcpy(out,v,5u); break; }
    case 'G': { const uint8_t v[5] = {0x3Eu,0x41u,0x49u,0x49u,0x7Au}; memcpy(out,v,5u); break; }
    case 'H': { const uint8_t v[5] = {0x7Fu,0x08u,0x08u,0x08u,0x7Fu}; memcpy(out,v,5u); break; }
    case 'I': { const uint8_t v[5] = {0x00u,0x41u,0x7Fu,0x41u,0x00u}; memcpy(out,v,5u); break; }
    case 'J': { const uint8_t v[5] = {0x20u,0x40u,0x41u,0x3Fu,0x01u}; memcpy(out,v,5u); break; }
    case 'K': { const uint8_t v[5] = {0x7Fu,0x08u,0x14u,0x22u,0x41u}; memcpy(out,v,5u); break; }
    case 'L': { const uint8_t v[5] = {0x7Fu,0x40u,0x40u,0x40u,0x40u}; memcpy(out,v,5u); break; }
    case 'M': { const uint8_t v[5] = {0x7Fu,0x02u,0x0Cu,0x02u,0x7Fu}; memcpy(out,v,5u); break; }
    case 'N': { const uint8_t v[5] = {0x7Fu,0x04u,0x08u,0x10u,0x7Fu}; memcpy(out,v,5u); break; }
    case 'O': { const uint8_t v[5] = {0x3Eu,0x41u,0x41u,0x41u,0x3Eu}; memcpy(out,v,5u); break; }
    case 'P': { const uint8_t v[5] = {0x7Fu,0x09u,0x09u,0x09u,0x06u}; memcpy(out,v,5u); break; }
    case 'Q': { const uint8_t v[5] = {0x3Eu,0x41u,0x51u,0x21u,0x5Eu}; memcpy(out,v,5u); break; }
    case 'R': { const uint8_t v[5] = {0x7Fu,0x09u,0x19u,0x29u,0x46u}; memcpy(out,v,5u); break; }
    case 'S': { const uint8_t v[5] = {0x46u,0x49u,0x49u,0x49u,0x31u}; memcpy(out,v,5u); break; }
    case 'T': { const uint8_t v[5] = {0x01u,0x01u,0x7Fu,0x01u,0x01u}; memcpy(out,v,5u); break; }
    case 'U': { const uint8_t v[5] = {0x3Fu,0x40u,0x40u,0x40u,0x3Fu}; memcpy(out,v,5u); break; }
    case 'V': { const uint8_t v[5] = {0x1Fu,0x20u,0x40u,0x20u,0x1Fu}; memcpy(out,v,5u); break; }
    case 'W': { const uint8_t v[5] = {0x3Fu,0x40u,0x38u,0x40u,0x3Fu}; memcpy(out,v,5u); break; }
    case 'X': { const uint8_t v[5] = {0x63u,0x14u,0x08u,0x14u,0x63u}; memcpy(out,v,5u); break; }
    case 'Y': { const uint8_t v[5] = {0x07u,0x08u,0x70u,0x08u,0x07u}; memcpy(out,v,5u); break; }
    case 'Z': { const uint8_t v[5] = {0x61u,0x51u,0x49u,0x45u,0x43u}; memcpy(out,v,5u); break; }
    case ':': { const uint8_t v[5] = {0x00u,0x36u,0x36u,0x00u,0x00u}; memcpy(out,v,5u); break; }
    case '%': { const uint8_t v[5] = {0x23u,0x13u,0x08u,0x64u,0x62u}; memcpy(out,v,5u); break; }
    case '/': { const uint8_t v[5] = {0x20u,0x10u,0x08u,0x04u,0x02u}; memcpy(out,v,5u); break; }
    case '-': { const uint8_t v[5] = {0x08u,0x08u,0x08u,0x08u,0x08u}; memcpy(out,v,5u); break; }
    case '.': { const uint8_t v[5] = {0x00u,0x60u,0x60u,0x00u,0x00u}; memcpy(out,v,5u); break; }
    case ' ': default: break;
  }
}

static void OLED_PutChar(char c)
{
  uint8_t font[5];
  Font5x7(c, font);
  for (uint8_t i = 0u; i < 5u; i++)
  {
    OLED_Data(font[i]);
  }
  OLED_Data(0x00u);
}

static void OLED_Puts(const char *text)
{
  while (*text != '\0')
  {
    OLED_PutChar(*text++);
  }
}

static void OLED_PrintLine(uint8_t page, const char *text)
{
  OLED_SetCursor(page, 0u);
  OLED_Puts(text);
}

static void Flash_CS(uint8_t high)
{
  HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static uint8_t SPI2_TxRx(uint8_t data)
{
  while ((SPI2->SR & SPI_SR_TXE) == 0u)
  {
  }
  *(__IO uint8_t *)&SPI2->DR = data;
  while ((SPI2->SR & SPI_SR_RXNE) == 0u)
  {
  }
  return (uint8_t)SPI2->DR;
}

static uint8_t Flash_ReadStatus(void)
{
  uint8_t status;
  Flash_CS(0u);
  SPI2_TxRx(0x05u);
  status = SPI2_TxRx(0xFFu);
  Flash_CS(1u);
  return status;
}

static uint8_t Flash_WaitReady(uint32_t timeout_ms)
{
  const uint32_t start = HAL_GetTick();
  while ((Flash_ReadStatus() & 0x01u) != 0u)
  {
    if ((uint32_t)(HAL_GetTick() - start) > timeout_ms)
    {
      return 0u;
    }
  }
  return 1u;
}

static void Flash_WriteEnable(void)
{
  Flash_CS(0u);
  SPI2_TxRx(0x06u);
  Flash_CS(1u);
}

static void Flash_SectorErase(uint32_t addr)
{
  Flash_WriteEnable();
  Flash_CS(0u);
  SPI2_TxRx(0x20u);
  SPI2_TxRx((uint8_t)(addr >> 16));
  SPI2_TxRx((uint8_t)(addr >> 8));
  SPI2_TxRx((uint8_t)addr);
  Flash_CS(1u);
  (void)Flash_WaitReady(1000u);
}

static void Flash_PageProgram(uint32_t addr, const uint8_t *data, uint8_t len)
{
  Flash_WriteEnable();
  Flash_CS(0u);
  SPI2_TxRx(0x02u);
  SPI2_TxRx((uint8_t)(addr >> 16));
  SPI2_TxRx((uint8_t)(addr >> 8));
  SPI2_TxRx((uint8_t)addr);
  for (uint8_t i = 0u; i < len; i++)
  {
    SPI2_TxRx(data[i]);
  }
  Flash_CS(1u);
  (void)Flash_WaitReady(10u);
}

static APP_MAYBE_UNUSED void Flash_Init_Custom(void)
{
  GPIO_InitTypeDef gpio = {0};
  uint8_t manufacturer;
  uint8_t memory_type;
  uint8_t capacity;

  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_SPI2_CLK_ENABLE();

  gpio.Pin = GPIO_PIN_13 | GPIO_PIN_15;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &gpio);

  gpio.Pin = GPIO_PIN_14;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &gpio);

  gpio.Pin = FLASH_CS_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(FLASH_CS_PORT, &gpio);
  Flash_CS(1u);

  /* SPI2 is configured directly because the project keeps the generated HAL
   * footprint small.  BR[2:0]=011 gives PCLK1/16, about 2.25 MHz at 36 MHz.
   * SPI2 直接配置寄存器，避免引入更多 HAL 生成代码；BR[2:0]=011 表示
   * PCLK1/16，在 36 MHz 下约为 2.25 MHz。
   */
  SPI2->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_BR_1 | SPI_CR1_BR_0;
  SPI2->CR1 |= SPI_CR1_SPE;

  /* JEDEC ID check keeps an unpopulated W25Q64 header from being mistaken for
   * a real flash chip when MISO is floating.
   * 读取 JEDEC ID 用于确认 Flash 是否真的存在，避免 MISO 悬空时误判为
   * 已接入 W25Q64。
   */
  Flash_CS(0u);
  SPI2_TxRx(0x9Fu);
  manufacturer = SPI2_TxRx(0xFFu);
  memory_type = SPI2_TxRx(0xFFu);
  capacity = SPI2_TxRx(0xFFu);
  Flash_CS(1u);

  g_flash_present = (((manufacturer == 0xEFu) || (manufacturer == 0xC8u)) &&
                     ((memory_type == 0x40u) || (memory_type == 0x60u)) &&
                     (capacity == 0x17u)) ? 1u : 0u;
  g_flash_log_addr = 0u;

  if (g_flash_present)
  {
    /* This demo log uses sector 0 as a rolling scratch area.
     * 演示日志使用第 0 扇区作为循环记录区。
     */
    Flash_SectorErase(0u);
    printf("[MONITOR] W25Q ID %02X %02X %02X, log sector erased\r\n",
           manufacturer, memory_type, capacity);
  }
}

static void Flash_LogFrame(const SensorFrame *frame, uint8_t state)
{
  uint8_t record[20];
  uint8_t sum = 0u;

  if (!g_flash_present)
  {
    return;
  }

  record[0] = 0xE1u;
  record[1] = 0x03u;
  record[2] = frame->seq;
  record[3] = frame->status;
  record[4] = frame->temp;
  record[5] = frame->humi;
  record[6] = (uint8_t)(frame->mq135_adc >> 8);
  record[7] = (uint8_t)frame->mq135_adc;
  record[8] = (uint8_t)(frame->mq2_adc >> 8);
  record[9] = (uint8_t)frame->mq2_adc;
  record[10] = frame->flame;
  record[11] = state;
  record[12] = (uint8_t)(HAL_GetTick() >> 24);
  record[13] = (uint8_t)(HAL_GetTick() >> 16);
  record[14] = (uint8_t)(HAL_GetTick() >> 8);
  record[15] = (uint8_t)HAL_GetTick();
  record[16] = g_threshold_profile;
  record[17] = 0u;
  record[18] = 0u;

  /* The record checksum covers the first 19 bytes and helps spot incomplete
   * or corrupted flash entries during later analysis.
   * 记录校验和覆盖前 19 字节，便于后续分析时发现未写完整或损坏的 Flash 记录。
   */
  for (uint8_t i = 0u; i < 19u; i++)
  {
    sum = (uint8_t)(sum + record[i]);
  }
  record[19] = sum;

  if ((g_flash_log_addr + sizeof(record)) > 4096u)
  {
    g_flash_log_addr = 0u;
    Flash_SectorErase(0u);
  }

  Flash_PageProgram(g_flash_log_addr, record, (uint8_t)sizeof(record));
  printf("[MONITOR] flash log addr=0x%04lX seq=%u state=%u\r\n",
         (unsigned long)g_flash_log_addr, frame->seq, state);
  g_flash_log_addr += sizeof(record);
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /* 8 MHz HSE x 9 = 72 MHz SYSCLK.  APB1 is divided by 2, which is why USART3
   * and SPI2 calculations use a 36 MHz peripheral clock.
   * 8 MHz 外部晶振经 PLL x9 得到 72 MHz 系统时钟；APB1 二分频，因此
   * USART3 和 SPI2 的外设时钟按 36 MHz 计算。
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif
