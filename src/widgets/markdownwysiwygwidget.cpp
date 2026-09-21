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
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineUrlScheme>

namespace {

const char kSchemeName[] = "gxde-md";
const char kEditorHost[] = "editor";
const char kDocumentHost[] = "doc";
const char kEditorPageUrl[] = "gxde-md://editor/index.html";
const char kEditorResourceRoot[] = ":/markdown/wysiwyg";
const char kBridgeObjectName[] = "gxdeEditor";

QString toJavaScriptString(const QString &value) {
    QJsonArray wrapper;
    wrapper.append(value);
    const QByteArray json = QJsonDocument(wrapper).toJson(
        QJsonDocument::Compact);
    return QString::fromUtf8(json.mid(1, json.size() - 2));
}

/// 文档所在目录对应的基准 URL，形如 gxde-md://doc/tmp/notes/ 。
/// 把磁盘绝对路径直接当 URL 路径使，省掉一层编解码；空目录返回空 URL。
QString documentBaseUrl(const QString &directory) {
    if (directory.isEmpty()) {
        return QString();
    }

    QUrl url;
    url.setScheme(QString::fromLatin1(kSchemeName));
    url.setHost(QString::fromLatin1(kDocumentHost));
    url.setPath(directory.endsWith(QLatin1Char('/'))
        ? directory : directory + QLatin1Char('/'));
    return url.toString();
}

}  // namespace

void registerMarkdownWysiwygUrlScheme() {
    QWebEngineUrlScheme scheme(kSchemeName);
    // 我们的 URL 形如 gxde-md://doc/tmp/x.png：有主机名，没有端口，
    // 所以是 Host 而不是 HostAndPort —— 后者会因为没有默认端口被拒绝注册。
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
    scheme.setDefaultPort(QWebEngineUrlScheme::PortUnspecified);
    // CorsEnabled：产物里的 <script type="module"> 带了 crossorigin，而模块脚本
    // 要走 CORS 检查。LocalAccessAllowed：页面里那个指向文档目录的 <base> 之下
    // 万一还有别的本地引用，别被一并拦掉。
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme
        | QWebEngineUrlScheme::LocalAccessAllowed
        | QWebEngineUrlScheme::ViewSourceAllowed
        | QWebEngineUrlScheme::CorsEnabled);
    QWebEngineUrlScheme::registerScheme(scheme);
}

MarkdownWysiwygSchemeHandler::MarkdownWysiwygSchemeHandler(QObject *parent)
    : QWebEngineUrlSchemeHandler(parent) {}

void MarkdownWysiwygSchemeHandler::requestStarted(
        QWebEngineUrlRequestJob *job) {
    const QUrl url = job->requestUrl();
    const QString host = url.host();

    QString path;
    if (host == QLatin1String(kEditorHost)) {
        path = QString::fromLatin1(kEditorResourceRoot) + url.path();
    } else if (host == QLatin1String(kDocumentHost)) {
        // 基准 URL 里塞的就是文档目录的绝对路径，这里拿来直接用。
        path = url.path();
    } else {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }

    // 挂在 job 上，job 一销毁文件跟着走，不用自己记账。
    QFile *file = new QFile(path, job);
    if (!file->open(QIODevice::ReadOnly)) {
        qWarning() << "Markdown 所见即所得：读不到资源" << path;
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }

    static const QMimeDatabase mimeDatabase;
    const QString mimeType = mimeDatabase
        .mimeTypeForFile(path, QMimeDatabase::MatchExtension).name();
    job->reply(mimeType.toUtf8(), file);
}

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

    // 得赶在 setUrl() 之前挂上，否则首次加载就走不到我们的处理器。
    m_schemeHandler = new MarkdownWysiwygSchemeHandler(this);
    m_webView->page()->profile()->installUrlSchemeHandler(
        QByteArray(kSchemeName), m_schemeHandler);

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

void MarkdownWysiwygWidget::setDocumentPath(const QString &path) {
    const QString directory = path.isEmpty()
        ? QString()
        : QFileInfo(path).absolutePath();

    if (m_documentDirectory == directory) {
        return;
    }

    m_documentDirectory = directory;

    // 已经挂到页面上的 <img> 不会因为换了基准就自己重算，只能让编辑器重画一遍。
    if (m_ready && !m_markdown.isEmpty()) {
        m_pendingMarkdown = m_markdown;
        m_hasPendingMarkdown = true;
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

    // 基准目录和文档内容拼成一条脚本：基准只对之后解析的 URL 生效，
    // 分成两次 runJavaScript 就赌上了两条脚本的执行次序。
    QStringList script;
    const QString baseUrl = documentBaseUrl(m_documentDirectory);
    if (!baseUrl.isEmpty()) {
        script << QStringLiteral("window.gxdeEditor.setBaseUrl(%1)")
            .arg(toJavaScriptString(baseUrl));
    }
    script << QStringLiteral("window.gxdeEditor.load(%1)")
        .arg(toJavaScriptString(m_pendingMarkdown));

    runScript(script.join(QLatin1Char(';')));
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
