#include "editorlanguage.h"

namespace {

bool matches(const QString &syntaxDefinitionName, std::initializer_list<const char *> aliases)
{
    for (const char *alias : aliases) {
        if (syntaxDefinitionName.compare(QString::fromLatin1(alias), Qt::CaseInsensitive) == 0) {
            return true;
        }
    }

    return false;
}

EditorLanguage::Type mapSyntaxDefinitionName(const QString &syntaxDefinitionName)
{
    if (matches(syntaxDefinitionName, {"Bash", "Zsh", "Fish", "POSIX Shell", "Shell Script"})) {
        return EditorLanguage::Bash;
    }

    if (matches(syntaxDefinitionName, {"Batchfile", "Batch"})) {
        return EditorLanguage::Batch;
    }

    if (matches(syntaxDefinitionName, {"CMake"})) {
        return EditorLanguage::CMake;
    }

    if (matches(syntaxDefinitionName, {"C++", "C", "Objective-C", "Objective-C++"})) {
        return EditorLanguage::Cpp;
    }

    if (matches(syntaxDefinitionName, {"C#"})) {
        return EditorLanguage::CSharp;
    }

    if (matches(syntaxDefinitionName, {"CSS"})) {
        return EditorLanguage::Css;
    }

    if (matches(syntaxDefinitionName, {"Diff", "Unified Diff"})) {
        return EditorLanguage::Diff;
    }

    if (matches(syntaxDefinitionName, {"HTML", "PHP (HTML)", "Handlebars", "Mustache", "React (JSX)", "React (TypeScript)"})) {
        return EditorLanguage::Html;
    }

    if (matches(syntaxDefinitionName, {"INI Files", "Configuration"})) {
        return EditorLanguage::Ini;
    }

    if (matches(syntaxDefinitionName, {"Java"})) {
        return EditorLanguage::Java;
    }

    if (matches(syntaxDefinitionName, {"JavaScript", "TypeScript", "QML"})) {
        return EditorLanguage::JavaScript;
    }

    if (matches(syntaxDefinitionName, {"JSON", "JSON5"})) {
        return EditorLanguage::Json;
    }

    if (matches(syntaxDefinitionName, {"Lua"})) {
        return EditorLanguage::Lua;
    }

    if (matches(syntaxDefinitionName, {"Makefile", "GNU Make"})) {
        return EditorLanguage::Makefile;
    }

    if (matches(syntaxDefinitionName, {"Markdown"})) {
        return EditorLanguage::Markdown;
    }

    if (matches(syntaxDefinitionName, {"Perl"})) {
        return EditorLanguage::Perl;
    }

    if (matches(syntaxDefinitionName, {"Python"})) {
        return EditorLanguage::Python;
    }

    if (matches(syntaxDefinitionName, {"Ruby"})) {
        return EditorLanguage::Ruby;
    }

    if (matches(syntaxDefinitionName, {"SQL", "MySQL"})) {
        return EditorLanguage::Sql;
    }

    if (matches(syntaxDefinitionName, {"XML", "XSLT"})) {
        return EditorLanguage::Xml;
    }

    if (matches(syntaxDefinitionName, {"YAML", "YAML Anchor"})) {
        return EditorLanguage::Yaml;
    }

    return EditorLanguage::PlainText;
}

}

EditorLanguage::Type EditorLanguage::fromSyntaxDefinitionName(const QString &syntaxDefinitionName)
{
    return mapSyntaxDefinitionName(syntaxDefinitionName);
}
