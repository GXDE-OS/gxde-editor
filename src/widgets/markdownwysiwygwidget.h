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

#ifndef MARKDOWNWYSIWYGWIDGET_H
#define MARKDOWNWYSIWYGWIDGET_H
#ifdef USE_WEBENGINE

#pragma once

#include <QWebEngineView>
#include <QWidget>

class QWebChannel;

class MarkdownWysiwygBridge : public QObject {
    Q_OBJECT

public:
    explicit MarkdownWysiwygBridge(QObject *parent = nullptr);

public slots:
    void ready();
    void markdownLoaded(const QString &markdown);
    void markdownChanged(const QString &markdown);

signals:
    void readyReceived();
    void markdownLoadedReceived(const QString &markdown);
    void markdownChangedReceived(const QString &markdown);
};

class MarkdownWysiwygWidget : public QWidget {
    Q_OBJECT

public:
    explicit MarkdownWysiwygWidget(QWidget *parent = nullptr);

    void setMarkdown(const QString &markdown);
    QString markdown() const { return m_markdown; }
    bool isEdited() const { return m_edited; }
    bool isReady() const { return m_ready; }

    void setReadOnly(bool readOnly);
    void focusEditor();

signals:
    void ready();
    void markdownEdited(const QString &markdown);

private:
    void runScript(const QString &script);
    void applyPendingMarkdown();
    void handleReady();
    void handleMarkdownLoaded(const QString &markdown);
    void handleMarkdownChanged(const QString &markdown);

    QWebEngineView *m_webView = nullptr;
    QWebChannel *m_webChannel = nullptr;
    MarkdownWysiwygBridge *m_bridge = nullptr;

    QString m_markdown;
    QString m_pendingMarkdown;
    bool m_hasPendingMarkdown = false;
    bool m_ready = false;
    bool m_edited = false;
    bool m_readOnly = false;
};

#endif
#endif
