#include "../src/editor/editorlanguage.h"
#include "../src/editor/abstracteditor.h"
#include "../src/editor/legacytexteditor.h"

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
static_assert(!std::is_copy_constructible<LegacyTextEditor>::value,
              "LegacyTextEditor must not be copy-constructible because it owns a DTextEdit instance.");
static_assert(!std::is_copy_assignable<LegacyTextEditor>::value,
              "LegacyTextEditor must not be copy-assignable because it owns a DTextEdit instance.");
static_assert(std::is_constructible<LegacyTextEditor>::value,
              "LegacyTextEditor should remain default-constructible.");
static_assert(!std::is_constructible<LegacyTextEditor, QWidget *>::value,
              "LegacyTextEditor should not advertise QWidget parent construction until it honors Qt parent ownership semantics.");

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
