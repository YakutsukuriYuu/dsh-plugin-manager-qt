#!/usr/bin/env python3
"""把 Lucide SVG（stroke=currentColor）按主题色批量烘焙成着色变体。

用法: python3 tools/make_icons.py
输出: resources/icons/<name>_<tone>.svg
      resources/icons/icons.cmake（供 CMakeLists include）
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "resources" / "icons"

# 主题色 → 变体后缀（与 Theme.qml / AppIcon.qml 的映射保持一致）
TONES = {
    "secondary": "#98989F",   # 常态
    "primary":   "#F5F5F7",   # 悬停/高亮
    "tertiary":  "#63636B",   # 弱化（空状态）
    "white":     "#FFFFFF",   # 主色底上的图标（导航选中、主按钮）
    "danger":    "#FF453A",   # 危险操作悬停
    "blue":      "#6B84FC",   # 主色浅（DSH 会话图标等）
}

# 只处理原始下载的图标（无 _tone 后缀的）
sources = sorted(p for p in SRC.glob("*.svg") if "_" not in p.stem)

generated = []
for src in sources:
    text = src.read_text(encoding="utf-8")
    for tone, color in TONES.items():
        out = text.replace('stroke="currentColor"', f'stroke="{color}"')
        name = f"{src.stem}_{tone}.svg"
        (SRC / name).write_text(out, encoding="utf-8")
        generated.append(f"resources/icons/{name}")

# 生成 CMake 文件列表
cmake = SRC / "icons.cmake"
lines = ["# 由 tools/make_icons.py 生成，请勿手改", "set(ICON_FILES"]
lines += [f"    {f}" for f in generated]
lines.append(")")
cmake.write_text("\n".join(lines) + "\n", encoding="utf-8")

print(f"已生成 {len(generated)} 个着色图标（{len(sources)} 图标 × {len(TONES)} 色）")
print(f"CMake 列表: {cmake}")
