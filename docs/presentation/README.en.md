# Presentation Notes

[README](../../README.md) | [Chinese](README.zh-CN.md)

This folder contains the LaTeX Beamer deck for the Dual STM32 Safety Monitor project.

## Files

- [dual_stm32_safety_monitor_slides.tex](dual_stm32_safety_monitor_slides.tex): editable LaTeX Beamer source.
- [dual_stm32_safety_monitor_slides.pdf](dual_stm32_safety_monitor_slides.pdf): generated 16:9 PDF deck.

The deck should describe the current backend as SENSOR acquisition plus MONITOR stream decoding, OLED, buzzer, keys, optional W25Q64 logging, and Web Serial JSON output. Treat RGB/WS2813 material as legacy unless the firmware support is restored.

## Build

Run from the repository root:

```powershell
latexmk -xelatex -interaction=nonstopmode -halt-on-error -outdir=build/presentation docs/presentation/dual_stm32_safety_monitor_slides.tex
```

Copy the generated PDF back into this folder only when the deck source has intentionally changed.

