# 展示文稿说明

[中文 README](../../README.zh-CN.md) | [English](README.en.md)

本目录存放 Dual STM32 Safety Monitor 项目的 LaTeX Beamer 展示文稿。

## 文件

- [dual_stm32_safety_monitor_slides.tex](dual_stm32_safety_monitor_slides.tex)：可编辑的 LaTeX Beamer 源文件。
- [dual_stm32_safety_monitor_slides.pdf](dual_stm32_safety_monitor_slides.pdf)：生成后的 16:9 PDF 文稿。

文稿应描述当前后端：SENSOR 采集、MONITOR 流式解码、OLED、蜂鸣器、按键、可选 W25Q64 记录和 Web Serial JSON 输出。RGB/WS2813 内容只应作为 legacy 资料，除非恢复对应固件支持。

## 构建

在仓库根目录执行：

```powershell
latexmk -xelatex -interaction=nonstopmode -halt-on-error -outdir=build/presentation docs/presentation/dual_stm32_safety_monitor_slides.tex
```

只有在有意修改文稿源码后，才需要把生成的 PDF 复制回本目录。

