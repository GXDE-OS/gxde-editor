#!/bin/bash
cd `dirname $0`
# 只扫 src/。原来写的是 */ ，那会把 web/ 也卷进来 —— lupdate 会去解析
# web/markdown-wysiwyg/node_modules 下的 .js，跑一次吐几千行 JS 语法错误，
# 真正的告警全被埋掉。可翻译的字符串全在 src/ 下，收窄之后结果逐字不变。
/usr/lib/qt6/bin/lupdate -recursive src/ -ts translations/gxde-editor_*.ts
