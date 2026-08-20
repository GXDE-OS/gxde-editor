// MarkdownPreviewWidget.cpp
#ifdef USE_WEBENGINE
#include "markdownpreviewwidget.h"
#include "../markdownlogic.h"
#include <QScrollBar>
#include <QTimer>
#include <QFile>
#include <QWebEngineSettings>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QtMath>
#include <QWebChannel>
#include <QFileInfo>

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
    m_webView->settings()->setAttribute(QWebEngineSettings::WebAttribute::LocalContentCanAccessFileUrls, true);
    m_webView->page()->setBackgroundColor(Qt::transparent);

    layout->addWidget(m_webView);
    setLayout(layout);
    initWebView();

    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(300);
    connect(m_updateTimer, &QTimer::timeout, this, &MarkdownPreviewWidget::handleUpdateTimeout);

    // 定时轮询预览区滚动位置，用于预览 -> 编辑器的同步
    m_scrollSyncTimer = new QTimer(this);
    m_scrollSyncTimer->setInterval(50);
    connect(m_scrollSyncTimer, &QTimer::timeout, this, &MarkdownPreviewWidget::pollPreviewScroll);

    // 页面加载完成后恢复滚动比例并开始监听预览区滚动
    connect(m_webView, &QWebEngineView::loadFinished,
            this, &MarkdownPreviewWidget::onPreviewLoadFinished);
    m_webView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_webView, &QWidget::customContextMenuRequested, this, [this](const QPoint &position) {
        emit contextMenuRequested(m_webView->mapToGlobal(position));
    });
}
bool MarkdownPreviewWidget::isSupport()
{
    // 只要编译期启用了 WebEngine，就启用 Markdown 预览。
    // 之前还会根据 root/chroot 环境禁用，导致部分环境下看不到预览界面。
    return true;
}

void MarkdownPreviewWidget::setSourceEditor(QTextEdit* editor)
{
    if (m_sourceEditor == editor) {
        refreshNow();
        return;
    }

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
        m_updateTimer->stop();
        m_updatePending = false;
        m_webView->setHtml(QString());
        return;
    }

    connect(m_sourceEditor, &QTextEdit::textChanged,
            this, &MarkdownPreviewWidget::scheduleUpdate);
    connect(m_sourceEditor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MarkdownPreviewWidget::syncEditorToPreview);

    QScrollBar *scrollBar = m_sourceEditor->verticalScrollBar();
    m_lastScrollRatio = ScrollSync::ratioFromScrollBar(
        scrollBar->value(), scrollBar->minimum(), scrollBar->maximum());
    m_lastPreviewRatio = m_lastScrollRatio;

    refreshNow();
}

void MarkdownPreviewWidget::setDocumentPath(const QString &path)
{
    if (m_documentPath == path)
        return;
    m_documentPath = path;
    if (m_sourceEditor)
        scheduleUpdate();
}

void MarkdownPreviewWidget::setReadMode(bool enabled)
{
    if (m_readMode == enabled)
        return;
    m_readMode = enabled;
    if (m_sourceEditor)
        refreshNow();
}

void MarkdownPreviewWidget::refreshNow()
{
    if (!m_sourceEditor)
        return;
    m_updatePending = false;
    performUpdate();
    if (!m_updateTimer->isActive())
        m_updateTimer->start();
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
        "(() => {"
        "const e = document.scrollingElement || document.documentElement || document.body;"
        "if (!e) return 0;"
        "const h = e.scrollHeight - e.clientHeight;"
        "return h > 0 ? (window.scrollY || e.scrollTop || 0) / h : 0;"
        "})()",
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
        QString("(() => {"
                "const e = document.scrollingElement || document.documentElement || document.body;"
                "if (!e) return;"
                "const h = e.scrollHeight - e.clientHeight;"
                "window.scrollTo(0, %1 * (h > 0 ? h : 0));"
                "})()").arg(ratio),
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
    m_lastScrollRatio = m_lastPreviewRatio;
    m_updatePending = true;
    if (!m_updateTimer->isActive()) {
        performUpdate();
        m_updateTimer->start();
    }
}

void MarkdownPreviewWidget::handleUpdateTimeout()
{
    if (!m_updatePending)
        return;
    performUpdate();
    m_updateTimer->start();
}

void MarkdownPreviewWidget::performUpdate()
{
    if (!m_sourceEditor)
        return;

    m_updatePending = false;

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
    QString renderMarkdown = markdown;
    if (!m_documentPath.isEmpty()) {
        renderMarkdown = MarkdownLogic::resolveImagePaths(
            markdown, QFileInfo(m_documentPath).absolutePath());
    }

    // 使用 QJsonDocument 正确序列化字符串
    QJsonArray a;
    a.append(QJsonValue(renderMarkdown));
    QJsonDocument doc(a); // 转换为 JSON 对象

    QString escapedMarkdown = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    // 移除 JSON 外层的引号（保留内容转义）
    if (escapedMarkdown.startsWith('[') && escapedMarkdown.endsWith(']')) {
        escapedMarkdown = escapedMarkdown.mid(2, escapedMarkdown.length() - 4);
    }

    const QString maxWidth = m_readMode ? QStringLiteral("800px") : QStringLiteral("none");
    const QString margin = m_readMode ? QStringLiteral("0 auto") : QStringLiteral("0");
    const QString padding = m_readMode ? QStringLiteral("45px") : QStringLiteral("24px");
    const QString layoutStyle = QStringLiteral(
        "<style>.markdown-body { max-width: %1; margin: %2; padding: %3; }</style>")
                                    .arg(maxWidth, margin, padding);
    QString paletteStyle;
    if (m_backgroundColor.isValid() && m_foregroundColor.isValid()) {
        const QColor codeBackground = m_backgroundColor.lightness() < 128
            ? m_backgroundColor.lighter(115) : m_backgroundColor.darker(105);
        paletteStyle = QStringLiteral(
            "<style>html, body, .markdown-body { background-color: %1; color: %2; }"
            ".markdown-body pre, .markdown-body code { background-color: %3; color: %2; }</style>")
                           .arg(m_backgroundColor.name(), m_foregroundColor.name(),
                                codeBackground.name());
    }

    const QString scrollThumb = m_darkMode
        ? QStringLiteral("rgba(255, 255, 255, 0.30)")
        : QStringLiteral("rgba(72, 72, 72, 0.32)");
    const QString scrollThumbHover = m_darkMode
        ? QStringLiteral("rgba(255, 255, 255, 0.46)")
        : QStringLiteral("rgba(72, 72, 72, 0.48)");
    const QString scrollbarStyle = QStringLiteral(R"(
        <style>
          *::-webkit-scrollbar {
            width: 10px;
            height: 10px;
          }
          *::-webkit-scrollbar-track,
          *::-webkit-scrollbar-corner {
            background: transparent;
          }
          *::-webkit-scrollbar-thumb {
            min-height: 32px;
            background-color: transparent;
            background-clip: content-box;
            border: 3px solid transparent;
            border-radius: 5px;
          }
          html.dtk-scrollbar-active::-webkit-scrollbar-thumb {
            background-color: %1;
          }
          html.dtk-scrollbar-hover::-webkit-scrollbar-thumb {
            background-color: %2;
            border-width: 1px;
          }
          *::-webkit-scrollbar-button {
            display: none;
            width: 0;
            height: 0;
          }
        </style>
    )").arg(scrollThumb, scrollThumbHover);

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

                    let scrollbarHideTimer = 0;
                    const showScrollbar = function(hovered) {
                        document.documentElement.classList.add('dtk-scrollbar-active');
                        document.documentElement.classList.toggle('dtk-scrollbar-hover', hovered);
                        window.clearTimeout(scrollbarHideTimer);
                        if (!hovered) {
                            scrollbarHideTimer = window.setTimeout(function() {
                                document.documentElement.classList.remove('dtk-scrollbar-active');
                            }, 1000);
                        }
                    };

                    window.addEventListener('scroll', function() {
                        showScrollbar(false);
                        const e = document.scrollingElement || document.documentElement || document.body;
                        if (!e) return;
                        const h = e.scrollHeight - e.clientHeight;
                        if (h > 0) {
                            bridge.syncPreviewToEditor((window.scrollY || e.scrollTop || 0) / h);
                        }
                    }, { passive: true });
                    window.addEventListener('pointermove', function(event) {
                        const atScrollbar = event.clientX >= document.documentElement.clientWidth - 12;
                        if (atScrollbar) showScrollbar(true);
                        else if (document.documentElement.classList.contains('dtk-scrollbar-hover'))
                            showScrollbar(false);
                    }, { passive: true });
                    window.addEventListener('pointerleave', function() {
                        showScrollbar(false);
                    }, { passive: true });
                });
            </script>
        </body>
        </html>
    )").arg((m_darkMode ? STYLE_DARK : STYLE_LIGHT)
                 + layoutStyle + paletteStyle + scrollbarStyle)
      .arg(escapedMarkdown); // 直接插入已转义内容
}

void MarkdownPreviewWidget::setDarkTheme(bool enabled)
{
    if (m_darkMode == enabled)
        return;
    m_darkMode = enabled;
    if (m_sourceEditor)
        refreshNow();
}

void MarkdownPreviewWidget::applyTheme(const QVariantMap &themeMap)
{
    const QVariantMap editorColors = themeMap.value(QStringLiteral("editor-colors")).toMap();
    const QVariantMap textStyles = themeMap.value(QStringLiteral("text-styles")).toMap();
    const QVariantMap normalStyle = textStyles.value(QStringLiteral("Normal")).toMap();
    const QColor background(editorColors.value(QStringLiteral("background-color")).toString());
    const QColor foreground(normalStyle.value(QStringLiteral("text-color")).toString());
    if (m_backgroundColor == background && m_foregroundColor == foreground)
        return;

    m_backgroundColor = background;
    m_foregroundColor = foreground;
    if (m_sourceEditor)
        refreshNow();
}

void MarkdownPreviewWidget::setPreviewScrollRatio(double ratio)
{
    if (!m_webView || !m_sourceEditor)
        return;

    m_lastPreviewRatio = ratio;
    m_webView->page()->runJavaScript(
        QString("(() => {"
                "const e = document.scrollingElement || document.documentElement || document.body;"
                "if (!e) return;"
                "const h = e.scrollHeight - e.clientHeight;"
                "window.scrollTo(0, %1 * (h > 0 ? h : 0));"
                "})()").arg(ratio)
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
