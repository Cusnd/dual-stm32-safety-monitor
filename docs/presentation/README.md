# Project Presentation / 项目展示文稿

This folder contains the bilingual LaTeX Beamer presentation for the Dual STM32 Safety Monitor project.

本目录存放 Dual STM32 Safety Monitor 项目的中英双语 LaTeX Beamer 展示文稿。

## Files / 文件

- [dual_stm32_safety_monitor_slides.tex](dual_stm32_safety_monitor_slides.tex): editable LaTeX Beamer source.
- [dual_stm32_safety_monitor_slides.pdf](dual_stm32_safety_monitor_slides.pdf): generated 16:9 PDF deck.

## Build / 构建

Run the following command from the repository root:

在仓库根目录执行：

```powershell
latexmk -xelatex -interaction=nonstopmode -halt-on-error -outdir=build/presentation docs/presentation/dual_stm32_safety_monitor_slides.tex
```

The generated PDF can then be copied back into this folder if needed.

如需更新仓库内的成品 PDF，可将生成结果复制回本目录。
