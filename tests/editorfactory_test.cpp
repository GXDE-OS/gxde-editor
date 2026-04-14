#include "../src/editor/abstracteditor.h"
#include "../src/editor/editorfactory.h"
#include "../src/editor/legacytexteditor.h"
#include "../src/editor/scintillaeditor.h"
#include "../src/editwrapper.h"
#include "../src/window.h"

#include <QApplication>
#include <QString>
#include <QObject>
#include <QTest>
#include <Qsci/qsciscintilla.h>

#include <memory>
#include <type_traits>

static_assert(std::is_base_of<AbstractEditor, ScintillaEditor>::value,
              "ScintillaEditor must implement the AbstractEditor interface.");

class EditorFactoryTest : public QObject
{
    Q_OBJECT

private slots:
    void createsScintillaEditorBackend();
    void scintillaEditorImplementsCoreEditingPrimitives();
    void fallsBackToLegacyEditorForUnknownEngine();
    void editWrapperUsesLegacyFactoryBackendByDefault();
    void editWrapperToleratesNonLegacyBackendWithoutLegacyTextEditor();
    void windowLegacyTextEditorHelperHandlesNonLegacyWrappers();
};

void EditorFactoryTest::createsScintillaEditorBackend()
{
    std::unique_ptr<AbstractEditor> editor = EditorFactory::create(QStringLiteral("scintilla"));

    QVERIFY2(dynamic_cast<ScintillaEditor *>(editor.get()) != nullptr,
             "EditorFactory should create a ScintillaEditor when the Scintilla backend is requested.");
    QVERIFY2(dynamic_cast<QsciScintilla *>(editor->widget()) != nullptr,
             "ScintillaEditor must be backed by a QsciScintilla widget.");

    editor->setText(QStringLiteral("hello"));
    QCOMPARE(editor->text(), QStringLiteral("hello"));

    editor->setReadOnly(true);
    QVERIFY(editor->isReadOnly());
}

void EditorFactoryTest::scintillaEditorImplementsCoreEditingPrimitives()
{
    ScintillaEditor editor;

    editor.setText(QStringLiteral("alpha beta\nalpha beta"));
    editor.setSelection(0, 0, 0, 5);
    QCOMPARE(editor.selectedText(), QStringLiteral("alpha"));

    QCOMPARE(editor.currentLine(), 1);
    QCOMPARE(editor.currentColumn(), 5);
    QCOMPARE(editor.lineCount(), 2);

    editor.highlightKeyword(QStringLiteral("beta"), editor.cursorPosition());
    QCOMPARE(editor.selectedText(), QStringLiteral("beta"));

    editor.replaceNext(QStringLiteral("beta"), QStringLiteral("gamma"));
    QCOMPARE(editor.text(), QStringLiteral("alpha gamma\nalpha beta"));

    editor.replaceAll(QStringLiteral("alpha"), QStringLiteral("omega"));
    QCOMPARE(editor.text(), QStringLiteral("omega gamma\nomega beta"));

    editor.jumpToLine(2, true);
    QCOMPARE(editor.currentLine(), 2);

    editor.scrollToLine(0, 1, 2);
    QCOMPARE(editor.currentLine(), 1);
    QCOMPARE(editor.currentColumn(), 2);
}

void EditorFactoryTest::fallsBackToLegacyEditorForUnknownEngine()
{
    std::unique_ptr<AbstractEditor> editor = EditorFactory::create(QStringLiteral("unknown"));

    QVERIFY2(dynamic_cast<LegacyTextEditor *>(editor.get()) != nullptr,
             "EditorFactory should fall back to LegacyTextEditor for unknown engine names.");
}

void EditorFactoryTest::editWrapperUsesLegacyFactoryBackendByDefault()
{
    EditWrapper wrapper;

    QVERIFY2(dynamic_cast<LegacyTextEditor *>(wrapper.editorBackend()) != nullptr,
             "EditWrapper should expose the default LegacyTextEditor backend created through EditorFactory.");
    QCOMPARE(wrapper.editorBackend()->widget(), static_cast<QWidget *>(wrapper.textEditor()));
}

void EditorFactoryTest::editWrapperToleratesNonLegacyBackendWithoutLegacyTextEditor()
{
    EditWrapper wrapper(std::unique_ptr<AbstractEditor>(new ScintillaEditor()));
    const QString path = QStringLiteral("/tmp/nonlegacy-editor.cpp");

    QVERIFY2(dynamic_cast<QsciScintilla *>(wrapper.editorWidget()) != nullptr,
             "EditWrapper should host the non-legacy backend widget directly.");
    QVERIFY(wrapper.textEditor() == nullptr);

    wrapper.editorBackend()->setText(QStringLiteral("int main() {}\n"));
    wrapper.updatePath(path);

    QCOMPARE(wrapper.filePath(), path);
    QVERIFY(wrapper.saveFile());

    wrapper.refresh();
    wrapper.checkForReload();

    QFile::remove(path);
}

void EditorFactoryTest::windowLegacyTextEditorHelperHandlesNonLegacyWrappers()
{
    EditWrapper legacyWrapper;
    EditWrapper scintillaWrapper(std::unique_ptr<AbstractEditor>(new ScintillaEditor()));

    QVERIFY(Window::legacyTextEditor(&legacyWrapper) != nullptr);
    QVERIFY(Window::legacyTextEditor(&scintillaWrapper) == nullptr);
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));

    QApplication app(argc, argv);
    EditorFactoryTest test;

    return QTest::qExec(&test, argc, argv);
}

#include "editorfactory_test.moc"
