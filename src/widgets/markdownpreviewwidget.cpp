// MarkdownPreviewWidget.cpp
#ifdef USE_WEBENGINE
#include "markdownpreviewwidget.h"
#include <QScrollBar>
#include <QFile>
#include <QWebEngineSettings>
#include <QJsonDocument>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>


// 使用GitHub风格的Markdown样式
const QString STYLE_LIGHT = R"(
  <link rel="stylesheet" href="qrc:/markdown/github-markdown.css">
  <style>
    .markdown-body {
      box-sizing: border-box;
      min-width: 200px;
      max-width: 1200px;
      margin: 0 auto;
      padding: 45px;
    }
    @media (max-width: 767px) {
      .markdown-body {
        padding: 15px;
      }
    }
  </style>
)";

const QString STYLE_DARK = R"(
  <link rel="stylesheet" href="qrc:/markdown/github-markdown-dark.css">
  <style>
    .markdown-body {
      background-color: #0d111700;
      color: #c9d1d9;
    }
  </style>
)";

MarkdownPreviewWidget::MarkdownPreviewWidget(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_webView = new QWebEngineView(this);
    m_webPage = new MarkdownWebPage(this);
    m_webView->setPage(m_webPage);
    m_webView->settings()->setAttribute(QWebEngineSettings::WebAttribute::LocalContentCanAccessRemoteUrls, true);
    m_webView->page()->setBackgroundColor(Qt::transparent);

    layout->addWidget(m_webView);
    setLayout(layout);
    initWebView();

    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(true);
    connect(m_updateTimer, &QTimer::timeout, this, &MarkdownPreviewWidget::performUpdate);
}

void MarkdownPreviewWidget::setSourceEditor(QTextEdit* editor) {
    if (m_sourceEditor == editor) return;

    if (m_sourceEditor) {
        disconnect(m_sourceEditor, &QTextEdit::textChanged,
                   this, &MarkdownPreviewWidget::scheduleUpdate);
    }

    m_sourceEditor = editor;

    if (m_sourceEditor) {
        connect(m_sourceEditor, &QTextEdit::textChanged,
                this, &MarkdownPreviewWidget::scheduleUpdate);
        scheduleUpdate();
    }
}

void MarkdownPreviewWidget::scheduleUpdate() {
    m_lastScrollPosition = m_webView->page()->scrollPosition().y();
    m_updateTimer->start(200); // 200ms防抖
}

void MarkdownPreviewWidget::performUpdate() {
    if (!m_sourceEditor) return;

    const QString content = m_sourceEditor->toPlainText();
    const QString html = generateHtml(content);

    m_webView->setHtml(html, QUrl("qrc:/"));

    // 延迟恢复滚动位置
    QTimer::singleShot(50, this, [this] {
        m_webView->page()->runJavaScript(
            QString("window.scrollTo(0, %1);").arg(m_lastScrollPosition)
        );
    });
}

QString MarkdownPreviewWidget::generateHtml(const QString& markdown) {
    // 使用 QJsonDocument 正确序列化字符串
    QJsonArray a;
    a.append(QJsonValue(markdown));
    QJsonDocument doc(a); // 转换为 JSON 对象

    QString escapedMarkdown = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    // 移除 JSON 外层的引号（保留内容转义）
    if (escapedMarkdown.startsWith('[') && escapedMarkdown.endsWith(']')) {
        escapedMarkdown = escapedMarkdown.mid(2, escapedMarkdown.length() - 4);
    }

    return QString(R"(
        <!DOCTYPE html>
        <html>
        <head>
            <meta charset="utf-8">
            %1
            <script src="qrc:/markdown/marked.min.js"></script>
        </head>
        <body>
            <article class="markdown-body">
                <div id="content"></div>
            </article>
            <script>
                const md = "%2";  // 使用双引号包裹
                document.getElementById('content').innerHTML = marked.parse(md);

                // 自动调整图片大小
                document.querySelectorAll('img').forEach(img => {
                    img.style.maxWidth = '100%';
                    img.style.height = 'auto';
                });
            </script>
        </body>
        </html>
    )").arg(m_darkMode ? STYLE_DARK : STYLE_LIGHT)
      .arg(escapedMarkdown); // 直接插入已转义内容
}

void MarkdownPreviewWidget::setDarkTheme(bool enabled) {
    m_darkMode = enabled;
    scheduleUpdate();
}

void MarkdownPreviewWidget::initWebView() {
    // 禁用上下文菜单
    //m_webView->setContextMenuPolicy(Qt::NoContextMenu);

    // 启用开发者工具（可选）
    m_webView->page()->setDevToolsPage(m_webView->page());
    m_webView->page()->triggerAction(QWebEnginePage::InspectElement);
}
#endif
