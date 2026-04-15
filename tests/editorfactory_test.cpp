#define private public
#define protected public
#include "../src/editor/abstracteditor.h"
#include "../src/editor/editorfactory.h"
#include "../src/editor/legacytexteditor.h"
#include "../src/editor/scintillaeditor.h"
#include "../src/editwrapper.h"
#include "../src/tabbar.h"
#include "../src/utils.h"
#include "../src/window.h"

#undef protected
#undef private

#include <QApplication>
#include <QFile>
#include <QMimeData>
#include <QTemporaryDir>
#include <QShortcut>
#include <QTemporaryFile>
#include <QString>
#include <QObject>
#include <QTest>
#include <QStyleOptionTab>
#include <DSettingsOption>
#include <Qsci/qsciscintillabase.h>
#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexerbash.h>
#include <Qsci/qscilexerhtml.h>
#include <Qsci/qscilexerjson.h>
#include <Qsci/qscilexerpython.h>
#include <Qsci/qsciscintilla.h>
#ifdef USE_WEBENGINE
#include "../src/widgets/markdownpreviewwidget.h"
#endif

#include <memory>
#include <type_traits>

static_assert(std::is_base_of<AbstractEditor, ScintillaEditor>::value,
              "ScintillaEditor must implement the AbstractEditor interface.");

class CountingScintillaEditor : public ScintillaEditor
{
public:
    QString text() const override
    {
        ++textCallCount;
        return ScintillaEditor::text();
    }

    mutable int textCallCount = 0;
};

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
    void scintillaBottomBarUsesCheapCharacterCount();
    void windowAppliesConfiguredTabWidthToScintillaEditors();
    void windowHonorsConfiguredScintillaEditorShortcut();
    void openScintillaWrappersRefreshConfiguredShortcuts();
    void scintillaOpenNewlineActionsMatchLegacyBehavior();
    void scintillaRepresentativeShortcutActionsWork();
    void openingFileReusesEmptyScintillaBlankTab();
#ifdef USE_WEBENGINE
    void markdownPreviewReconnectsForScintillaMarkdownTabs();
#endif
    void windowCanPrintScintillaEditorToPdf();
    void saveAsRefreshesScintillaBottomBarHighlight();
    void windowDisconnectEditorSignalsHandlesLegacyAndScintillaBackends();
    void windowLegacyTextEditorHelperHandlesNonLegacyWrappers();
    void scintillaHighlightMenuSupportsManualSelection();
    void scintillaHighlighterMapsCommonDefinitions();
    void scintillaHighlighterKeepsThemeColorsAfterLexerReload();
#ifdef USE_WEBENGINE
    void markdownPreviewUsesBalancedSplitAndNonGitHubStyle();
#endif
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

void EditorFactoryTest::scintillaBottomBarUsesCheapCharacterCount()
{
    CountingScintillaEditor *editor = new CountingScintillaEditor();
    EditWrapper wrapper{std::unique_ptr<AbstractEditor>(editor)};
    QsciScintilla *widget = qobject_cast<QsciScintilla *>(wrapper.editorWidget());

    QVERIFY(widget != nullptr);

    editor->setText(QStringLiteral("alpha\nbeta"));
    editor->textCallCount = 0;
    wrapper.updateBottomBarForBackend();

    QCOMPARE(widget->length(), 10);
    QCOMPARE(editor->textCallCount, 0);
}

void EditorFactoryTest::windowAppliesConfiguredTabWidthToScintillaEditors()
{
    Window window(nullptr);
    window.m_settings->settings->option("advance.editor.tabspacenumber")->setValue(8);

    std::unique_ptr<EditWrapper> wrapper(window.createEditor());
    QsciScintilla *widget = qobject_cast<QsciScintilla *>(wrapper->editorWidget());

    QVERIFY(widget != nullptr);
    QCOMPARE(widget->indentationWidth(), 8);
    QCOMPARE(widget->tabWidth(), 8);
}

void EditorFactoryTest::windowHonorsConfiguredScintillaEditorShortcut()
{
    Window window(nullptr);
    window.m_settings->settings->option("shortcuts.editor.selectall")->setValue(QStringLiteral("Ctrl+U"));
    EditWrapper *wrapper = new EditWrapper(std::unique_ptr<AbstractEditor>(new ScintillaEditor()));
    QsciScintilla *widget = qobject_cast<QsciScintilla *>(wrapper->editorWidget());
    QShortcut *shortcut = nullptr;

    QVERIFY(widget != nullptr);

    wrapper->editorBackend()->setText(QStringLiteral("alpha\nbeta"));
    window.addTabWithWrapper(wrapper, QStringLiteral("/tmp/shortcut.txt"), QStringLiteral("shortcut.txt"), 0);
    for (QShortcut *candidate : wrapper->findChildren<QShortcut *>()) {
        if (candidate->key() == QKeySequence(QStringLiteral("Ctrl+U"))) {
            shortcut = candidate;
            break;
        }
    }

    QVERIFY(shortcut != nullptr);
    QCOMPARE(shortcut->key(), QKeySequence(QStringLiteral("Ctrl+U")));
    window.removeWrapper(QStringLiteral("/tmp/shortcut.txt"), true);
}

void EditorFactoryTest::openScintillaWrappersRefreshConfiguredShortcuts()
{
    Window window(nullptr);
    EditWrapper *wrapper = new EditWrapper(std::unique_ptr<AbstractEditor>(new ScintillaEditor()));
    QShortcut *shortcut = nullptr;

    window.addTabWithWrapper(wrapper, QStringLiteral("/tmp/refresh-shortcut.txt"), QStringLiteral("refresh-shortcut.txt"), 0);

    window.m_settings->settings->option("shortcuts.editor.selectall")->setValue(QStringLiteral("Ctrl+Shift+U"));

    for (QShortcut *candidate : wrapper->findChildren<QShortcut *>()) {
        if (candidate->key() == QKeySequence(QStringLiteral("Ctrl+Shift+U"))) {
            shortcut = candidate;
            break;
        }
    }

    QVERIFY(shortcut != nullptr);
    QCOMPARE(shortcut->key(), QKeySequence(QStringLiteral("Ctrl+Shift+U")));
    window.removeWrapper(QStringLiteral("/tmp/refresh-shortcut.txt"), true);
}

void EditorFactoryTest::scintillaOpenNewlineActionsMatchLegacyBehavior()
{
    Window window(nullptr);
    EditWrapper *aboveWrapper = new EditWrapper(std::unique_ptr<AbstractEditor>(new ScintillaEditor()));
    EditWrapper *belowWrapper = new EditWrapper(std::unique_ptr<AbstractEditor>(new ScintillaEditor()));
    QsciScintilla *aboveEditor = qobject_cast<QsciScintilla *>(aboveWrapper->editorWidget());
    QsciScintilla *belowEditor = qobject_cast<QsciScintilla *>(belowWrapper->editorWidget());
    int line = -1;
    int index = -1;

    QVERIFY(aboveEditor != nullptr);
    QVERIFY(belowEditor != nullptr);

    aboveWrapper->editorBackend()->setText(QStringLiteral("alpha\nbeta"));
    aboveEditor->setCursorPosition(0, 2);
    QVERIFY(window.triggerScintillaSettingAction(aboveWrapper, QStringLiteral("shortcuts.editor.opennewlineabove")));
    QCOMPARE(aboveEditor->text(), QStringLiteral("\nalpha\nbeta"));
    aboveEditor->getCursorPosition(&line, &index);
    QCOMPARE(line, 0);
    QCOMPARE(index, 0);

    belowWrapper->editorBackend()->setText(QStringLiteral("alpha\nbeta"));
    belowEditor->setCursorPosition(0, 2);
    QVERIFY(window.triggerScintillaSettingAction(belowWrapper, QStringLiteral("shortcuts.editor.opennewlinebelow")));
    QCOMPARE(belowEditor->text(), QStringLiteral("alpha\n\nbeta"));
    belowEditor->getCursorPosition(&line, &index);
    QCOMPARE(line, 1);
    QCOMPARE(index, 0);

    delete aboveWrapper;
    delete belowWrapper;
}

void EditorFactoryTest::scintillaRepresentativeShortcutActionsWork()
{
    Window window(nullptr);
    EditWrapper *wrapper = new EditWrapper(std::unique_ptr<AbstractEditor>(new ScintillaEditor()));
    QsciScintilla *editor = qobject_cast<QsciScintilla *>(wrapper->editorWidget());

    QVERIFY(editor != nullptr);

    wrapper->updatePath(QStringLiteral("/tmp/shortcut-actions.cpp"));
    wrapper->editorBackend()->setText(QStringLiteral("hello\nworld"));
    editor->setCursorPosition(0, 0);
    QVERIFY(window.triggerScintillaSettingAction(wrapper, QStringLiteral("shortcuts.editor.capitalizeword")));
    QCOMPARE(editor->text(), QStringLiteral("Hello\nworld"));

    QVERIFY(window.triggerScintillaSettingAction(wrapper, QStringLiteral("shortcuts.editor.setmark")));
    QVERIFY(editor->property("cursorMark").toBool());

    editor->setSelection(0, 0, 0, 5);
    const long currentBefore = editor->SendScintilla(QsciScintillaBase::SCI_GETCURRENTPOS);
    const long anchorBefore = editor->SendScintilla(QsciScintillaBase::SCI_GETANCHOR);
    QVERIFY(window.triggerScintillaSettingAction(wrapper, QStringLiteral("shortcuts.editor.exchangemark")));
    QCOMPARE(editor->SendScintilla(QsciScintillaBase::SCI_GETCURRENTPOS), anchorBefore);
    QCOMPARE(editor->SendScintilla(QsciScintillaBase::SCI_GETANCHOR), currentBefore);

    editor->setSelection(0, 0, 1, 5);
    QVERIFY(window.triggerScintillaSettingAction(wrapper, QStringLiteral("shortcuts.editor.joinlines")));
    QCOMPARE(editor->text(), QStringLiteral("Hello world"));

    QVERIFY(window.triggerScintillaSettingAction(wrapper, QStringLiteral("shortcuts.editor.togglecomment")));
    QCOMPARE(editor->text(), QStringLiteral("// Hello world"));

    delete wrapper;
}

void EditorFactoryTest::openingFileReusesEmptyScintillaBlankTab()
{
    Window window(nullptr);
    QTemporaryDir dir;
    const QString filePath = dir.filePath(QStringLiteral("reused.cpp"));
    QFile file(filePath);

    QVERIFY(dir.isValid());
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("int main() { return 0; }\n");
    file.close();

    window.addBlankTab();
    const QString blankPath = window.m_tabbar->currentPath();
    QVERIFY(Utils::isDraftFile(blankPath));
    QCOMPARE(window.m_tabbar->count(), 1);

    window.addTab(filePath, true);

    QCOMPARE(window.m_tabbar->count(), 1);
    QCOMPARE(window.m_tabbar->currentPath(), filePath);
    QVERIFY(window.wrapper(blankPath) == nullptr);
    QVERIFY(window.wrapper(filePath) != nullptr);
}

#ifdef USE_WEBENGINE
void EditorFactoryTest::markdownPreviewReconnectsForScintillaMarkdownTabs()
{
    EditWrapper wrapper(std::unique_ptr<AbstractEditor>(new ScintillaEditor()));

    QVERIFY(wrapper.m_markdownPreview != nullptr);

    wrapper.updatePath(QStringLiteral("/tmp/readme.md"));
    wrapper.m_isLoadFinished = false;
    wrapper.handleFileLoadFinished(QByteArrayLiteral("UTF-8"), QStringLiteral("# Title\n\ncontent"));
    QTRY_VERIFY(wrapper.isLoadFinished());

    QVERIFY(wrapper.m_markdownPreview->isVisible());
    QVERIFY(wrapper.m_markdownPreview->m_sourceEditor != nullptr);
    QCOMPARE(wrapper.m_markdownPreview->m_sourceEditor->toPlainText(), QStringLiteral("# Title\n\ncontent"));
}
#endif

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

void EditorFactoryTest::saveAsRefreshesScintillaBottomBarHighlight()
{
    EditWrapper *wrapper = new EditWrapper(std::unique_ptr<AbstractEditor>(new ScintillaEditor()));
    QTemporaryDir dir;
    const QString originalPath = dir.filePath(QStringLiteral("plain.txt"));
    const QString renamedPath = dir.filePath(QStringLiteral("renamed.cpp"));

    QVERIFY(dir.isValid());

    wrapper->updatePath(originalPath);
    wrapper->editorBackend()->setText(QStringLiteral("int main() { return 0; }\n"));
    wrapper->bottomBar()->setHightlightName(QStringLiteral("None"));

    wrapper->updatePath(renamedPath);
    if (AbstractEditor *editor = wrapper->editorBackend()) {
        editor->loadHighlighter();
    }

    QCOMPARE(wrapper->bottomBar()->m_highlightMenu->m_text->text(), QStringLiteral("C++"));

    delete wrapper;
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

void EditorFactoryTest::scintillaHighlightMenuSupportsManualSelection()
{
    EditWrapper wrapper(std::unique_ptr<AbstractEditor>(new ScintillaEditor()));
    QsciScintilla *widget = qobject_cast<QsciScintilla *>(wrapper.editorWidget());

    QVERIFY(widget != nullptr);
    QVERIFY(wrapper.bottomBar()->m_highlightMenu->m_menu != nullptr);
    QVERIFY(!wrapper.bottomBar()->m_highlightMenu->actions().isEmpty());

    QAction *pythonAction = nullptr;
    const QList<QAction *> rootActions = wrapper.bottomBar()->m_highlightMenu->m_menu->actions();
    for (QAction *rootAction : rootActions) {
        if (rootAction->data().toString() == QStringLiteral("Python")) {
            pythonAction = rootAction;
            break;
        }

        if (QMenu *subMenu = rootAction->menu()) {
            for (QAction *action : subMenu->actions()) {
                if (action->data().toString() == QStringLiteral("Python")) {
                    pythonAction = action;
                    break;
                }
            }
        }

        if (pythonAction) {
            break;
        }
    }

    QVERIFY2(pythonAction != nullptr,
             "Scintilla-backed wrappers should expose the manual highlight menu with syntax actions.");

    wrapper.updatePath(QStringLiteral("/tmp/manual-highlight.txt"));
    wrapper.editorBackend()->setText(QStringLiteral("plain text\n"));
    pythonAction->trigger();

    QVERIFY2(qobject_cast<QsciLexerPython *>(widget->lexer()) != nullptr,
             "Selecting a manual highlight scheme should attach the matching Scintilla lexer.");
    QCOMPARE(wrapper.bottomBar()->m_highlightMenu->m_text->text(), QStringLiteral("Python"));
}

void EditorFactoryTest::scintillaHighlighterMapsCommonDefinitions()
{
    ScintillaEditor editor;
    QsciScintilla *widget = qobject_cast<QsciScintilla *>(editor.widget());

    QVERIFY(widget != nullptr);

    widget->setProperty("filepath", QStringLiteral("/tmp/highlight.json"));
    editor.setText(QStringLiteral("{\"name\": \"gxde\"}\n"));
    editor.loadHighlighter();
    QVERIFY(qobject_cast<QsciLexerJSON *>(widget->lexer()) != nullptr);

    widget->setProperty("filepath", QStringLiteral("/tmp/highlight.sh"));
    editor.setText(QStringLiteral("#!/bin/bash\necho ok\n"));
    editor.loadHighlighter();
    QVERIFY(qobject_cast<QsciLexerBash *>(widget->lexer()) != nullptr);

    widget->setProperty("filepath", QStringLiteral("/tmp/highlight.html"));
    editor.setText(QStringLiteral("<html><body>Hello</body></html>\n"));
    editor.loadHighlighter();
    QVERIFY(qobject_cast<QsciLexerHTML *>(widget->lexer()) != nullptr);
}

void EditorFactoryTest::scintillaHighlighterKeepsThemeColorsAfterLexerReload()
{
    ScintillaEditor editor;
    QsciScintilla *widget = qobject_cast<QsciScintilla *>(editor.widget());
    QTemporaryDir dir;
    QFile themeFile(dir.filePath(QStringLiteral("theme.json")));

    QVERIFY(widget != nullptr);
    QVERIFY(dir.isValid());
    QVERIFY(themeFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    themeFile.write(R"({
        "editor-colors": {
            "background-color": "#111111",
            "current-line": "#222222",
            "line-numbers": "#bbbbbb",
            "find-match-background": "#444444",
            "bracket-match-fg": "#ffffff",
            "bracket-match-bg": "#333333"
        },
        "text-styles": {
            "Normal": {
                "text-color": "#eeeeee",
                "selected-text-color": "#ffffff",
                "selected-bg-color": "#555555"
            }
        }
    })");
    themeFile.close();

    editor.setThemeWithPath(themeFile.fileName());
    widget->setProperty("filepath", QStringLiteral("/tmp/theme.cpp"));
    editor.setText(QStringLiteral("int main() { return 0; }\n"));
    editor.loadHighlighter();

    QsciLexerCPP *lexer = qobject_cast<QsciLexerCPP *>(widget->lexer());
    QVERIFY(lexer != nullptr);
    QCOMPARE(lexer->paper(0), QColor(QStringLiteral("#111111")));
    QCOMPARE(lexer->color(0), QColor(QStringLiteral("#eeeeee")));
}

#ifdef USE_WEBENGINE
void EditorFactoryTest::markdownPreviewUsesBalancedSplitAndNonGitHubStyle()
{
    EditWrapper wrapper(std::unique_ptr<AbstractEditor>(new ScintillaEditor()));

    QVERIFY(wrapper.m_markdownPreview != nullptr);

    wrapper.resize(1000, 700);
    wrapper.updatePath(QStringLiteral("/tmp/readme.md"));
    wrapper.m_isLoadFinished = false;
    wrapper.handleFileLoadFinished(QByteArrayLiteral("UTF-8"), QStringLiteral("# Title\n\ncontent"));
    QTRY_VERIFY(wrapper.isLoadFinished());
    QVERIFY(wrapper.m_markdownPreview->isVisible());

    wrapper.layout()->activate();
    const int editorWidth = wrapper.editorWidget()->width();
    const int previewWidth = wrapper.m_markdownPreview->width();
    QVERIFY2(qAbs(editorWidth - previewWidth) < 80,
             "Markdown split view should keep the editor and preview close to a 50/50 layout.");

    const QString html = wrapper.m_markdownPreview->generateHtml(QStringLiteral("# Title\n\ncontent"));
    QVERIFY2(!html.contains(QStringLiteral("github-markdown.css")) &&
             !html.contains(QStringLiteral("github-markdown-dark.css")),
             "Markdown preview should no longer inject the GitHub-style stylesheet.");
}
#endif

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));

    QApplication app(argc, argv);
    EditorFactoryTest test;

    return QTest::qExec(&test, argc, argv);
}

#include "editorfactory_test.moc"
