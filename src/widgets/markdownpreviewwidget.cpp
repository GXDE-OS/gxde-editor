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

// JavaScript 同步代码
const QString SCROLL_SYNC_JS = R"(
let lastSentPosition = 0;
const SYNC_THRESHOLD = 5; // 像素阈值避免微小抖动

window.addEventListener('scroll', function(e) {
    const current = window.scrollY || document.documentElement.scrollTop;
    if (Math.abs(current - lastSentPosition) > SYNC_THRESHOLD) {
        lastSentPosition = current;
        window.external.syncPreviewScroll(current);
    }
});
)";

MarkdownPreviewWidget::MarkdownPreviewWidget(QWidget* parent)
    : QWidget(parent)
{
    // 禁用sandbox
    qputenv("QTWEBENGINE_DISABLE_SANDBOX", "1");

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

    m_scrollSyncTimer = new QTimer(this);
    m_scrollSyncTimer->setSingleShot(true);
    //connect(m_scrollSyncTimer, &QTimer::timeout, this, &MarkdownPreviewWidget::setupScrollSync);
}

void MarkdownPreviewWidget::setSourceEditor(QTextEdit* editor) {
    if (m_sourceEditor == editor) return;

    if (m_sourceEditor) {
        disconnect(m_sourceEditor, &QTextEdit::textChanged,
                   this, &MarkdownPreviewWidget::scheduleUpdate);
    }

    if (m_sourceEditor) {
        disconnect(m_sourceEditor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MarkdownPreviewWidget::syncEditorToPreview);
    }

    m_sourceEditor = editor;

    if (m_sourceEditor) {
        connect(m_sourceEditor, &QTextEdit::textChanged,
                this, &MarkdownPreviewWidget::scheduleUpdate);
        scheduleUpdate();
    }

    // 初始化滚动同步
    if (m_sourceEditor) {
        connect(m_sourceEditor->verticalScrollBar(), &QScrollBar::valueChanged,
                this, &MarkdownPreviewWidget::syncEditorToPreview);
        // 初始化内容
        m_webView->setHtml(generateHtml(m_sourceEditor->toPlainText()));
    }
}

void MarkdownPreviewWidget::setupScrollSync() {
    // 注入JavaScript同步处理器
    QWebEngineScript script;
    script.setSourceCode(SCROLL_SYNC_JS);
    script.setName("ScrollSync");
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setInjectionPoint(QWebEngineScript::DocumentReady);
    m_webView->page()->scripts().insert(script);
}

double MarkdownPreviewWidget::calculateSyncRatio() {
    const int editorHeight = m_sourceEditor->height();
    const int editorContentHeight = m_sourceEditor->document()->size().height();
    const double editorVisibleRatio = static_cast<double>(editorHeight) / editorContentHeight;

    // 获取预览内容高度
    int previewContentHeight = 0;
    m_webView->page()->runJavaScript("document.documentElement.scrollHeight",
        [&](const QVariant& result) {
            previewContentHeight = result.toInt();
        });

    const int previewHeight = m_webView->height();
    const double previewVisibleRatio = static_cast<double>(previewHeight) / previewContentHeight;

    // 计算比例修正系数（考虑padding等因素）
    return (1.0 - previewVisibleRatio) / (1.0 - editorVisibleRatio);
}

void MarkdownPreviewWidget::syncEditorToPreview() {
    //if (m_syncing) return;
    m_syncing = true;

    // 计算滚动比例
    QScrollBar* scrollBar = m_sourceEditor->verticalScrollBar();
    const double ratio = static_cast<double>(scrollBar->value()) / scrollBar->maximum();

    // 异步执行滚动操作
    m_webView->page()->runJavaScript(
        QString("window.scrollTo(0, document.documentElement.scrollHeight * %1);")
            .arg(ratio)
    );

    // 200ms后解除同步锁定
    //QTimer::singleShot(200, this, [this](){ m_syncing = false; });
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
    //m_webView->page()->setDevToolsPage(m_webView->page());
    //m_webView->page()->triggerAction(QWebEnginePage::InspectElement);
}
#endif
