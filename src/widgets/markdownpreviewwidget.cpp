#include "markdownpreviewwidget.h"
#include <QVBoxLayout>
#include <QNetworkReply>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QFile>
#include <QTextStream>
#include <QScrollBar>
#include <QFileInfo>
#include <QDesktopServices>
#include <QBuffer>

MarkdownPreviewWidget::MarkdownPreviewWidget(QWidget *parent) : QWidget(parent) {
    // 设置布局
    QVBoxLayout *layout = new QVBoxLayout(this);
    browser = new QTextBrowser(this);
    layout->addWidget(browser);

    browser->setOpenLinks(false);
    browser->setOpenExternalLinks(false);

    connect(browser, &QTextBrowser::anchorClicked, this, &MarkdownPreviewWidget::openUrl);

    // 设置网络管理器用于加载网络图片
    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, &QNetworkAccessManager::finished, this, &MarkdownPreviewWidget::onImageLoaded);
}

void MarkdownPreviewWidget::openUrl(const QUrl &link)
{
    auto path = link.path();
    path.remove(0, 1);
    QFileInfo info(path);

    if(info.isDir()) {
        QDesktopServices::openUrl(link);
    }
    else if(info.isFile()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
    else {
        QDesktopServices::openUrl(link);
    }
}

void MarkdownPreviewWidget::setMarkdown(const QString &markdown) {
    // 保存当前滚动条位置
    int scrollBarValue = browser->verticalScrollBar()->value();

    // 设置HTML内容
    //browser->setHtml(htmlContent);
    browser->setMarkdown(markdown);

    // 恢复滚动条位置
    browser->verticalScrollBar()->setValue(scrollBarValue);

    // 手动高亮代码块
    highlightCodeBlocks();

    // 查找并加载网络图片
    //loadNetworkImages(browser->toHtml());
}

void MarkdownPreviewWidget::onImageLoaded(QNetworkReply *reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QImage image;
        image.loadFromData(reply->readAll());
        if (!image.isNull()) {
            // 缩放 image 防止过大超出预览框
            if (image.width() > browser->width() || image.height() > browser->height()) {
                image = image.scaled(QSize(browser->width() * 0.95,
                                           browser->height() * 0.95), Qt::KeepAspectRatio);
            }

            // 插入图片到文档
            QTextDocument *document = browser->document();
            QTextCursor cursor(document);

            QTextImageFormat imageFormat;
            imageFormat.setName(reply->url().toString());
            cursor.insertImage(image);
        }
    }
    reply->deleteLater();
}

void MarkdownPreviewWidget::highlightCodeBlocks() {
    QTextDocument *document = browser->document();
    QTextCursor cursor(document);

    // 匹配代码块
    QRegularExpression regex("<pre><code>(.*?)</code></pre>", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatchIterator matches = regex.globalMatch(document->toHtml());

    while (matches.hasNext()) {
        QRegularExpressionMatch match = matches.next();
        QString codeBlock = match.captured(1);

        // 设置代码块样式
        QTextCharFormat codeFormat;
        codeFormat.setFontFamily("Courier New");
        codeFormat.setBackground(Qt::lightGray);
        codeFormat.setForeground(Qt::darkBlue);

        // 替换代码块为高亮文本
        cursor.setPosition(match.capturedStart());
        cursor.setPosition(match.capturedEnd(), QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertText(codeBlock, codeFormat);
    }
}

void MarkdownPreviewWidget::loadNetworkImages(const QString &htmlContent) {
    QRegularExpression regex("<img[^>]+src=\"([^\">]+)\"");
    QRegularExpressionMatchIterator matches = regex.globalMatch(htmlContent);
    while (matches.hasNext()) {
        QRegularExpressionMatch match = matches.next();
        QString imageUrl = match.captured(1);
        if (imageUrl.startsWith("http")) {
            // 下载网络图片
            networkManager->get(QNetworkRequest(QUrl(imageUrl)));
        }
    }
}

QString MarkdownPreviewWidget::loadTemplate(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("Failed to open template file.");
        return "";
    }
    QTextStream in(&file);
    return in.readAll();
}
