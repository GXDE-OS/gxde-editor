#ifndef MARKDOWNPREVIEWWIDGET_H
#define MARKDOWNPREVIEWWIDGET_H
#ifdef USE_WEBENGINE
// markdownpreviewwidget.h
#pragma once
#include <QWidget>
#include <QWebEngineView>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QTimer>
#include <QDesktopServices>
#include <QProcessEnvironment>
#include <QVariantMap>

// 用于实现调用浏览器打开超链接
class QWebChannel;
class MarkdownPreviewWidget;

// 仅暴露滚动同步接口给 QWebChannel，避免把整个 QWidget 暴露给页面
class MarkdownPreviewBridge : public QObject {
    Q_OBJECT
public:
    explicit MarkdownPreviewBridge(MarkdownPreviewWidget* widget);

public slots:
    void syncPreviewToEditor(double ratio);

private:
    MarkdownPreviewWidget* m_widget;
};

class MarkdownWebPage : public QWebEnginePage {
    Q_OBJECT
public:
    explicit MarkdownWebPage(QObject* parent = nullptr) : QWebEnginePage(parent) {}

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override {
        if (type == NavigationTypeLinkClicked) {
            if (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https"))
                QDesktopServices::openUrl(url);
            return false;
        }
        return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    }
};

class MarkdownPreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit MarkdownPreviewWidget(QWidget* parent = nullptr);
    void setSourceEditor(QTextEdit* editor);
    void setDarkTheme(bool enabled);
    void applyTheme(const QVariantMap &themeMap);
    void setDocumentPath(const QString &path);
    void setReadMode(bool enabled);
    void refreshNow();

    static bool isSupport();

    void syncPreviewToEditor(double ratio);

signals:
    void contextMenuRequested(const QPoint &globalPosition);

private slots:
    void scheduleUpdate();
    void performUpdate();
    void handleUpdateTimeout();

    void syncEditorToPreview();
    void pollPreviewScroll();
    void onPreviewLoadFinished(bool ok);

private:
    void initWebView();
    QString generateHtml(const QString& markdown);
    void setPreviewScrollRatio(double ratio);

    QWebEngineView* m_webView = nullptr;
    MarkdownWebPage* m_webPage = nullptr;
    QWebChannel* m_webChannel = nullptr;
    MarkdownPreviewBridge* m_bridge = nullptr;
    QTextEdit* m_sourceEditor = nullptr;
    QTimer* m_updateTimer = nullptr;
    QTimer* m_scrollSyncTimer = nullptr;
    bool m_syncing = false;
    bool m_darkMode = false;
    bool m_readMode = false;
    bool m_updatePending = false;
    QString m_documentPath;
    QColor m_backgroundColor;
    QColor m_foregroundColor;
    double m_lastScrollRatio = 0.0;
    double m_lastPreviewRatio = 0.0;
    int m_loadGeneration = 0;
};
#endif
#endif
