# -*- coding: utf-8 -*-
"""检查 QML 里「引用了不存在控件 id」这类失效引用（记一笔保存 bug 的同类风险）。

用法：python tools\check_qml_refs.py
返回码：0 = 没发现问题；1 = 发现可疑引用
"""
import io
import os
import re
import sys

UI_ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "src", "ui")

# 只检查这些后缀的局部控件 id（避免把 TextInput.AlignVCenter 等枚举误判）
SUFFIXES = ("Input", "Field", "Picker", "Popup", "Dialog", "List", "View", "Area", "Mouse", "Bar", "Chart", "Chevron", "Panel")
ID_PATTERN = re.compile(r"\bid\s*:\s*([A-Za-z_]\w*)")
REF_PATTERN = re.compile(r"\b([A-Za-z_]\w*)\s*\.\s*(?:text|forceActiveFocus|activeFocus|visible|open|close|show|submit|selectedIndex|currentIndex|count)\b")

problems = []
for dirpath, _, files in os.walk(UI_ROOT):
    for name in sorted(files):
        if not name.endswith(".qml"):
            continue
        path = os.path.join(dirpath, name)
        text = io.open(path, encoding="utf-8").read()
        defined = set(m.group(1) for m in ID_PATTERN.finditer(text))
        for match in REF_PATTERN.finditer(text):
            ref = match.group(1)
            if ref in defined:
                continue
            # 首字母大写的多半是类型/枚举（TextInput.AlignVCenter 等）
            if ref[0].isupper():
                continue
            # xxx.searchField.yyy 这类是别的对象的属性别名，不是局部 id
            if match.start() > 0 and text[match.start() - 1] == ".":
                continue
            # 触发条件不在局部控件后缀范围内就跳过
            if not ref.endswith(SUFFIXES):
                continue
            line = text[: match.start()].count("\n") + 1
            problems.append((os.path.relpath(path, UI_ROOT), line, ref))

if problems:
    print("发现可疑的失效控件引用：")
    for rel, line, ref in problems:
        print("  %s:%d  引用了未定义的 id「%s」" % (rel, line, ref))
    sys.exit(1)

print("OK：未发现失效控件引用")