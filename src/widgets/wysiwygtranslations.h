/*
 * Copyright (C) 2026 CharOfString <root@charofstring.cc>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------------
 * Markdown WYSIWYG editor frontend
 */

#ifndef WYSIWYGTRANSLATIONS_H
#define WYSIWYGTRANSLATIONS_H

#include <QString>

/**
 * 编辑器页面（web/markdown-wysiwyg/）的界面文案，编译成一段脚本。
 *
 * 页面是纯前端产物，不认识 Qt 的翻译机制，所以文案得由这边翻译好、在页面脚本
 * 执行之前注入进去。注入的脚本给页面留下两样东西：
 *
 *     window.__gxdeI18n = { texts: { "Heading 1": "标题 1", ... },
 *                           locale: "zh-CN" }
 *
 * texts 的键就是英文原文，跟 .ts 里的 <source> 一字不差；页面查不到译文时直接
 * 用键本身。这样页面侧不需要另建一张 key -> 英文 的对照表，两边也就没有对不上的
 * 可能 —— 漏一条最多是那一处留在英文上，不会变成空白。
 *
 * locale 是给 <html lang> 用的，别省：Chromium 拿它决定汉字取哪套字形，
 * 写死 zh-CN 的话日文用户看到的是中文字形。
 *
 * 文案本身在 .cpp 里，那张表同时也是 lupdate 取 <source> 的地方。加一条文案要在
 * 两处动手：这里加进表，页面里用 t()/t1() 取。
 */
QString markdownWysiwygLocaleScript();

#endif  // WYSIWYGTRANSLATIONS_H
