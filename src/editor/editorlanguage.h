#ifndef EDITORLANGUAGE_H
#define EDITORLANGUAGE_H

#include <QString>

class EditorLanguage
{
public:
    enum Type {
        PlainText,
        Bash,
        Batch,
        CMake,
        Cpp,
        CSharp,
        Css,
        Diff,
        Html,
        Ini,
        Java,
        JavaScript,
        Json,
        Lua,
        Makefile,
        Markdown,
        Perl,
        Python
        ,Ruby,
        Sql,
        Xml,
        Yaml
    };

    static Type fromSyntaxDefinitionName(const QString &syntaxDefinitionName);
};

#endif
