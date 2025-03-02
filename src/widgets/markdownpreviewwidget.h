#ifndef MARKDOWNPREVIEWWIDGET_H
#define MARKDOWNPREVIEWWIDGET_H
// markdownpreviewwidget.h
#pragma once
#include <QWidget>
#include <QWebEngineView>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QTimer>

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
    QTextEdit* m_sourceEditor = nullptr;
    QTimer* m_updateTimer;
    int m_lastScrollPosition = 0;
    bool m_darkMode = false;
};
#endif
