#ifndef MARKDOWNPREVIEWWIDGET_H
#define MARKDOWNPREVIEWWIDGET_H

#include <QWidget>
#include <QTextBrowser>
#include <QNetworkAccessManager>

class MarkdownPreviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit MarkdownPreviewWidget(QWidget *parent = nullptr);
    void setMarkdown(const QString &markdown);

private slots:
    void onImageLoaded(QNetworkReply *reply);

private:
    void highlightCodeBlocks();
    void loadNetworkImages(const QString &htmlContent);
    QString loadTemplate(const QString &path);
    void openUrl(const QUrl &link);

    QTextBrowser *browser;
    QNetworkAccessManager *networkManager;
};

#endif // MARKDOWNPREVIEWWIDGET_H
