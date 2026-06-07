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

/* Sensor-to-monitor frame v2:
 * AA 55 | LEN | VER TEMP HUMI MQ135 MQ2 RAIN THERM THERM_C10 FLAGS SEQ STATUS | SUM
 * The checksum is the low 8 bits of LEN plus all payload bytes.
 * 采集节点到显示节点的数据帧 v2：帧头为 AA 55，最后 1 字节为长度和负载
 * 累加后的低 8 位校验和，用于过滤串口噪声或错位数据。
 */
#define FRAME_HEAD0       0xAAu
#define FRAME_HEAD1       0x55u
#define FRAME_VERSION     2u
#define FRAME_PAYLOAD_LEN 18u
#define FRAME_TOTAL_LEN   (2u + 1u + FRAME_PAYLOAD_LEN + 1u)
#define NODE_RX_BUF_SIZE  64u

#define STATUS_DHT_ERROR      0x01u
#define STATUS_THERM_HOT_DO   0x02u
#define STATUS_RAIN_WET       0x04u
#define STATUS_THERM_ADC_ERR  0x08u

/* Timing values are expressed in milliseconds and compared with HAL_GetTick().
 * The code avoids long blocking delays in the monitor so serial reception,
 * buttons, display refresh, alarm output, and logging can cooperate.
 * 所有周期参数都用毫秒表示，并与 HAL_GetTick() 比较；显示节点尽量避免长时间
 * 阻塞，使串口接收、按键、刷新、报警和日志记录能在主循环中协作运行。
 */
#define USART_BAUDRATE    115200u
#define SENSOR_PERIOD_MS  1000u
#define DHT11_PERIOD_MS   2100u
#define UI_PERIOD_MS      500u
#define ALARM_PERIOD_MS   100u
#define NODE_TIMEOUT_MS   5000u
#define MUTE_TIME_MS      60000u
#define FLASH_LOG_PERIOD_MS 10000u

#define OLED_WIDTH_PIXELS 128u
#define OLED_FONT_WIDTH   6u

#define RAIN_WET_ADC_DEFAULT       1400u
#define THERM_WARN_C10_DEFAULT     450
#define THERM_DANGER_C10_DEFAULT   700

#define FLASH_SIZE_BYTES       0x800000u
#define FLASH_SECTOR_SIZE      4096u
#define FLASH_META_ENTRY_SIZE  16u
#define FLASH_META_MAGIC0      0x4Du
#define FLASH_META_MAGIC1      0x32u
#define FLASH_RECORD_MAGIC     0xE2u
#define FLASH_LOG_START_ADDR   FLASH_SECTOR_SIZE
#define FLASH_LOG_RECORD_SIZE  32u
#define FLASH_LOG_END_ADDR     FLASH_SIZE_BYTES

#define WS2813_LED_COUNT       1u
#define WS2813_BITS_PER_LED    24u
#define WS2813_RESET_SLOTS     48u
#define WS2813_BUFFER_LEN      ((WS2813_LED_COUNT * WS2813_BITS_PER_LED) + WS2813_RESET_SLOTS)
#define WS2813_TIMER_PERIOD    89u
#define WS2813_CODE0_CCR       26u
#define WS2813_CODE1_CCR       52u

/* Sensor-node pins.  PA9/PA10 are intentionally not used here because this
 * board already routes them to the CH340C USB-to-UART bridge for USART1 debug.
 * 采集节点引脚：PA9/PA10 默认连接板载 CH340C，所以保留给 USART1 调试串口，
 * 不再分配给外部模块。
 */
#define DHT11_PORT        GPIOB
#define DHT11_PIN         GPIO_PIN_12
#define FLAME_PORT        GPIOB
#define FLAME_PIN         GPIO_PIN_13
#define THERM_DO_PORT     GPIOB
#define THERM_DO_PIN      GPIO_PIN_9

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
#define EXT_RGB_PORT      GPIOA
#define EXT_RGB_PIN       GPIO_PIN_6

typedef struct
{
  uint8_t temp;
  uint8_t humi;
  uint16_t mq135_adc;
  uint16_t mq2_adc;
  uint16_t rain_adc;
  uint16_t therm_adc;
  int16_t therm_c10;
  uint8_t flame;
  uint8_t rain_wet;
  uint8_t therm_hot;
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
  uint16_t rain_wet;
  int16_t therm_warn_c10;
  int16_t therm_danger_c10;
} AlarmThresholds;

static const AlarmThresholds k_threshold_profiles[] =
{
  {2200u, 1800u, 2800u, RAIN_WET_ADC_DEFAULT, THERM_WARN_C10_DEFAULT, THERM_DANGER_C10_DEFAULT},
  {1800u, 1400u, 2400u, 1200u, 400, 650},
  {2600u, 2200u, 3300u, 1800u, 500, 750},
};

typedef struct
{
  uint16_t adc;
  int16_t c10;
} NtcTablePoint;

static const NtcTablePoint k_ntc_table[] =
{
  {128u, 1293},
  {384u, 866},
  {640u, 685},
  {896u, 567},
  {1152u, 477},
  {1408u, 403},
  {1664u, 338},
  {1920u, 278},
  {2176u, 222},
  {2432u, 167},
  {2688u, 111},
  {2944u, 53},
  {3200u, -11},
  {3456u, -87},
  {3712u, -186},
  {3968u, -364},
};

static SensorFrame g_latest_frame;
static uint32_t g_last_rx_ms = 0u;
static uint8_t g_have_rx = 0u;
static uint8_t g_page = 0u;
static uint8_t g_threshold_profile = 0u;
static uint32_t g_mute_until_ms = 0u;
static uint8_t g_flash_present = 0u;
static uint32_t g_flash_log_addr = 0u;
static uint32_t g_flash_record_count = 0u;
static uint32_t g_flash_meta_addr = 0u;
static uint8_t g_ext_rgb_ready = 0u;
static uint8_t g_ext_rgb_last_r = 0xFFu;
static uint8_t g_ext_rgb_last_g = 0xFFu;
static uint8_t g_ext_rgb_last_b = 0xFFu;
static volatile uint8_t g_node_rx_buf[NODE_RX_BUF_SIZE];
static volatile uint8_t g_node_rx_head = 0u;
static volatile uint8_t g_node_rx_tail = 0u;
static uint16_t g_ws2813_dma_buf[WS2813_BUFFER_LEN];

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
static uint16_t Sensor_Filter(uint16_t previous, uint16_t sample, uint8_t valid);
static int16_t Thermistor_AdcToC10(uint16_t adc, uint8_t *valid);
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
static uint8_t Monitor_LinkWaiting(void);
static uint8_t Monitor_NodeLost(void);
static uint8_t Monitor_Danger(void);
static uint8_t Monitor_Warn(void);
static uint8_t Monitor_Muted(void);
static const char *Monitor_AlarmState(void);
static void Monitor_PrintFrontendJson(const SensorFrame *frame);
static void LED_Set(uint8_t red, uint8_t green, uint8_t blue);
static void Buzzer_Set(uint8_t on);
static void WS2813_Init_Custom(void);
static void WS2813_SetColor(uint8_t red, uint8_t green, uint8_t blue);
static void WS2813_FillBuffer(uint8_t red, uint8_t green, uint8_t blue);
static void WS2813_SendBuffer(uint16_t len);
static void OLED_Init_Custom(void);
static APP_MAYBE_UNUSED void OLED_Clear(void);
static void OLED_SetCursor(uint8_t page, uint8_t col);
static void OLED_PrintLine(uint8_t page, const char *text);
static void Flash_Init_Custom(void);
static void Flash_LogFrame(const SensorFrame *frame, uint8_t state);
static void Flash_ReadData(uint32_t addr, uint8_t *data, uint16_t len);
static void Flash_WriteMetadata(void);
static uint8_t Flash_LoadMetadata(void);
static uint16_t Flash_Crc16(const uint8_t *data, uint8_t len);
static void Flash_U32ToBytes(uint8_t *out, uint32_t value);
static uint32_t Flash_BytesToU32(const uint8_t *in);

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
  uint32_t last_dht_ms = 0u - DHT11_PERIOD_MS;
  uint8_t seq = 0u;
  uint8_t temp = 0u;
  uint8_t humi = 0u;
  uint8_t dht_ok = 0u;
  uint8_t avg_valid = 0u;
  uint16_t mq135_avg = 0u;
  uint16_t mq2_avg = 0u;
  uint16_t rain_avg = 0u;
  uint16_t therm_avg = 0u;

  while (1)
  {
    const uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - last_sensor_ms) >= SENSOR_PERIOD_MS)
    {
      SensorFrame frame;
      const uint16_t mq135_raw = ADC1_ReadChannel(4u);
      const uint16_t mq2_raw = ADC1_ReadChannel(5u);
      const uint16_t rain_raw = ADC1_ReadChannel(6u);
      const uint16_t therm_raw = ADC1_ReadChannel(7u);
      uint8_t therm_adc_ok = 0u;
      const uint8_t therm_do_hot =
        (HAL_GPIO_ReadPin(THERM_DO_PORT, THERM_DO_PIN) == GPIO_PIN_RESET) ? 1u : 0u;

      /* DHT11 requires more than 2 seconds between conversions.  Frames still
       * go out once per second; skipped frames reuse the last valid reading.
       * DHT11 每次采集间隔需大于 2 秒；串口帧仍保持每秒发送，未到刷新
       * 间隔时沿用上一次温湿度值。
       */
      if ((uint32_t)(now - last_dht_ms) >= DHT11_PERIOD_MS)
      {
        dht_ok = DHT11_Read(&temp, &humi);
        last_dht_ms = now;
      }

      /* A lightweight exponential moving average smooths noisy MQ sensor ADC
       * values without storing a full sample window.
       * 使用轻量级指数滑动平均来平滑 MQ 传感器 ADC 噪声，不需要保存完整
       * 采样窗口，适合资源有限的单片机。
       */
      if (!avg_valid)
      {
        mq135_avg = mq135_raw;
        mq2_avg = mq2_raw;
        rain_avg = rain_raw;
        therm_avg = therm_raw;
        avg_valid = 1u;
      }
      else
      {
        mq135_avg = Sensor_Filter(mq135_avg, mq135_raw, avg_valid);
        mq2_avg = Sensor_Filter(mq2_avg, mq2_raw, avg_valid);
        rain_avg = Sensor_Filter(rain_avg, rain_raw, avg_valid);
        therm_avg = Sensor_Filter(therm_avg, therm_raw, avg_valid);
      }

      frame.temp = temp;
      frame.humi = humi;
      frame.mq135_adc = mq135_avg;
      frame.mq2_adc = mq2_avg;
      frame.rain_adc = rain_avg;
      frame.therm_adc = therm_avg;
      frame.therm_c10 = Thermistor_AdcToC10(therm_avg, &therm_adc_ok);
      /* The flame module used by this project is treated as active-low.
       * 本项目按低电平有效处理火焰模块：读到 RESET 表示触发。
       */
      frame.flame = (HAL_GPIO_ReadPin(FLAME_PORT, FLAME_PIN) == GPIO_PIN_RESET) ? 1u : 0u;
      frame.rain_wet = (rain_avg >= RAIN_WET_ADC_DEFAULT) ? 1u : 0u;
      frame.therm_hot = therm_do_hot;
      frame.seq = seq++;
      frame.status = dht_ok ? 0u : STATUS_DHT_ERROR;
      frame.status |= therm_do_hot ? STATUS_THERM_HOT_DO : 0u;
      frame.status |= frame.rain_wet ? STATUS_RAIN_WET : 0u;
      frame.status |= therm_adc_ok ? 0u : STATUS_THERM_ADC_ERR;

      Sensor_SendFrame(&frame);
      printf("[SENSOR] seq=%u t=%u h=%u mq135=%u mq2=%u rain=%u therm=%d.%dC flame=%u status=0x%02X\r\n",
             frame.seq, frame.temp, frame.humi, frame.mq135_adc, frame.mq2_adc,
             frame.rain_adc, frame.therm_c10 / 10, frame.therm_c10 < 0 ? -(frame.therm_c10 % 10) : (frame.therm_c10 % 10),
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
  uint8_t last_logged_state = 0xFFu;

  /* Give the sensor link a startup grace period before reporting NODE LOST.
   * The OLED shows WAIT SENSOR until the first valid frame arrives or the
   * timeout expires, which avoids a misleading lost-node warning at boot.
   * 上电后先给传感器链路一个启动宽限期；收到第一帧合法数据或超时前，
   * OLED 显示 WAIT SENSOR，避免刚启动就误导为节点丢失。
   */
  g_have_rx = 0u;
  g_last_rx_ms = HAL_GetTick();

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

    if ((g_flash_present != 0u) && (g_have_rx != 0u) && !Monitor_NodeLost())
    {
      const uint8_t state = Monitor_Danger() ? 2u : (Monitor_Warn() ? 1u : 0u);
      if ((state != last_logged_state) || ((uint32_t)(now - last_log_ms) >= FLASH_LOG_PERIOD_MS))
      {
        Flash_LogFrame(&g_latest_frame, state);
        last_log_ms = now;
        last_logged_state = state;
      }
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

  __HAL_RCC_GPIOA_CLK_ENABLE();
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

  /* MQ135/MQ2 plus the rain and thermistor modules are all analog inputs.
   * Some single-module demo wiring places rain/thermistor on PA4/PA5 too, but
   * this reference design already uses those pins for MQ sensors, so PA6/PA7
   * are used here.
   * MQ135/MQ2 以及雨量、热敏 AO 都是模拟输入。部分单模块演示接线也会把
   * 雨量/热敏放在 PA4/PA5，但本参考设计已把这两个脚给 MQ 模块，因此新增
   * 两路使用 PA6/PA7。
   */
  gpio.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
  gpio.Mode = GPIO_MODE_ANALOG;
  HAL_GPIO_Init(GPIOA, &gpio);

  gpio.Pin = FLAME_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(FLAME_PORT, &gpio);

  gpio.Pin = THERM_DO_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(THERM_DO_PORT, &gpio);

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
  ADC1->SMPR2 |= ADC_SMPR2_SMP4 | ADC_SMPR2_SMP5 | ADC_SMPR2_SMP6 | ADC_SMPR2_SMP7;
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

static uint16_t Sensor_Filter(uint16_t previous, uint16_t sample, uint8_t valid)
{
  if (valid == 0u)
  {
    return sample;
  }
  return (uint16_t)(((uint32_t)previous * 3u + sample) / 4u);
}

static int16_t Thermistor_AdcToC10(uint16_t adc, uint8_t *valid)
{
  const uint8_t table_count = (uint8_t)(sizeof(k_ntc_table) / sizeof(k_ntc_table[0]));

  if ((adc <= 8u) || (adc >= 4088u))
  {
    *valid = 0u;
    return 0;
  }

  *valid = 1u;
  if (adc <= k_ntc_table[0].adc)
  {
    return k_ntc_table[0].c10;
  }
  if (adc >= k_ntc_table[table_count - 1u].adc)
  {
    return k_ntc_table[table_count - 1u].c10;
  }

  for (uint8_t i = 0u; i < (uint8_t)(table_count - 1u); i++)
  {
    const NtcTablePoint *lo = &k_ntc_table[i];
    const NtcTablePoint *hi = &k_ntc_table[i + 1u];
    if ((adc >= lo->adc) && (adc <= hi->adc))
    {
      const int32_t adc_span = (int32_t)hi->adc - (int32_t)lo->adc;
      const int32_t temp_span = (int32_t)hi->c10 - (int32_t)lo->c10;
      const int32_t offset = (int32_t)adc - (int32_t)lo->adc;
      return (int16_t)((int32_t)lo->c10 + ((temp_span * offset) / adc_span));
    }
  }

  return 0;
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
  out[3] = FRAME_VERSION;
  out[4] = frame->temp;
  out[5] = frame->humi;
  out[6] = (uint8_t)(frame->mq135_adc >> 8);
  out[7] = (uint8_t)(frame->mq135_adc & 0xFFu);
  out[8] = (uint8_t)(frame->mq2_adc >> 8);
  out[9] = (uint8_t)(frame->mq2_adc & 0xFFu);
  out[10] = (uint8_t)(frame->rain_adc >> 8);
  out[11] = (uint8_t)(frame->rain_adc & 0xFFu);
  out[12] = (uint8_t)(frame->therm_adc >> 8);
  out[13] = (uint8_t)(frame->therm_adc & 0xFFu);
  out[14] = (uint8_t)((uint16_t)frame->therm_c10 >> 8);
  out[15] = (uint8_t)((uint16_t)frame->therm_c10 & 0xFFu);
  out[16] = frame->flame;
  out[17] = frame->rain_wet;
  out[18] = frame->therm_hot;
  out[19] = frame->seq;
  out[20] = frame->status;
  out[21] = Frame_Checksum(&out[2], (uint8_t)(1u + FRAME_PAYLOAD_LEN));
  return FRAME_TOTAL_LEN;
}

static uint8_t Frame_Decode(const uint8_t in[FRAME_TOTAL_LEN], SensorFrame *frame)
{
  /* Header, fixed length, and checksum are checked before updating the latest
   * monitor data.  Bad frames are ignored so noise cannot corrupt the display.
   * 更新显示数据前先检查帧头、固定长度和校验和；错误帧会被丢弃，避免串口
   * 噪声污染 OLED 数据。
   */
  if ((in[0] != FRAME_HEAD0) || (in[1] != FRAME_HEAD1) ||
      (in[2] != FRAME_PAYLOAD_LEN) || (in[3] != FRAME_VERSION))
  {
    return 0u;
  }
  if (Frame_Checksum(&in[2], (uint8_t)(1u + FRAME_PAYLOAD_LEN)) != in[21])
  {
    return 0u;
  }

  frame->temp = in[4];
  frame->humi = in[5];
  frame->mq135_adc = (uint16_t)(((uint16_t)in[6] << 8) | in[7]);
  frame->mq2_adc = (uint16_t)(((uint16_t)in[8] << 8) | in[9]);
  frame->rain_adc = (uint16_t)(((uint16_t)in[10] << 8) | in[11]);
  frame->therm_adc = (uint16_t)(((uint16_t)in[12] << 8) | in[13]);
  frame->therm_c10 = (int16_t)(((uint16_t)in[14] << 8) | in[15]);
  frame->flame = in[16];
  frame->rain_wet = in[17];
  frame->therm_hot = in[18];
  frame->seq = in[19];
  frame->status = in[20];
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
  WS2813_Init_Custom();
  WS2813_SetColor(0u, 0u, 24u);
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
        g_have_rx = 1u;
        g_last_rx_ms = HAL_GetTick();
        printf("[MONITOR] rx v%u seq=%u t=%u h=%u mq135=%u mq2=%u rain=%u therm=%d.%dC flame=%u status=0x%02X\r\n",
               FRAME_VERSION,
               frame.seq, frame.temp, frame.humi, frame.mq135_adc, frame.mq2_adc,
               frame.rain_adc, frame.therm_c10 / 10,
               frame.therm_c10 < 0 ? -(frame.therm_c10 % 10) : (frame.therm_c10 % 10),
               frame.flame, frame.status);
        Monitor_PrintFrontendJson(&frame);
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

static uint8_t Monitor_LinkWaiting(void)
{
  return ((g_have_rx == 0u) && !Monitor_NodeLost()) ? 1u : 0u;
}

static uint8_t Monitor_NodeLost(void)
{
  return ((uint32_t)(HAL_GetTick() - g_last_rx_ms) > NODE_TIMEOUT_MS) ? 1u : 0u;
}

static uint8_t Monitor_Danger(void)
{
  const AlarmThresholds *th = &k_threshold_profiles[g_threshold_profile];
  if (Monitor_LinkWaiting() || Monitor_NodeLost())
  {
    return 0u;
  }
  return ((g_latest_frame.flame != 0u) ||
          (g_latest_frame.mq2_adc >= th->smoke_danger) ||
          (g_latest_frame.therm_hot != 0u) ||
          (((g_latest_frame.status & STATUS_THERM_ADC_ERR) == 0u) &&
           (g_latest_frame.therm_c10 >= th->therm_danger_c10))) ? 1u : 0u;
}

static uint8_t Monitor_Warn(void)
{
  const AlarmThresholds *th = &k_threshold_profiles[g_threshold_profile];
  if (Monitor_LinkWaiting())
  {
    return 0u;
  }
  if (Monitor_NodeLost())
  {
    return 1u;
  }
  return (((g_latest_frame.status & STATUS_DHT_ERROR) != 0u) ||
          (g_latest_frame.mq135_adc >= th->air_warn) ||
          (g_latest_frame.mq2_adc >= th->smoke_warn) ||
          (g_latest_frame.rain_wet != 0u) ||
          (g_latest_frame.rain_adc >= th->rain_wet) ||
          (((g_latest_frame.status & STATUS_THERM_ADC_ERR) == 0u) &&
           (g_latest_frame.therm_c10 >= th->therm_warn_c10))) ? 1u : 0u;
}

static uint8_t Monitor_Muted(void)
{
  const uint32_t now = HAL_GetTick();
  return ((int32_t)(g_mute_until_ms - now) > 0) ? 1u : 0u;
}

static const char *Monitor_AlarmState(void)
{
  if (Monitor_Danger())
  {
    return "danger";
  }
  if (Monitor_LinkWaiting())
  {
    return "waiting";
  }
  if (Monitor_NodeLost())
  {
    return "node_lost";
  }
  if (Monitor_Warn())
  {
    return "warn";
  }
  return "normal";
}

static void Monitor_PrintFrontendJson(const SensorFrame *frame)
{
  printf("{\"type\":\"sensor\",\"schemaVersion\":%u,\"seq\":%u,\"tickMs\":%lu,\"tempC\":%u,"
         "\"humidityPct\":%u,\"mq135Raw\":%u,\"mq2Raw\":%u,\"rainRaw\":%u,"
         "\"thermRaw\":%u,\"thermC10\":%d,\"rainWet\":%u,\"thermHot\":%u,\"flame\":%u,"
         "\"status\":%u,\"alarm\":\"%s\",\"thresholdProfile\":%u,"
         "\"mute\":%u,\"flashReady\":%u,\"flashRecords\":%lu,\"externalRgb\":%u}\n",
         (unsigned int)FRAME_VERSION,
         (unsigned int)frame->seq,
         (unsigned long)HAL_GetTick(),
         (unsigned int)frame->temp,
         (unsigned int)frame->humi,
         (unsigned int)frame->mq135_adc,
         (unsigned int)frame->mq2_adc,
         (unsigned int)frame->rain_adc,
         (unsigned int)frame->therm_adc,
         (int)frame->therm_c10,
         (unsigned int)frame->rain_wet,
         (unsigned int)frame->therm_hot,
         (unsigned int)frame->flame,
         (unsigned int)frame->status,
         Monitor_AlarmState(),
         (unsigned int)g_threshold_profile,
         (unsigned int)Monitor_Muted(),
         (unsigned int)g_flash_present,
         (unsigned long)g_flash_record_count,
         (unsigned int)g_ext_rgb_ready);
}

static void Monitor_UpdateAlarm(void)
{
  const uint32_t now = HAL_GetTick();
  const uint8_t muted = Monitor_Muted();

  /* Alarm priority is important: real danger beats node-lost, node-lost beats
   * normal warning, and muted only suppresses the buzzer, not the LED color.
   * 报警优先级很重要：真实危险高于节点丢失，节点丢失高于普通预警；静音
   * 只关闭蜂鸣器，不改变 LED 状态颜色。
   */
  if (Monitor_Danger())
  {
    LED_Set(1u, 0u, 0u);
    WS2813_SetColor(((now / 150u) % 2u == 0u) ? 96u : 18u, 0u, 0u);
    Buzzer_Set((muted == 0u) && ((now / 150u) % 2u == 0u));
  }
  else if (Monitor_LinkWaiting())
  {
    LED_Set(0u, 0u, 1u);
    WS2813_SetColor(0u, 0u, 28u);
    Buzzer_Set(0u);
  }
  else if (Monitor_NodeLost())
  {
    LED_Set(0u, 0u, 1u);
    WS2813_SetColor(0u, 0u, ((now / 700u) % 2u == 0u) ? 80u : 12u);
    Buzzer_Set((muted == 0u) && ((now / 700u) % 2u == 0u));
  }
  else if (Monitor_Warn())
  {
    LED_Set(1u, 1u, 0u);
    WS2813_SetColor(72u, 32u, 0u);
    Buzzer_Set(0u);
  }
  else
  {
    LED_Set(0u, 1u, 0u);
    WS2813_SetColor(0u, 54u, 0u);
    Buzzer_Set(0u);
  }
}

static void Monitor_UpdateDisplay(void)
{
  char line[24];
  const uint8_t waiting = Monitor_LinkWaiting();
  const uint8_t lost = Monitor_NodeLost();

  /* Each row is overwritten in place.  Avoiding a full-screen clear prevents
   * the OLED from visibly blinking during normal refreshes.
   * 每行直接原位覆盖；避免每次全屏清空，减少 OLED 正常刷新时的可见闪烁。
   */
  if (g_page == 0u)
  {
    OLED_PrintLine(0, waiting ? "WAIT SENSOR" :
                      (lost ? "NODE LOST" :
                       (Monitor_Danger() ? "STATE DANGER" :
                        (Monitor_Warn() ? "STATE WARN" : "STATE NORMAL"))));
    snprintf(line, sizeof(line), "T:%02uC H:%02u%%", g_latest_frame.temp, g_latest_frame.humi);
    OLED_PrintLine(1, line);
    snprintf(line, sizeof(line), "AIR:%04u MQ2:%04u", g_latest_frame.mq135_adc,
             g_latest_frame.mq2_adc);
    OLED_PrintLine(2, line);
    snprintf(line, sizeof(line), "R:%04u N:%d.%d", g_latest_frame.rain_adc,
             g_latest_frame.therm_c10 / 10,
             g_latest_frame.therm_c10 < 0 ? -(g_latest_frame.therm_c10 % 10) :
             (g_latest_frame.therm_c10 % 10));
    OLED_PrintLine(3, line);
  }
  else
  {
    const AlarmThresholds *th = &k_threshold_profiles[g_threshold_profile];
    snprintf(line, sizeof(line), "PROFILE:%u", g_threshold_profile);
    OLED_PrintLine(0, line);
    snprintf(line, sizeof(line), "AIR:%04u MQ2:%04u", th->air_warn, th->smoke_warn);
    OLED_PrintLine(1, line);
    snprintf(line, sizeof(line), "RAIN:%04u HOT:%d", th->rain_wet, th->therm_danger_c10 / 10);
    OLED_PrintLine(2, line);
    snprintf(line, sizeof(line), "SEQ:%03u F:%s L:%lu", g_latest_frame.seq,
             g_flash_present ? "OK" : "NO", (unsigned long)(g_flash_record_count % 1000u));
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

static void WS2813_Init_Custom(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_TIM3_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  gpio.Pin = EXT_RGB_PIN;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(EXT_RGB_PORT, &gpio);

  TIM3->PSC = 0u;
  TIM3->ARR = WS2813_TIMER_PERIOD;
  TIM3->CCR1 = 0u;
  TIM3->CCMR1 &= ~(TIM_CCMR1_OC1M | TIM_CCMR1_CC1S);
  TIM3->CCMR1 |= TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;
  TIM3->CCER |= TIM_CCER_CC1E;
  TIM3->DIER |= TIM_DIER_CC1DE;
  TIM3->CR1 |= TIM_CR1_ARPE;
  TIM3->EGR = TIM_EGR_UG;

  DMA1_Channel6->CCR = 0u;
  DMA1_Channel6->CPAR = (uint32_t)&TIM3->CCR1;
  DMA1->IFCR = DMA_IFCR_CGIF6 | DMA_IFCR_CTCIF6 | DMA_IFCR_CHTIF6 | DMA_IFCR_CTEIF6;
  g_ext_rgb_ready = 1u;
}

static void WS2813_SetColor(uint8_t red, uint8_t green, uint8_t blue)
{
  if (g_ext_rgb_ready == 0u)
  {
    return;
  }
  if ((red == g_ext_rgb_last_r) && (green == g_ext_rgb_last_g) && (blue == g_ext_rgb_last_b))
  {
    return;
  }

  g_ext_rgb_last_r = red;
  g_ext_rgb_last_g = green;
  g_ext_rgb_last_b = blue;
  WS2813_FillBuffer(red, green, blue);
  WS2813_SendBuffer(WS2813_BUFFER_LEN);
}

static void WS2813_FillBuffer(uint8_t red, uint8_t green, uint8_t blue)
{
  const uint8_t grb[3] = {green, red, blue};
  uint16_t pos = 0u;

  for (uint8_t led = 0u; led < WS2813_LED_COUNT; led++)
  {
    for (uint8_t color = 0u; color < 3u; color++)
    {
      uint8_t value = grb[color];
      for (uint8_t bit = 0u; bit < 8u; bit++)
      {
        g_ws2813_dma_buf[pos++] = (value & 0x80u) ? WS2813_CODE1_CCR : WS2813_CODE0_CCR;
        value <<= 1u;
      }
    }
  }

  while (pos < WS2813_BUFFER_LEN)
  {
    g_ws2813_dma_buf[pos++] = 0u;
  }
}

static void WS2813_SendBuffer(uint16_t len)
{
  const uint32_t start = HAL_GetTick();

  DMA1_Channel6->CCR &= ~DMA_CCR_EN;
  DMA1_Channel6->CMAR = (uint32_t)g_ws2813_dma_buf;
  DMA1_Channel6->CNDTR = len;
  DMA1->IFCR = DMA_IFCR_CGIF6 | DMA_IFCR_CTCIF6 | DMA_IFCR_CHTIF6 | DMA_IFCR_CTEIF6;
  DMA1_Channel6->CCR = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PSIZE_0 |
                       DMA_CCR_MSIZE_0 | DMA_CCR_PL_1 | DMA_CCR_EN;

  TIM3->CNT = 0u;
  TIM3->CR1 |= TIM_CR1_CEN;
  while ((DMA1->ISR & DMA_ISR_TCIF6) == 0u)
  {
    if ((uint32_t)(HAL_GetTick() - start) > 3u)
    {
      break;
    }
  }

  TIM3->CR1 &= ~TIM_CR1_CEN;
  DMA1_Channel6->CCR &= ~DMA_CCR_EN;
  DMA1->IFCR = DMA_IFCR_CGIF6 | DMA_IFCR_CTCIF6 | DMA_IFCR_CHTIF6 | DMA_IFCR_CTEIF6;
  TIM3->CCR1 = 0u;
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

static void OLED_DataFill(uint8_t data, uint8_t count)
{
  I2C_Start();
  I2C_WriteByte(0x78u);
  I2C_WriteByte(0x40u);
  for (uint8_t i = 0u; i < count; i++)
  {
    I2C_WriteByte(data);
  }
  I2C_Stop();
}

static void OLED_DataBuffer(const uint8_t *data, uint8_t len)
{
  I2C_Start();
  I2C_WriteByte(0x78u);
  I2C_WriteByte(0x40u);
  for (uint8_t i = 0u; i < len; i++)
  {
    I2C_WriteByte(data[i]);
  }
  I2C_Stop();
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

static APP_MAYBE_UNUSED void OLED_Clear(void)
{
  for (uint8_t page = 0u; page < 8u; page++)
  {
    OLED_SetCursor(page, 0u);
    OLED_DataFill(0x00u, OLED_WIDTH_PIXELS);
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

static void OLED_PrintLine(uint8_t page, const char *text)
{
  uint8_t pixels[OLED_WIDTH_PIXELS];
  uint8_t col = 0u;

  memset(pixels, 0, sizeof(pixels));
  while ((*text != '\0') && ((uint16_t)col + OLED_FONT_WIDTH <= OLED_WIDTH_PIXELS))
  {
    uint8_t font[5];
    Font5x7(*text++, font);
    for (uint8_t i = 0u; i < 5u; i++)
    {
      pixels[col++] = font[i];
    }
    pixels[col++] = 0x00u;
  }

  OLED_SetCursor(page, 0u);
  OLED_DataBuffer(pixels, OLED_WIDTH_PIXELS);
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

static void Flash_ReadData(uint32_t addr, uint8_t *data, uint16_t len)
{
  Flash_CS(0u);
  SPI2_TxRx(0x03u);
  SPI2_TxRx((uint8_t)(addr >> 16));
  SPI2_TxRx((uint8_t)(addr >> 8));
  SPI2_TxRx((uint8_t)addr);
  for (uint16_t i = 0u; i < len; i++)
  {
    data[i] = SPI2_TxRx(0xFFu);
  }
  Flash_CS(1u);
}

static uint16_t Flash_Crc16(const uint8_t *data, uint8_t len)
{
  uint16_t crc = 0xFFFFu;

  for (uint8_t i = 0u; i < len; i++)
  {
    crc ^= data[i];
    for (uint8_t bit = 0u; bit < 8u; bit++)
    {
      crc = (crc & 0x0001u) ? (uint16_t)((crc >> 1) ^ 0xA001u) : (uint16_t)(crc >> 1);
    }
  }

  return crc;
}

static void Flash_U32ToBytes(uint8_t *out, uint32_t value)
{
  out[0] = (uint8_t)(value >> 24);
  out[1] = (uint8_t)(value >> 16);
  out[2] = (uint8_t)(value >> 8);
  out[3] = (uint8_t)value;
}

static uint32_t Flash_BytesToU32(const uint8_t *in)
{
  return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) |
         ((uint32_t)in[2] << 8) | (uint32_t)in[3];
}

static uint8_t Flash_LoadMetadata(void)
{
  uint8_t entry[FLASH_META_ENTRY_SIZE];
  uint8_t found = 0u;
  uint32_t next_meta = 0u;

  for (uint32_t addr = 0u; addr < FLASH_SECTOR_SIZE; addr += FLASH_META_ENTRY_SIZE)
  {
    uint8_t blank = 1u;
    Flash_ReadData(addr, entry, (uint16_t)sizeof(entry));
    for (uint8_t i = 0u; i < sizeof(entry); i++)
    {
      if (entry[i] != 0xFFu)
      {
        blank = 0u;
        break;
      }
    }

    if (blank != 0u)
    {
      next_meta = addr;
      break;
    }

    if ((entry[0] == FLASH_META_MAGIC0) && (entry[1] == FLASH_META_MAGIC1) &&
        (entry[2] == FRAME_VERSION))
    {
      const uint16_t stored_crc = (uint16_t)(((uint16_t)entry[12] << 8) | entry[13]);
      const uint16_t calc_crc = Flash_Crc16(entry, 12u);
      const uint32_t log_addr = Flash_BytesToU32(&entry[4]);
      const uint32_t count = Flash_BytesToU32(&entry[8]);

      if ((stored_crc == calc_crc) &&
          (log_addr >= FLASH_LOG_START_ADDR) &&
          (log_addr < FLASH_LOG_END_ADDR) &&
          (((log_addr - FLASH_LOG_START_ADDR) % FLASH_LOG_RECORD_SIZE) == 0u))
      {
        g_flash_log_addr = log_addr;
        g_flash_record_count = count;
        found = 1u;
      }
    }

    next_meta = addr + FLASH_META_ENTRY_SIZE;
  }

  if (next_meta >= FLASH_SECTOR_SIZE)
  {
    next_meta = FLASH_SECTOR_SIZE;
  }
  g_flash_meta_addr = next_meta;

  return found;
}

static void Flash_WriteMetadata(void)
{
  uint8_t entry[FLASH_META_ENTRY_SIZE];
  uint16_t crc;

  if (g_flash_meta_addr + FLASH_META_ENTRY_SIZE > FLASH_SECTOR_SIZE)
  {
    Flash_SectorErase(0u);
    g_flash_meta_addr = 0u;
  }

  memset(entry, 0xFF, sizeof(entry));
  entry[0] = FLASH_META_MAGIC0;
  entry[1] = FLASH_META_MAGIC1;
  entry[2] = FRAME_VERSION;
  entry[3] = 0u;
  Flash_U32ToBytes(&entry[4], g_flash_log_addr);
  Flash_U32ToBytes(&entry[8], g_flash_record_count);
  crc = Flash_Crc16(entry, 12u);
  entry[12] = (uint8_t)(crc >> 8);
  entry[13] = (uint8_t)crc;

  Flash_PageProgram(g_flash_meta_addr, entry, (uint8_t)sizeof(entry));
  g_flash_meta_addr += FLASH_META_ENTRY_SIZE;
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
  g_flash_log_addr = FLASH_LOG_START_ADDR;
  g_flash_record_count = 0u;
  g_flash_meta_addr = 0u;

  if (g_flash_present)
  {
    if (Flash_LoadMetadata() == 0u)
    {
      g_flash_log_addr = FLASH_LOG_START_ADDR;
      g_flash_record_count = 0u;
      g_flash_meta_addr = FLASH_SECTOR_SIZE;
      Flash_WriteMetadata();
    }

    printf("[MONITOR] W25Q ID %02X %02X %02X, cursor=0x%06lX records=%lu\r\n",
           manufacturer, memory_type, capacity,
           (unsigned long)g_flash_log_addr, (unsigned long)g_flash_record_count);
  }
}

static void Flash_LogFrame(const SensorFrame *frame, uint8_t state)
{
  uint8_t record[FLASH_LOG_RECORD_SIZE];
  const uint32_t tick = HAL_GetTick();
  const uint16_t crc_len = (uint16_t)(FLASH_LOG_RECORD_SIZE - 2u);
  uint16_t crc;

  if (!g_flash_present)
  {
    return;
  }

  if ((g_flash_log_addr < FLASH_LOG_START_ADDR) ||
      (g_flash_log_addr + FLASH_LOG_RECORD_SIZE > FLASH_LOG_END_ADDR) ||
      (((g_flash_log_addr - FLASH_LOG_START_ADDR) % FLASH_LOG_RECORD_SIZE) != 0u))
  {
    g_flash_log_addr = FLASH_LOG_START_ADDR;
  }

  if (((g_flash_log_addr - FLASH_LOG_START_ADDR) % FLASH_SECTOR_SIZE) == 0u)
  {
    Flash_SectorErase(g_flash_log_addr);
  }

  memset(record, 0xFF, sizeof(record));
  record[0] = FLASH_RECORD_MAGIC;
  record[1] = FRAME_VERSION;
  record[2] = frame->seq;
  record[3] = frame->status;
  record[4] = frame->temp;
  record[5] = frame->humi;
  record[6] = (uint8_t)(frame->mq135_adc >> 8);
  record[7] = (uint8_t)frame->mq135_adc;
  record[8] = (uint8_t)(frame->mq2_adc >> 8);
  record[9] = (uint8_t)frame->mq2_adc;
  record[10] = (uint8_t)(frame->rain_adc >> 8);
  record[11] = (uint8_t)frame->rain_adc;
  record[12] = (uint8_t)(frame->therm_adc >> 8);
  record[13] = (uint8_t)frame->therm_adc;
  record[14] = (uint8_t)((uint16_t)frame->therm_c10 >> 8);
  record[15] = (uint8_t)((uint16_t)frame->therm_c10);
  record[16] = frame->flame;
  record[17] = frame->rain_wet;
  record[18] = frame->therm_hot;
  record[19] = state;
  record[20] = (uint8_t)(tick >> 24);
  record[21] = (uint8_t)(tick >> 16);
  record[22] = (uint8_t)(tick >> 8);
  record[23] = (uint8_t)tick;
  record[24] = g_threshold_profile;
  record[25] = Monitor_Muted();
  Flash_U32ToBytes(&record[26], g_flash_record_count);

  crc = Flash_Crc16(record, (uint8_t)crc_len);
  record[30] = (uint8_t)(crc >> 8);
  record[31] = (uint8_t)crc;

  Flash_PageProgram(g_flash_log_addr, record, (uint8_t)sizeof(record));
  printf("[MONITOR] flash log addr=0x%06lX count=%lu seq=%u state=%u\r\n",
         (unsigned long)g_flash_log_addr, (unsigned long)g_flash_record_count,
         frame->seq, state);

  g_flash_log_addr += FLASH_LOG_RECORD_SIZE;
  g_flash_record_count++;
  if (g_flash_log_addr >= FLASH_LOG_END_ADDR)
  {
    g_flash_log_addr = FLASH_LOG_START_ADDR;
  }
  Flash_WriteMetadata();
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
