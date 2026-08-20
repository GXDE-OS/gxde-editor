#ifndef MARKDOWNLOGIC_H
#define MARKDOWNLOGIC_H

#include <QDir>
#include <QMetaType>
#include <QRegularExpression>
#include <QString>
#include <QStringView>
#include <QUrl>
#include <QtGlobal>

enum class ViewMode {
    Edit,
    ReadView,
    LivePreview
};

Q_DECLARE_METATYPE(ViewMode)

class ViewModeFsm
{
public:
    static ViewMode resolveDefaultMode(bool isMarkdown, bool previewAvailable)
    {
        return isMarkdown && previewAvailable ? ViewMode::LivePreview : ViewMode::Edit;
    }

    static bool canSwitchTo(ViewMode target, bool isMarkdown, bool previewAvailable)
    {
        if (target == ViewMode::LivePreview)
            return isMarkdown && previewAvailable;
        return true;
    }

    static ViewMode fallbackWhenMarkdownLost(ViewMode current)
    {
        return current == ViewMode::LivePreview ? ViewMode::Edit : current;
    }

    static ViewMode elevateWhenMarkdownGained(ViewMode current, bool previewAvailable)
    {
        if (current == ViewMode::Edit && previewAvailable)
            return ViewMode::LivePreview;
        return current;
    }

    static bool isReadOnlyTextMode(ViewMode mode, bool isMarkdown, bool previewAvailable)
    {
        return mode == ViewMode::ReadView && (!isMarkdown || !previewAvailable);
    }
};

class MarkdownLogic
{
public:
    static bool isMarkdownByDefinitionName(const QString &definitionName)
    {
        return definitionName == QStringLiteral("Markdown");
    }

    static bool isMarkdownByFileName(const QString &fileName)
    {
        const QString lower = fileName.toLower();
        return lower.endsWith(QStringLiteral(".md"))
            || lower.endsWith(QStringLiteral(".markdown"))
            || lower.endsWith(QStringLiteral(".mdown"));
    }

    static bool isMarkdown(const QString &fileName, const QString &definitionName)
    {
        return isMarkdownByDefinitionName(definitionName) || isMarkdownByFileName(fileName);
    }

    static QString resolveImagePaths(const QString &markdown, const QString &baseDir)
    {
        if (markdown.isEmpty() || baseDir.isEmpty())
            return markdown;

        static const QRegularExpression imageExpression(
            QStringLiteral("!\\[([^\\]]*)\\]\\(([^)]+?)(\\s+\"[^\"]*\")?\\)"));

        QString result;
        qsizetype last = 0;
        QRegularExpressionMatchIterator matches = imageExpression.globalMatch(markdown);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            result += QStringView(markdown).mid(last, match.capturedStart() - last);

            const QString path = match.captured(2).trimmed();
            QString resolved = path;
            const QUrl url(path);
            if (url.scheme().isEmpty()) {
                resolved = QUrl::fromLocalFile(QDir(baseDir).absoluteFilePath(path))
                               .toString(QUrl::FullyEncoded);
            }

            result += QStringLiteral("![") + match.captured(1) + QStringLiteral("](")
                    + resolved + match.captured(3) + QStringLiteral(")");
            last = match.capturedEnd();
        }
        result += QStringView(markdown).mid(last);
        return result;
    }
};

class ScrollSync
{
public:
    static double clampRatio(double ratio)
    {
        return qBound(0.0, ratio, 1.0);
    }

    static double ratioFromScrollBar(int value, int minimum, int maximum)
    {
        if (maximum <= minimum)
            return 0.0;
        return clampRatio(double(value - minimum) / double(maximum - minimum));
    }
};

#endif
