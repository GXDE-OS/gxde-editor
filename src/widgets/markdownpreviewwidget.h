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

// 用于实现调用浏览器打开超链接
class MarkdownWebPage : public QWebEnginePage {
    Q_OBJECT
public:
    explicit MarkdownWebPage(QObject* parent = nullptr) : QWebEnginePage(parent) {}

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override {
        if (type == NavigationTypeLinkClicked) {
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

private slots:
    void scheduleUpdate();
    void performUpdate();

private:
    void initWebView();
    void updateScrollPosition();
    QString generateHtml(const QString& markdown);

    QWebEngineView* m_webView;
    MarkdownWebPage* m_webPage;
    QTextEdit* m_sourceEditor = nullptr;
    QTimer* m_updateTimer;
    int m_lastScrollPosition = 0;
    bool m_darkMode = false;
};
#endif
#endif
