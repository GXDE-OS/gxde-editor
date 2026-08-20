/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "markdownpreview.h"

#include <QFileInfo>
#include <QTextDocument>
#include <QUrl>

MarkdownPreview::MarkdownPreview(QWidget *parent)
    : QTextBrowser(parent)
{
    setAccessibleName("MarkdownPreviewView");
    setFrameShape(QFrame::NoFrame);
    setOpenLinks(false);
    setOpenExternalLinks(false);
    setReadOnly(true);

    // 预览内容与窗口边缘保持呼吸感，编辑区和预览区之间由 QSplitter 手柄分隔。
    setViewportMargins(24, 20, 24, 24);

    // 预览默认正文字号比 QTextDocument 的默认 9pt 大一些。
    QFont previewFont = font();
    previewFont.setPointSizeF(12);
    setFont(previewFont);
}

bool MarkdownPreview::isSupported()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return true;
#else
    return false;
#endif
}

bool MarkdownPreview::isMarkdownFile(const QString &filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return suffix == QStringLiteral("md") || suffix == QStringLiteral("markdown");
}

void MarkdownPreview::updatePreview(const QString &markdown)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QTextDocument *newDoc = new QTextDocument(this);

    // 让预览正文字号保持易读：跟随控件字体（固定 12pt），主题切换时颜色仍由调色板决定。
    QFont baseFont = font();
    baseFont.setPointSizeF(12);
    newDoc->setDefaultFont(baseFont);

    // 基础样式：标题分级、段落间距与等宽代码。
    newDoc->setDefaultStyleSheet(QStringLiteral(
        "h1 { font-size: 32px; font-weight: 600; margin-top: 24px; margin-bottom: 12px; }"
        "h2 { font-size: 27px; font-weight: 600; margin-top: 20px; margin-bottom: 10px; }"
        "h3 { font-size: 23px; font-weight: 600; margin-top: 16px; margin-bottom: 8px; }"
        "h4 { font-size: 19px; font-weight: 600; margin-top: 14px; margin-bottom: 8px; }"
        "h5 { font-size: 17px; font-weight: 600; margin-top: 12px; margin-bottom: 6px; }"
        "h6 { font-size: 16px; font-weight: 600; margin-top: 10px; margin-bottom: 6px; }"
        "p { margin-top: 0; margin-bottom: 12px; }"
        "ul, ol { margin-top: 0; margin-bottom: 12px; }"
        "code, pre { font-family: 'monospace'; }"
        "pre { margin-top: 0; margin-bottom: 12px; }"
        "blockquote { margin-left: 16px; margin-top: 0; margin-bottom: 12px; }"));
    newDoc->setMarkdown(markdown);

    // setDocument 会接管新文档（父对象为 this）并释放上一份文档，无需手动 delete
    setDocument(newDoc);
#else
    Q_UNUSED(markdown)
    setPlainText(markdown);
#endif
}

void MarkdownPreview::doSetSource(const QUrl &url, QTextDocument::ResourceType type)
{
    Q_UNUSED(url)
    Q_UNUSED(type)
    // 预览只读，不进行链接跳转
}
