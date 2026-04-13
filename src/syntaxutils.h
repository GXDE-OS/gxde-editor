#ifndef SYNTAXUTILS_H
#define SYNTAXUTILS_H

#include <QString>
#include <KF5/KSyntaxHighlighting/KSyntaxHighlighting/definition.h>
#include <KF5/KSyntaxHighlighting/KSyntaxHighlighting/repository.h>

class SyntaxUtils
{
public:
    static QString detectSyntaxDefinitionName(const KSyntaxHighlighting::Repository &repository,
                                              const QString &filePath,
                                              const QString &content = QString());
    static bool shouldDeferSyntaxHighlight(int characterCount);
    static bool shouldLoadTextIncrementally(int characterCount);
    static int incrementalTextLoadChunkSize();
    static int syntaxHighlightBatchSize(int characterCount);
    static int syntaxHighlightIntervalMs(int characterCount);
};

#endif
