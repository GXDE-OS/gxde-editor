#include "../src/editor/editorlanguage.h"
#include "../src/editor/abstracteditor.h"

#include <QObject>
#include <QTest>

#include <type_traits>

template<typename T>
class HasLanguageGetter
{
private:
    template<typename U>
    static auto test(int) -> decltype(std::declval<const U &>().language(), std::true_type());

    template<typename>
    static std::false_type test(...);

public:
    static const bool value = decltype(test<T>(0))::value;
};

template<typename T>
class HasLanguageSetter
{
private:
    template<typename U>
    static auto test(int) -> decltype(std::declval<U &>().setLanguage(EditorLanguage::PlainText), std::true_type());

    template<typename>
    static std::false_type test(...);

public:
    static const bool value = decltype(test<T>(0))::value;
};

static_assert(!HasLanguageGetter<AbstractEditor>::value,
              "AbstractEditor should not expose language() before language state is wired through the backend.");
static_assert(!HasLanguageSetter<AbstractEditor>::value,
              "AbstractEditor should not expose setLanguage() before language changes are applied to the backend.");

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
