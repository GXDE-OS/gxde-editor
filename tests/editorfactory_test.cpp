#define private public
#define protected public
#include "../src/editor/abstracteditor.h"
#include "../src/editor/editorfactory.h"
#include "../src/editor/legacytexteditor.h"
#include "../src/editor/scintillaeditor.h"
#include "../src/editwrapper.h"
#include "../src/tabbar.h"
#include "../src/window.h"

#undef protected
#undef private

#include <QApplication>
#include <QMimeData>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QString>
#include <QObject>
#include <QTest>
#include <QStyleOptionTab>
#include <Qsci/qsciscintillabase.h>
#include <Qsci/qscilexercpp.h>
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
    void scintillaEditorSupportsRenderedSelectionsBulkLoadAndLexerLoading();
    void scintillaEditorFindNavigationPreservesAdvancedMatchAndHighlightsAllMatches();
    void fallsBackToLegacyEditorForUnknownEngine();
    void editWrapperUsesScintillaFactoryBackendByDefault();
    void editWrapperToleratesNonLegacyBackendWithoutLegacyTextEditor();
    void tabbarMimeTransferCapturesWrapperFilePathForScintillaEditors();
    void scintillaWrapperIncrementallyLoadsLargeFiles();
    void scintillaEditorSkipsLexerForLargeDocuments();
    void scintillaRefreshUsesIncrementalReloadPath();
    void scintillaRefreshReloadsHighlighter();
    void scintillaBottomBarTracksScintillaState();
    void windowCanPrintScintillaEditorToPdf();
    void windowDisconnectEditorSignalsHandlesLegacyAndScintillaBackends();
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

void EditorFactoryTest::scintillaEditorSupportsRenderedSelectionsBulkLoadAndLexerLoading()
{
    ScintillaEditor editor;
    QsciScintilla *widget = qobject_cast<QsciScintilla *>(editor.widget());

    QVERIFY(widget != nullptr);

    QStringList lines;
    for (int i = 0; i < 80; ++i) {
        lines << QStringLiteral("line %1").arg(i);
    }

    lines[2] = QStringLiteral("needle line");
    editor.setText(lines.join(QStringLiteral("\n")));
    widget->setFirstVisibleLine(50);

    editor.highlightKeyword(QStringLiteral("needle"), 0);
    editor.renderAllSelections();
    QVERIFY2(widget->firstVisibleLine() < 50,
             "renderAllSelections should bring the active search result back into view.");

    QVERIFY(widget->updatesEnabled());
    editor.beginBulkLoad();
    QVERIFY2(!widget->updatesEnabled(),
             "beginBulkLoad should suspend widget updates during large text loads.");
    editor.endBulkLoad();
    QVERIFY2(widget->updatesEnabled(),
             "endBulkLoad should restore widget updates after loading.");

    widget->setProperty("filepath", QStringLiteral("/tmp/example.cpp"));
    editor.loadHighlighter();
    QVERIFY2(qobject_cast<QsciLexerCPP *>(widget->lexer()) != nullptr,
             "loadHighlighter should attach a QScintilla lexer that matches the current file type.");
}

void EditorFactoryTest::scintillaEditorFindNavigationPreservesAdvancedMatchAndHighlightsAllMatches()
{
    ScintillaEditor editor;
    QsciScintilla *widget = qobject_cast<QsciScintilla *>(editor.widget());

    QVERIFY(widget != nullptr);

    editor.setText(QStringLiteral("beta alpha beta alpha beta"));
    editor.highlightKeyword(QStringLiteral("beta"), 0);

    const int firstSelectionColumn = editor.currentColumn();

    editor.saveMarkStatus();
    editor.updateCursorKeywordSelection(editor.cursorPosition(), true);
    editor.renderAllSelections();
    editor.restoreMarkStatus();

    QVERIFY2(editor.currentColumn() > firstSelectionColumn,
             "Find Next should keep the advanced search match instead of restoring the old one.");

    const long firstIndicators = widget->SendScintilla(QsciScintillaBase::SCI_INDICATORALLONFOR, 0);
    const long secondIndicators = widget->SendScintilla(QsciScintillaBase::SCI_INDICATORALLONFOR, 11);
    const long thirdIndicators = widget->SendScintilla(QsciScintillaBase::SCI_INDICATORALLONFOR, 22);

    QVERIFY2(firstIndicators != 0 && secondIndicators != 0 && thirdIndicators != 0,
             "highlightKeyword should mark every search match, not only the active one.");
}

void EditorFactoryTest::fallsBackToLegacyEditorForUnknownEngine()
{
    std::unique_ptr<AbstractEditor> editor = EditorFactory::create(QStringLiteral("unknown"));

    QVERIFY2(dynamic_cast<LegacyTextEditor *>(editor.get()) != nullptr,
             "EditorFactory should fall back to LegacyTextEditor for unknown engine names.");
}

void EditorFactoryTest::editWrapperUsesScintillaFactoryBackendByDefault()
{
    EditWrapper wrapper;

    QVERIFY2(dynamic_cast<ScintillaEditor *>(wrapper.editorBackend()) != nullptr,
             "EditWrapper should expose the default ScintillaEditor backend created through EditorFactory.");
    QVERIFY(wrapper.textEditor() == nullptr);
    QCOMPARE(wrapper.editorBackend()->widget(), wrapper.editorWidget());
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

void EditorFactoryTest::tabbarMimeTransferCapturesWrapperFilePathForScintillaEditors()
{
    Window sourceWindow(nullptr);
    Window targetWindow(nullptr);

    std::unique_ptr<AbstractEditor> backend(new ScintillaEditor());
    EditWrapper *wrapper = new EditWrapper(std::move(backend));
    const QString path = QStringLiteral("/tmp/scintilla-transfer.cpp");
    const QString tabName = QStringLiteral("scintilla-transfer.cpp");

    wrapper->updatePath(path);
    sourceWindow.addTabWithWrapper(wrapper, path, tabName, 0);

    Tabbar *sourceTabbar = sourceWindow.findChild<Tabbar *>();
    Tabbar *targetTabbar = targetWindow.findChild<Tabbar *>();
    QVERIFY(sourceTabbar != nullptr);
    QVERIFY(targetTabbar != nullptr);

    QStyleOptionTab option;
    std::unique_ptr<QMimeData> mimeData(sourceTabbar->createMimeDataFromTab(0, option));
    QVERIFY(mimeData != nullptr);
    QCOMPARE(static_cast<EditWrapper *>(mimeData->property("wrapper").value<void *>()), wrapper);
    QCOMPARE(mimeData->property("filepath").toString(), path);

    QPoint hotspot;
    QVERIFY(!sourceTabbar->createDragPixmapFromTab(0, option, &hotspot).isNull());

    targetTabbar->insertFromMimeData(0, mimeData.get());
    QCOMPARE(targetWindow.wrapper(path), wrapper);
    QCOMPARE(targetWindow.getEditorWidget(path), wrapper->editorWidget());

    sourceWindow.removeWrapper(path, false);
    targetWindow.removeWrapper(path, true);
}

void EditorFactoryTest::scintillaWrapperIncrementallyLoadsLargeFiles()
{
    EditWrapper wrapper(std::unique_ptr<AbstractEditor>(new ScintillaEditor()));
    QString content;

    while (content.size() <= (2 * 1024 * 1024)) {
        content.append(QStringLiteral("0123456789abcdef\n"));
    }

    wrapper.updatePath(QStringLiteral("/tmp/large-scintilla.cpp"));
    wrapper.m_isLoadFinished = false;
    wrapper.handleFileLoadFinished(QByteArrayLiteral("UTF-8"), content);

    QVERIFY2(!wrapper.isLoadFinished(),
             "Scintilla-backed wrappers should enter the incremental large-file load path.");
    QVERIFY(wrapper.m_pendingLoadTimer->isActive());

    QTRY_VERIFY(wrapper.isLoadFinished());
    QCOMPARE(wrapper.editorBackend()->text(), content);
}

void EditorFactoryTest::scintillaEditorSkipsLexerForLargeDocuments()
{
    ScintillaEditor editor;
    QsciScintilla *widget = qobject_cast<QsciScintilla *>(editor.widget());
    QString content;

    QVERIFY(widget != nullptr);

    while (content.size() <= (1024 * 1024)) {
        content.append(QStringLiteral("int value = 42;\n"));
    }

    widget->setProperty("filepath", QStringLiteral("/tmp/large-file.cpp"));
    editor.setText(content);
    editor.loadHighlighter();

    QVERIFY2(widget->lexer() == nullptr,
             "Scintilla should skip attaching a lexer when the document is large enough to require the safe path.");
}

void EditorFactoryTest::scintillaRefreshUsesIncrementalReloadPath()
{
    EditWrapper wrapper(std::unique_ptr<AbstractEditor>(new ScintillaEditor()));
    QTemporaryFile file;
    QString content;

    QVERIFY(file.open());
    while (content.size() <= (2 * 1024 * 1024)) {
        content.append(QStringLiteral("0123456789abcdef\n"));
    }

    file.write(content.toUtf8());
    file.flush();

    wrapper.updatePath(file.fileName());
    wrapper.refresh();

    QVERIFY2(!wrapper.isLoadFinished(),
             "Refreshing a large Scintilla document should reuse the incremental safe-load path.");
    QVERIFY(wrapper.m_pendingLoadTimer->isActive());

    QTRY_VERIFY(wrapper.isLoadFinished());
    QCOMPARE(wrapper.editorBackend()->text(), content);
}

void EditorFactoryTest::scintillaRefreshReloadsHighlighter()
{
    EditWrapper wrapper(std::unique_ptr<AbstractEditor>(new ScintillaEditor()));
    QTemporaryFile file(QDir::tempPath() + QStringLiteral("/refresh-highlighter-XXXXXX.cpp"));
    QsciScintilla *widget = qobject_cast<QsciScintilla *>(wrapper.editorWidget());

    QVERIFY(widget != nullptr);
    QVERIFY(file.open());
    file.write("int main() { return 0; }\n");
    file.flush();

    wrapper.updatePath(file.fileName());
    wrapper.refresh();

    QTRY_VERIFY(widget->lexer() != nullptr);
    QVERIFY2(qobject_cast<QsciLexerCPP *>(widget->lexer()) != nullptr,
             "Refreshing a Scintilla-backed C++ file should re-apply the matching lexer.");
}

void EditorFactoryTest::scintillaBottomBarTracksScintillaState()
{
    EditWrapper wrapper(std::unique_ptr<AbstractEditor>(new ScintillaEditor()));
    QsciScintilla *widget = qobject_cast<QsciScintilla *>(wrapper.editorWidget());

    QVERIFY(widget != nullptr);

    wrapper.show();
    wrapper.updatePath(QStringLiteral("/tmp/bottombar.cpp"));
    wrapper.m_isLoadFinished = false;
    wrapper.handleFileLoadFinished(QByteArrayLiteral("UTF-8"), QStringLiteral("alpha\nbeta"));
    QTRY_VERIFY(wrapper.isLoadFinished());
    widget->setFocus();
    QTest::qWait(0);
    QTest::keyClick(widget, Qt::Key_Down);
    QTest::keyClick(widget, Qt::Key_Right);
    QTest::keyClick(widget, Qt::Key_Right);
    qApp->processEvents();
    QTRY_COMPARE(wrapper.bottomBar()->m_highlightMenu->m_text->text(), QStringLiteral("C++"));

    QCOMPARE(wrapper.bottomBar()->m_charCountLabel->text(), QStringLiteral("Characters 10"));
    QCOMPARE(wrapper.bottomBar()->m_positionLabel->text(), QStringLiteral("Row 2 , Column 3"));
    QCOMPARE(wrapper.bottomBar()->m_cursorStatus->text(), QStringLiteral("INSERT"));
}

void EditorFactoryTest::windowCanPrintScintillaEditorToPdf()
{
    Window window(nullptr);
    EditWrapper *wrapper = new EditWrapper(std::unique_ptr<AbstractEditor>(new ScintillaEditor()));
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("printable.cpp"));
    const QString outputPath = dir.filePath(QStringLiteral("printable.pdf"));

    QVERIFY(dir.isValid());

    wrapper->updatePath(path);
    wrapper->editorBackend()->setText(QStringLiteral("int main() { return 0; }\n"));
    window.addTabWithWrapper(wrapper, path, QStringLiteral("printable.cpp"), 0);

    QVERIFY2(window.printEditorToPdf(wrapper, outputPath),
             "Window should print Scintilla-backed tabs to PDF instead of bailing out.");
    QVERIFY(QFileInfo(outputPath).exists());
    QVERIFY(QFileInfo(outputPath).size() > 0);

    window.removeWrapper(path, true);
}

void EditorFactoryTest::windowDisconnectEditorSignalsHandlesLegacyAndScintillaBackends()
{
    EditWrapper legacyWrapper(std::unique_ptr<AbstractEditor>(new LegacyTextEditor()));
    EditWrapper scintillaWrapper(std::unique_ptr<AbstractEditor>(new ScintillaEditor()));
    QObject receiver;
    int legacyCount = 0;
    int scintillaCount = 0;

    QVERIFY(QObject::connect(legacyWrapper.textEditor(), &DTextEdit::modificationChanged, &receiver,
                             [&legacyCount] (const QString &, bool) { ++legacyCount; }));

    QsciScintilla *scintillaEditor = qobject_cast<QsciScintilla *>(scintillaWrapper.editorWidget());
    QVERIFY(scintillaEditor != nullptr);
    QVERIFY(QObject::connect(scintillaEditor, &QsciScintilla::modificationChanged, &receiver,
                             [&scintillaCount] (bool) { ++scintillaCount; }));

    QVERIFY(QMetaObject::invokeMethod(legacyWrapper.textEditor(), "modificationChanged",
                                      Qt::DirectConnection,
                                      Q_ARG(QString, QStringLiteral("/tmp/file.txt")),
                                      Q_ARG(bool, true)));
    QVERIFY(QMetaObject::invokeMethod(scintillaEditor, "modificationChanged",
                                      Qt::DirectConnection,
                                      Q_ARG(bool, true)));
    QCOMPARE(legacyCount, 1);
    QCOMPARE(scintillaCount, 1);

    Window::disconnectEditorSignals(&legacyWrapper, &receiver);
    Window::disconnectEditorSignals(&scintillaWrapper, &receiver);

    QVERIFY(QMetaObject::invokeMethod(legacyWrapper.textEditor(), "modificationChanged",
                                      Qt::DirectConnection,
                                      Q_ARG(QString, QStringLiteral("/tmp/file.txt")),
                                      Q_ARG(bool, false)));
    QVERIFY(QMetaObject::invokeMethod(scintillaEditor, "modificationChanged",
                                      Qt::DirectConnection,
                                      Q_ARG(bool, false)));
    QCOMPARE(legacyCount, 1);
    QCOMPARE(scintillaCount, 1);
}

void EditorFactoryTest::windowLegacyTextEditorHelperHandlesNonLegacyWrappers()
{
    EditWrapper legacyWrapper(std::unique_ptr<AbstractEditor>(new LegacyTextEditor()));
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
