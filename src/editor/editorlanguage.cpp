#include "editorlanguage.h"

namespace {

EditorLanguage::Type mapSyntaxDefinitionName(const QString &syntaxDefinitionName)
{
    if (syntaxDefinitionName.compare(QStringLiteral("C++"), Qt::CaseInsensitive) == 0) {
        return EditorLanguage::Cpp;
    }

    if (syntaxDefinitionName.compare(QStringLiteral("Markdown"), Qt::CaseInsensitive) == 0) {
        return EditorLanguage::Markdown;
    }

    if (syntaxDefinitionName.compare(QStringLiteral("Python"), Qt::CaseInsensitive) == 0) {
        return EditorLanguage::Python;
    }

    return EditorLanguage::PlainText;
}

}

EditorLanguage::Type EditorLanguage::fromSyntaxDefinitionName(const QString &syntaxDefinitionName)
{
    return mapSyntaxDefinitionName(syntaxDefinitionName);
}
