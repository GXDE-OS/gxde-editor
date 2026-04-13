#ifndef EDITORLANGUAGE_H
#define EDITORLANGUAGE_H

#include <QString>

class EditorLanguage
{
public:
    enum Type {
        PlainText,
        Cpp,
        Markdown,
        Python
    };

    static Type fromSyntaxDefinitionName(const QString &syntaxDefinitionName);
};

#endif
