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

#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlSchemeHandler>
#include <QWebEngineView>
#include <QWidget>

class QAction;
class QWebChannel;

/**
 * 编辑器页面的 scheme 处理器。
 *
 * 页面本身是从 Qt 资源里拿的，但 markdown 里的图片在磁盘上：qrc 页面不许加载
 * file:// 子资源（连 LocalContentCanAccessFileUrls 也救不了，那个属性只管
 * file:// 页面之间的互访），所以图片得由我们自己读出来喂给页面。
 *
 * 主机名分两种：
 *   gxde-md://editor/…  编辑器自身资源，映射到 :/markdown/wysiwyg
 *   gxde-md://doc/…     文档目录，路径直接就是磁盘上的绝对路径
 */
class MarkdownWysiwygSchemeHandler : public QWebEngineUrlSchemeHandler {
    Q_OBJECT

public:
    explicit MarkdownWysiwygSchemeHandler(QObject *parent = nullptr);

    void requestStarted(QWebEngineUrlRequestJob *job) override;
};

/**
 * 注册编辑器用的 URL scheme。
 *
 * QWebEngineUrlScheme::registerScheme() 必须赶在 QApplication 构造之前调用，
 * 所以只能由 main() 在最开头喊一声，没法藏在控件构造函数里。
 */
void registerMarkdownWysiwygUrlScheme();

class MarkdownWysiwygBridge : public QObject {
    Q_OBJECT

public:
    explicit MarkdownWysiwygBridge(QObject *parent = nullptr);

public slots:
    void ready();
    void markdownLoaded(const QString &markdown);
    void markdownChanged(const QString &markdown);
    void historyState(bool canUndo, bool canRedo);

signals:
    void readyReceived();
    void markdownLoadedReceived(const QString &markdown);
    void markdownChangedReceived(const QString &markdown);
    void historyStateReceived(bool canUndo, bool canRedo);
};

class MarkdownWysiwygWidget : public QWidget {
    Q_OBJECT

public:
    explicit MarkdownWysiwygWidget(QWidget *parent = nullptr);

    void setMarkdown(const QString &markdown);
    /// 文档路径，用来把 markdown 里的相对图片路径落到磁盘上。
    /// 换文件时要在 setMarkdown() 之前设置。
    void setDocumentPath(const QString &path);
    QString markdown() const { return m_markdown; }
    bool isEdited() const { return m_edited; }
    bool isReady() const { return m_ready; }

    void setReadOnly(bool readOnly);
    void focusEditor();

    QList<QAction *> contextMenuActions();

signals:
    void ready();
    void markdownEdited(const QString &markdown);
    void contextMenuRequested(const QPoint &globalPosition);

private:
    void runScript(const QString &script);
    void applyPendingMarkdown();
    void handleReady();
    void handleMarkdownLoaded(const QString &markdown);
    void handleMarkdownChanged(const QString &markdown);
    void refreshActionStates();

    QWebEngineView *m_webView = nullptr;
    QWebChannel *m_webChannel = nullptr;
    MarkdownWysiwygBridge *m_bridge = nullptr;
    MarkdownWysiwygSchemeHandler *m_schemeHandler = nullptr;

    QAction *m_undoAction = nullptr;
    QAction *m_redoAction = nullptr;
    QAction *m_cutAction = nullptr;
    QAction *m_copyAction = nullptr;
    QAction *m_pasteAction = nullptr;
    QAction *m_selectAllAction = nullptr;

    QString m_documentDirectory;
    QString m_markdown;
    QString m_pendingMarkdown;
    bool m_hasPendingMarkdown = false;
    bool m_ready = false;
    bool m_edited = false;
    bool m_readOnly = false;
    bool m_canUndo = false;
    bool m_canRedo = false;
};

#endif
#endif
