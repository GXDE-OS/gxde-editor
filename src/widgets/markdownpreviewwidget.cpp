// MarkdownPreviewWidget.cpp
#ifdef USE_WEBENGINE
#include "markdownpreviewwidget.h"
#include <QScrollBar>
#include <QFile>
#include <QWebEngineSettings>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QtMath>
#include <QWebChannel>

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

MarkdownPreviewBridge::MarkdownPreviewBridge(MarkdownPreviewWidget* widget)
    : QObject(widget)
    , m_widget(widget)
{
}

void MarkdownPreviewBridge::syncPreviewToEditor(double ratio)
{
    if (m_widget) {
        m_widget->syncPreviewToEditor(ratio);
    }
}

MarkdownPreviewWidget::MarkdownPreviewWidget(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_webView = new QWebEngineView(this);
    m_webPage = new MarkdownWebPage(this);
    m_webChannel = new QWebChannel(this);
    m_bridge = new MarkdownPreviewBridge(this);
    m_webChannel->registerObject(QStringLiteral("markdownPreview"), m_bridge);
    m_webPage->setWebChannel(m_webChannel);

    m_webView->setPage(m_webPage);
    m_webView->settings()->setAttribute(QWebEngineSettings::WebAttribute::LocalContentCanAccessRemoteUrls, true);
    m_webView->page()->setBackgroundColor(Qt::transparent);

    layout->addWidget(m_webView);
    setLayout(layout);
    initWebView();

    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(true);
    connect(m_updateTimer, &QTimer::timeout, this, &MarkdownPreviewWidget::performUpdate);

    // 定时轮询预览区滚动位置，用于预览 -> 编辑器的同步
    m_scrollSyncTimer = new QTimer(this);
    m_scrollSyncTimer->setInterval(50);
    connect(m_scrollSyncTimer, &QTimer::timeout, this, &MarkdownPreviewWidget::pollPreviewScroll);

    // 页面加载完成后恢复滚动比例并开始监听预览区滚动
    connect(m_webView, &QWebEngineView::loadFinished,
            this, &MarkdownPreviewWidget::onPreviewLoadFinished);
}

bool MarkdownPreviewWidget::isSupport()
{
    // 只要编译期启用了 WebEngine，就启用 Markdown 预览。
    // 之前还会根据 root/chroot 环境禁用，导致部分环境下看不到预览界面。
    return true;
}

void MarkdownPreviewWidget::setSourceEditor(QTextEdit* editor)
{
    if (m_sourceEditor == editor)
        return;

    if (m_sourceEditor) {
        disconnect(m_sourceEditor, &QTextEdit::textChanged,
                   this, &MarkdownPreviewWidget::scheduleUpdate);
        disconnect(m_sourceEditor->verticalScrollBar(), &QScrollBar::valueChanged,
                   this, &MarkdownPreviewWidget::syncEditorToPreview);
    }

    m_sourceEditor = editor;
    ++m_loadGeneration;
    m_syncing = false;
    m_lastScrollRatio = 0.0;
    m_lastPreviewRatio = 0.0;

    if (m_scrollSyncTimer)
        m_scrollSyncTimer->stop();

    if (!m_sourceEditor) {
        m_webView->setHtml(QString());
        return;
    }

    connect(m_sourceEditor, &QTextEdit::textChanged,
            this, &MarkdownPreviewWidget::scheduleUpdate);
    connect(m_sourceEditor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MarkdownPreviewWidget::syncEditorToPreview);

    // 初始化内容，页面加载完成后会恢复滚动位置
    m_webView->setHtml(generateHtml(m_sourceEditor->toPlainText()), QUrl("qrc:/"));
}

void MarkdownPreviewWidget::syncEditorToPreview()
{
    if (!m_sourceEditor || m_syncing)
        return;

    QScrollBar* scrollBar = m_sourceEditor->verticalScrollBar();
    if (scrollBar->maximum() <= 0)
        return;

    const double ratio = static_cast<double>(scrollBar->value()) / scrollBar->maximum();
    m_lastPreviewRatio = ratio;

    m_syncing = true;
    setPreviewScrollRatio(ratio);

    // 短暂锁定，避免 JS 侧回读滚动位置再次触发编辑器滚动
    QTimer::singleShot(100, this, [this] {
        m_syncing = false;
    });
}

void MarkdownPreviewWidget::syncPreviewToEditor(double ratio)
{
    if (!m_sourceEditor || m_syncing)
        return;

    QScrollBar* scrollBar = m_sourceEditor->verticalScrollBar();
    if (scrollBar->maximum() <= 0)
        return;

    m_lastPreviewRatio = ratio;
    m_syncing = true;
    scrollBar->setValue(qRound(ratio * scrollBar->maximum()));

    QTimer::singleShot(100, this, [this] {
        m_syncing = false;
    });
}

void MarkdownPreviewWidget::pollPreviewScroll()
{
    if (!m_sourceEditor || m_syncing || !m_webView)
        return;

    const int generation = m_loadGeneration;
    m_webView->page()->runJavaScript(
        "var e = document.documentElement;"
        "var h = e.scrollHeight - e.clientHeight;"
        "h > 0 ? (window.scrollY || e.scrollTop || 0) / h : 0;",
        [this, generation](const QVariant &result) {
            if (generation != m_loadGeneration || !m_sourceEditor || m_syncing)
                return;

            const double ratio = result.toDouble();
            if (qAbs(ratio - m_lastPreviewRatio) < 0.0005)
                return;

            m_lastPreviewRatio = ratio;

            QScrollBar* scrollBar = m_sourceEditor->verticalScrollBar();
            if (scrollBar->maximum() <= 0)
                return;

            m_syncing = true;
            scrollBar->setValue(qRound(ratio * scrollBar->maximum()));
            QTimer::singleShot(100, this, [this] {
                m_syncing = false;
            });
        });
}

void MarkdownPreviewWidget::onPreviewLoadFinished(bool ok)
{
    if (!ok || !m_sourceEditor)
        return;

    // 在 JS 真正执行 scrollTo 后再记录比例并启动轮询，
    // 避免轮询读到恢复前的旧位置导致滚动被拉回去。
    const double ratio = m_lastScrollRatio;
    const int generation = m_loadGeneration;
    m_webView->page()->runJavaScript(
        QString("var e = document.documentElement;"
                "var h = e.scrollHeight - e.clientHeight;"
                "window.scrollTo(0, %1 * (h > 0 ? h : 0));").arg(ratio),
        [this, ratio, generation](const QVariant &) {
            if (generation != m_loadGeneration)
                return;
            m_lastPreviewRatio = ratio;
            if (m_sourceEditor && m_scrollSyncTimer && !m_scrollSyncTimer->isActive())
                m_scrollSyncTimer->start();
        }
    );
}

void MarkdownPreviewWidget::scheduleUpdate()
{
    // 记录当前预览比例，内容刷新后尽量维持阅读位置
    m_lastScrollRatio = m_lastPreviewRatio;
    m_updateTimer->start(200); // 200ms防抖
}

void MarkdownPreviewWidget::performUpdate()
{
    if (!m_sourceEditor)
        return;

    // 刷新期间暂停轮询，避免在页面重新加载过程中误同步
    if (m_scrollSyncTimer)
        m_scrollSyncTimer->stop();

    const QString content = m_sourceEditor->toPlainText();
    const QString html = generateHtml(content);

    ++m_loadGeneration;
    m_webView->setHtml(html, QUrl("qrc:/"));
}

QString MarkdownPreviewWidget::generateHtml(const QString& markdown)
{
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
            <script src="qrc:///qtwebchannel/qwebchannel.js"></script>
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

                // 通过 QWebChannel 将预览区滚动事件实时通知给 C++ 侧
                new QWebChannel(qt.webChannelTransport, function(channel) {
                    const bridge = channel.objects.markdownPreview;
                    if (!bridge) return;
                    window.addEventListener('scroll', function() {
                        const e = document.documentElement;
                        const h = e.scrollHeight - e.clientHeight;
                        if (h > 0) {
                            bridge.syncPreviewToEditor((window.scrollY || e.scrollTop || 0) / h);
                        }
                    });
                });
            </script>
        </body>
        </html>
    )").arg(m_darkMode ? STYLE_DARK : STYLE_LIGHT)
      .arg(escapedMarkdown); // 直接插入已转义内容
}

void MarkdownPreviewWidget::setDarkTheme(bool enabled)
{
    m_darkMode = enabled;
    scheduleUpdate();
}

void MarkdownPreviewWidget::setPreviewScrollRatio(double ratio)
{
    if (!m_webView || !m_sourceEditor)
        return;

    m_lastPreviewRatio = ratio;
    m_webView->page()->runJavaScript(
        QString("var e = document.documentElement;"
                "var h = e.scrollHeight - e.clientHeight;"
                "window.scrollTo(0, %1 * (h > 0 ? h : 0));").arg(ratio)
    );
}

void MarkdownPreviewWidget::initWebView()
{
    // 禁用上下文菜单
    //m_webView->setContextMenuPolicy(Qt::NoContextMenu);

    // 启用开发者工具（可选）
    //m_webView->page()->setDevToolsPage(m_webView->page());
    //m_webView->page()->triggerAction(QWebEnginePage::InspectElement);
}
#endif
