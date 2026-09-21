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

#include "wysiwygtranslations.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QLocale>
#include <QVariantMap>

namespace {

/**
 * 页面上所有给人看的字，英文原文，按出现的地方分组。
 *
 * 这张表是给 lupdate 看的：它从这里取 <source> 写进 .ts。运行时查表用的是同一批
 * 原文字符串（见下面的 markdownWysiwygLocaleScript()），所以这里加一条、页面里
 * 就能用一条。
 *
 * 重复的原文只列一次：lupdate 按 (context, source) 去重，列两遍在 .ts 里也还是
 * 一条，反而显得像两处不同的文案。
 *
 * lupdate 认 `//:` 开头的注释，会当作 <extracomment> 带给译者。像 Copy、Text、
 * Code 这种脱离上下文就不知道指什么的词，注释不能省。
 */
const char *const kTextSources[] = {
    //: 空文档里的占位提示，提示用户输入斜杠唤起块类型菜单
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Type / to insert"),

    //: 斜杠菜单（输入 / 唤起）里的分组名
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Text"),
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "List"),
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Advanced"),

    //: 斜杠菜单里的块类型名，同时也是顶部工具栏标题下拉里的选项名
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Heading 1"),
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Heading 2"),
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Heading 3"),
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Heading 4"),
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Heading 5"),
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Heading 6"),
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Quote"),
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Divider"),
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Bullet List"),
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Ordered List"),
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Task List"),
    //: 斜杠菜单里的代码块，不是"复制"那个 Copy
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Code"),
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Table"),

    //: 顶部工具栏标题下拉里的正文选项
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Paragraph"),

    //: 选中文字后浮出的工具栏按钮的悬停提示
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Bold"),
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Italic"),
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Strikethrough"),
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Inline code"),
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Link"),

    //: 代码块工具栏：语言搜索框的占位
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Search language"),
    //: 代码块工具栏：语言搜索没有结果时的提示
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "No result"),
    //: 代码块工具栏：复制按钮
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Copy"),

    //: 链接浮层里输入地址的占位
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Paste link..."),

    //: 插入表格前的尺寸面板
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Insert Table"),
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Rows"),
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Columns"),
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Confirm"),
    //: 表格尺寸面板上"减一"按钮的无障碍名字，%1 是"Rows"或"Columns"
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Decrease %1"),
    //: 表格尺寸面板上"加一"按钮的无障碍名字，%1 是"Rows"或"Columns"
    QT_TRANSLATE_NOOP("MarkdownWysiwyg", "Increase %1"),
};

}  // namespace

QString markdownWysiwygLocaleScript()
{
    QVariantMap texts;
    for (const char *source : kTextSources) {
        texts.insert(QString::fromUtf8(source),
                     QCoreApplication::translate("MarkdownWysiwyg", source));
    }

    QVariantMap payload;
    payload.insert(QStringLiteral("texts"), texts);
    // HTML 的 lang 要的是 BCP 47，Qt 给的是 zh_CN 这种下划线写法。
    payload.insert(QStringLiteral("locale"),
                   QLocale::system().name().replace(QLatin1Char('_'),
                                                    QLatin1Char('-')));

    return QStringLiteral("window.__gxdeI18n=%1;").arg(
        QString::fromUtf8(QJsonDocument::fromVariant(payload)
                              .toJson(QJsonDocument::Compact)));
}
