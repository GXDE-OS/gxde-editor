#include "../src/editor/editorlanguage.h"

#include <QObject>
#include <QTest>

class EditorLanguageTest : public QObject
{
    Q_OBJECT

private slots:
    void mapsKnownSyntaxDefinitionNames();
    void fallsBackToPlainTextForUnknownDefinitions();
};

void EditorLanguageTest::mapsKnownSyntaxDefinitionNames()
{
    QCOMPARE(EditorLanguage::fromSyntaxDefinitionName(QStringLiteral("Python")), EditorLanguage::Python);
}

void EditorLanguageTest::fallsBackToPlainTextForUnknownDefinitions()
{
    QCOMPARE(EditorLanguage::fromSyntaxDefinitionName(QStringLiteral("Not a real syntax")), EditorLanguage::PlainText);
    QCOMPARE(EditorLanguage::fromSyntaxDefinitionName(QString()), EditorLanguage::PlainText);
}

QTEST_APPLESS_MAIN(EditorLanguageTest)

#include "editorlanguage_test.moc"
