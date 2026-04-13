#include "../src/editor/abstracteditor.h"
#include "../src/editor/editorfactory.h"
#include "../src/editor/legacytexteditor.h"
#include "../src/editor/scintillaeditor.h"

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
    void fallsBackToLegacyEditorForUnknownEngine();
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

void EditorFactoryTest::fallsBackToLegacyEditorForUnknownEngine()
{
    std::unique_ptr<AbstractEditor> editor = EditorFactory::create(QStringLiteral("unknown"));

    QVERIFY2(dynamic_cast<LegacyTextEditor *>(editor.get()) != nullptr,
             "EditorFactory should fall back to LegacyTextEditor for unknown engine names.");
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));

    QApplication app(argc, argv);
    EditorFactoryTest test;

    return QTest::qExec(&test, argc, argv);
}

#include "editorfactory_test.moc"
