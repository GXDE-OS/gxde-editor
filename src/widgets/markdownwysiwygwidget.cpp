/*
 * Copyright (C) 2026 CharOfString <root@charofstring.cc>
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
 * ----------------------------------------------------------------------------
 * Markdown WYSIWYG editor frontend
 */

#ifdef USE_WEBENGINE
#include "markdownwysiwygwidget.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebChannel>
#include <QWebEnginePage>

namespace {

const char kEditorPageUrl[] = "qrc:/markdown/wysiwyg/index.html";
const char kBridgeObjectName[] = "gxdeEditor";

QString toJavaScriptString(const QString &value) {
    QJsonArray wrapper;
    wrapper.append(value);
    const QByteArray json = QJsonDocument(wrapper).toJson(
        QJsonDocument::Compact);
    return QString::fromUtf8(json.mid(1, json.size() - 2));
}

}  // namespace

MarkdownWysiwygBridge::MarkdownWysiwygBridge(QObject *parent)
    : QObject(parent) {}

void MarkdownWysiwygBridge::ready() {
    emit readyReceived();
}

void MarkdownWysiwygBridge::markdownLoaded(const QString &markdown) {
    emit markdownLoadedReceived(markdown);
}

void MarkdownWysiwygBridge::markdownChanged(const QString &markdown) {
    emit markdownChangedReceived(markdown);
}

MarkdownWysiwygWidget::MarkdownWysiwygWidget(QWidget *parent)
        : QWidget(parent) {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_webView = new QWebEngineView(this);
    m_webView->setContextMenuPolicy(Qt::NoContextMenu);

    m_webChannel = new QWebChannel(this);
    m_bridge = new MarkdownWysiwygBridge(this);
    m_webChannel->registerObject(QString::fromLatin1(kBridgeObjectName),
        m_bridge);
    m_webView->page()->setWebChannel(m_webChannel);

    layout->addWidget(m_webView);
    setLayout(layout);

    connect(m_bridge, &MarkdownWysiwygBridge::readyReceived,
            this, &MarkdownWysiwygWidget::handleReady);
    connect(m_bridge, &MarkdownWysiwygBridge::markdownLoadedReceived,
            this, &MarkdownWysiwygWidget::handleMarkdownLoaded);
    connect(m_bridge, &MarkdownWysiwygBridge::markdownChangedReceived,
            this, &MarkdownWysiwygWidget::handleMarkdownChanged);
    connect(m_webView, &QWebEngineView::loadFinished, this, [](bool ok) {
        if (!ok) {
            qWarning() << "Failed to load Milkdown:"
                << QString::fromLatin1(kEditorPageUrl);
        }
    });

    m_webView->setUrl(QUrl(QString::fromLatin1(kEditorPageUrl)));
}

void MarkdownWysiwygWidget::setMarkdown(const QString &markdown) {
    m_markdown = markdown;
    m_edited = false;
    m_pendingMarkdown = markdown;
    m_hasPendingMarkdown = true;

    if (m_ready) {
        applyPendingMarkdown();
    }
}

void MarkdownWysiwygWidget::setReadOnly(bool readOnly) {
    if (m_readOnly == readOnly) {
        return;
    }

    m_readOnly = readOnly;
    if (m_ready) {
        runScript(QStringLiteral("window.gxdeEditor.setReadOnly(%1)")
            .arg(m_readOnly ? QStringLiteral("true") : QStringLiteral("false")));
    }
}

void MarkdownWysiwygWidget::focusEditor() {
    if (m_ready) {
        runScript(QStringLiteral("window.gxdeEditor.focus()"));
    }
}

void MarkdownWysiwygWidget::runScript(const QString &script) {
    m_webView->page()->runJavaScript(script);
}

void MarkdownWysiwygWidget::applyPendingMarkdown() {
    m_hasPendingMarkdown = false;
    m_edited = false;
    runScript(QStringLiteral("window.gxdeEditor.load(%1)")
        .arg(toJavaScriptString(m_pendingMarkdown)));
}

void MarkdownWysiwygWidget::handleReady() {
    m_ready = true;

    if (m_readOnly) {
        runScript(QStringLiteral("window.gxdeEditor.setReadOnly(true)"));
    }
    if (m_hasPendingMarkdown) {
        applyPendingMarkdown();
    }

    emit ready();
}

void MarkdownWysiwygWidget::handleMarkdownLoaded(const QString &markdown) {
    m_markdown = markdown;
    m_edited = false;
}

void MarkdownWysiwygWidget::handleMarkdownChanged(const QString &markdown) {
    if (markdown == m_markdown) {
        return;
    }

    m_markdown = markdown;
    m_edited = true;
    emit markdownEdited(markdown);
}

#endif
