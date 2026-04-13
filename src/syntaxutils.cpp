#include "syntaxutils.h"

#include <QFileInfo>
#include <QMimeDatabase>

namespace {
static const int kDeferredSyntaxHighlightThreshold = 1024 * 1024;
}

QString SyntaxUtils::detectSyntaxDefinitionName(const KSyntaxHighlighting::Repository &repository,
                                                const QString &filePath,
                                                const QString &content)
{
    const QString fileName = QFileInfo(filePath).fileName();
    const auto fileNameDefinition = repository.definitionForFileName(fileName);

    if (fileNameDefinition.isValid()) {
        return fileNameDefinition.name();
    }

    if (content.isEmpty()) {
        return QString();
    }

    QMimeDatabase mimeDatabase;
    const QString mimeType = mimeDatabase.mimeTypeForFileNameAndData(fileName, content.left(4096).toUtf8()).name();
    const auto mimeDefinition = repository.definitionForMimeType(mimeType);

    if (mimeDefinition.isValid()) {
        return mimeDefinition.name();
    }

    return QString();
}

bool SyntaxUtils::shouldDeferSyntaxHighlight(int characterCount)
{
    return characterCount >= kDeferredSyntaxHighlightThreshold;
}
