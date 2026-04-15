#include "scintillaeditor.h"

#include "editorlanguage.h"
#include "../syntaxutils.h"
#include "../utils.h"

#include <Qsci/qscilexer.h>
#include <Qsci/qscilexerbash.h>
#include <Qsci/qscilexerbatch.h>
#include <Qsci/qscilexercmake.h>
#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexercsharp.h>
#include <Qsci/qscilexercss.h>
#include <Qsci/qscilexerdiff.h>
#include <Qsci/qscilexerhtml.h>
#include <Qsci/qscilexerjava.h>
#include <Qsci/qscilexerjavascript.h>
#include <Qsci/qscilexerjson.h>
#include <Qsci/qscilexerlua.h>
#include <Qsci/qscilexermakefile.h>
#include <Qsci/qscilexermarkdown.h>
#include <Qsci/qscilexerperl.h>
#include <Qsci/qscilexerproperties.h>
#include <Qsci/qscilexerpython.h>
#include <Qsci/qscilexerruby.h>
#include <Qsci/qscilexersql.h>
#include <Qsci/qscilexerxml.h>
#include <Qsci/qscilexeryaml.h>
#include <Qsci/qsciscintilla.h>

#include <QColor>
#include <QFileInfo>
#include <QFontMetrics>
#include <QVariantMap>

namespace {
int boundedPosition(int position, int size)
{
    return qBound(0, position, size);
}

QsciLexer *createLexerForLanguage(EditorLanguage::Type language, QsciScintilla *editor)
{
    switch (language) {
    case EditorLanguage::Bash:
        return new QsciLexerBash(editor);
    case EditorLanguage::Batch:
        return new QsciLexerBatch(editor);
    case EditorLanguage::CMake:
        return new QsciLexerCMake(editor);
    case EditorLanguage::Cpp:
        return new QsciLexerCPP(editor);
    case EditorLanguage::CSharp:
        return new QsciLexerCSharp(editor);
    case EditorLanguage::Css:
        return new QsciLexerCSS(editor);
    case EditorLanguage::Diff:
        return new QsciLexerDiff(editor);
    case EditorLanguage::Html:
        return new QsciLexerHTML(editor);
    case EditorLanguage::Ini:
        return new QsciLexerProperties(editor);
    case EditorLanguage::Java:
        return new QsciLexerJava(editor);
    case EditorLanguage::JavaScript:
        return new QsciLexerJavaScript(editor);
    case EditorLanguage::Json:
        return new QsciLexerJSON(editor);
    case EditorLanguage::Lua:
        return new QsciLexerLua(editor);
    case EditorLanguage::Makefile:
        return new QsciLexerMakefile(editor);
    case EditorLanguage::Markdown:
        return new QsciLexerMarkdown(editor);
    case EditorLanguage::Perl:
        return new QsciLexerPerl(editor);
    case EditorLanguage::Python:
        return new QsciLexerPython(editor);
    case EditorLanguage::Ruby:
        return new QsciLexerRuby(editor);
    case EditorLanguage::Sql:
        return new QsciLexerSQL(editor);
    case EditorLanguage::Xml:
        return new QsciLexerXML(editor);
    case EditorLanguage::Yaml:
        return new QsciLexerYAML(editor);
    case EditorLanguage::PlainText:
    default:
        return nullptr;
    }
}
}

ScintillaEditor::ScintillaEditor()
    : m_editor(new QsciScintilla())
{
    m_editor->setUtf8(true);
    m_editor->setBraceMatching(QsciScintilla::StrictBraceMatch);
    m_editor->setCaretLineVisible(true);
    m_editor->setMarginLineNumbers(0, true);

    m_font = m_editor->font();
    if (!m_font.fixedPitch()) {
        m_font.setStyleHint(QFont::Monospace);
        m_font.setFixedPitch(true);
    }

    m_editor->setFont(m_font);
    m_editor->setMarginsFont(m_font);
    m_editor->setMarginWidth(0, QStringLiteral("0000"));
    m_searchIndicator = m_editor->indicatorDefine(QsciScintilla::StraightBoxIndicator);
    m_editor->setIndicatorDrawUnder(true, m_searchIndicator);
}

ScintillaEditor::~ScintillaEditor()
{
    delete m_lexer;
    delete m_editor;
}

QWidget *ScintillaEditor::widget()
{
    return m_editor;
}

QString ScintillaEditor::text() const
{
    return m_editor->text();
}

void ScintillaEditor::setText(const QString &text)
{
    m_editor->setText(text);
}

bool ScintillaEditor::isReadOnly() const
{
    return m_editor->isReadOnly();
}

void ScintillaEditor::setReadOnly(bool readOnly)
{
    m_editor->setReadOnly(readOnly);
}

QString ScintillaEditor::selectedText() const
{
    return m_editor->selectedText();
}

void ScintillaEditor::setSelection(int fromLine, int fromIndex, int toLine, int toIndex)
{
    m_editor->setSelection(fromLine, fromIndex, toLine, toIndex);
}

int ScintillaEditor::currentLine() const
{
    int line = 0;
    int index = 0;
    m_editor->getCursorPosition(&line, &index);
    return line + 1;
}

int ScintillaEditor::currentColumn() const
{
    int line = 0;
    int index = 0;
    m_editor->getCursorPosition(&line, &index);
    return index;
}

int ScintillaEditor::cursorPosition() const
{
    int line = 0;
    int index = 0;
    m_editor->getCursorPosition(&line, &index);
    return m_editor->positionFromLineIndex(line, index);
}

int ScintillaEditor::scrollOffset() const
{
    return m_editor->firstVisibleLine();
}

int ScintillaEditor::lineCount() const
{
    return m_editor->lines();
}

bool ScintillaEditor::isModified() const
{
    return m_editor->isModified();
}

void ScintillaEditor::setModified(bool modified)
{
    m_editor->setModified(modified);
}

void ScintillaEditor::moveToStart()
{
    m_editor->setCursorPosition(0, 0);
    m_editor->setFirstVisibleLine(0);
}

void ScintillaEditor::jumpToLine(int line, bool keepLineAtCenter)
{
    if (lineCount() <= 0) {
        return;
    }

    const int targetLine = qBound(1, line, lineCount()) - 1;
    m_editor->setCursorPosition(targetLine, 0);
    m_editor->ensureLineVisible(targetLine);

    if (keepLineAtCenter) {
        const int lineHeight = qMax(1, QFontMetrics(m_font).lineSpacing());
        const int visibleLines = qMax(1, m_editor->viewport()->height() / lineHeight);
        m_editor->setFirstVisibleLine(qMax(0, targetLine - visibleLines / 2));
    }
}

void ScintillaEditor::scrollToLine(int scrollOffset, int row, int column)
{
    if (lineCount() <= 0) {
        return;
    }

    const int targetLine = qBound(1, row, lineCount()) - 1;
    m_editor->setFirstVisibleLine(qMax(0, scrollOffset));
    m_editor->setCursorPosition(targetLine, qMax(0, column));
    m_editor->ensureLineVisible(targetLine);
}

void ScintillaEditor::setLineWrapMode(bool enable)
{
    m_editor->setWrapMode(enable ? QsciScintilla::WrapWord : QsciScintilla::WrapNone);
}

void ScintillaEditor::setFontFamily(const QString &fontName)
{
    m_font.setFamily(fontName);
    m_editor->setFont(m_font);
    m_editor->setMarginsFont(m_font);
}

void ScintillaEditor::setFontSize(int fontSize)
{
    m_font.setPointSize(fontSize);
    m_editor->setFont(m_font);
    m_editor->setMarginsFont(m_font);
}

void ScintillaEditor::replaceAll(const QString &replaceText, const QString &withText)
{
    if (replaceText.isEmpty()) {
        return;
    }

    m_editor->setText(text().replace(replaceText, withText));
}

void ScintillaEditor::replaceNext(const QString &replaceText, const QString &withText)
{
    if (replaceText.isEmpty()) {
        return;
    }

    int fromLine = 0;
    int fromIndex = 0;
    int toLine = 0;
    int toIndex = 0;
    if (m_editor->hasSelectedText() && selectedText() == replaceText) {
        m_editor->getSelection(&fromLine, &fromIndex, &toLine, &toIndex);
        QString content = text();
        const int start = m_editor->positionFromLineIndex(fromLine, fromIndex);
        const int end = m_editor->positionFromLineIndex(toLine, toIndex);

        content.replace(start, end - start, withText);
        m_editor->setText(content);
        updateCursorKeywordSelection(start + withText.size(), true);
        return;
    }

    highlightKeyword(replaceText, cursorPosition());
}

void ScintillaEditor::replaceRest(const QString &replaceText, const QString &withText)
{
    if (replaceText.isEmpty()) {
        return;
    }

    const int position = cursorPosition();
    const QString content = text();
    QString suffix = content.mid(position);
    suffix.replace(replaceText, withText);
    m_editor->setText(content.left(position) + suffix);
}

bool ScintillaEditor::findKeywordForward(const QString &keyword)
{
    return !keyword.isEmpty() && text().contains(keyword);
}

void ScintillaEditor::removeKeywords()
{
    m_searchKeyword.clear();
    clearSearchIndicators();
    clearSelection();
}

void ScintillaEditor::highlightKeyword(const QString &keyword, int position)
{
    m_searchKeyword = keyword;

    if (m_searchKeyword.trimmed().isEmpty()) {
        clearSearchIndicators();
        clearSelection();
        return;
    }

    updateSearchIndicators();
    updateCursorKeywordSelection(position, true);
}

void ScintillaEditor::updateCursorKeywordSelection(int position, bool findNext)
{
    if (m_searchKeyword.isEmpty()) {
        clearSelection();
        return;
    }

    const QString content = text();
    if (content.isEmpty()) {
        clearSelection();
        return;
    }

    const int startPosition = boundedPosition(position, content.size());
    int matchPosition = -1;

    if (findNext) {
        matchPosition = content.indexOf(m_searchKeyword, startPosition);
        if (matchPosition < 0) {
            matchPosition = content.indexOf(m_searchKeyword);
        }
    } else {
        matchPosition = content.lastIndexOf(m_searchKeyword, qMax(0, startPosition - 1));
        if (matchPosition < 0) {
            matchPosition = content.lastIndexOf(m_searchKeyword);
        }
    }

    if (matchPosition < 0) {
        clearSelection();
        return;
    }

    setSelectionByPosition(matchPosition, matchPosition + m_searchKeyword.size());
}

void ScintillaEditor::renderAllSelections()
{
    if (!m_editor->hasSelectedText()) {
        return;
    }

    int fromLine = 0;
    int fromIndex = 0;
    int toLine = 0;
    int toIndex = 0;
    m_editor->getSelection(&fromLine, &fromIndex, &toLine, &toIndex);
    m_editor->ensureLineVisible(fromLine);
    m_editor->ensureLineVisible(toLine);
    m_editor->repaint();
}

void ScintillaEditor::saveMarkStatus()
{
    m_hasSavedSelection = false;
}

void ScintillaEditor::restoreMarkStatus()
{
}

void ScintillaEditor::setThemeWithPath(const QString &path)
{
    m_themePath = path;
    const QVariantMap jsonMap = Utils::getThemeMapFromPath(path);
    const QVariantMap editorColors = jsonMap.value(QStringLiteral("editor-colors")).toMap();
    const QVariantMap textStyles = jsonMap.value(QStringLiteral("text-styles")).toMap();
    const QVariantMap normalText = textStyles.value(QStringLiteral("Normal")).toMap();

    const QColor backgroundColor(editorColors.value(QStringLiteral("background-color")).toString());
    const QColor currentLineColor(editorColors.value(QStringLiteral("current-line")).toString());
    const QColor lineNumberColor(editorColors.value(QStringLiteral("line-numbers")).toString());
    const QColor selectionForeground(normalText.value(QStringLiteral("selected-text-color")).toString());
    const QColor selectionBackground(normalText.value(QStringLiteral("selected-bg-color")).toString());
    const QColor textColor(normalText.value(QStringLiteral("text-color")).toString());
    const QColor findMatchColor(editorColors.value(QStringLiteral("find-match-background")).toString());

    m_editor->setPaper(backgroundColor);
    m_editor->setColor(textColor);
    m_editor->setCaretLineBackgroundColor(currentLineColor);
    m_editor->setMarginsBackgroundColor(backgroundColor);
    m_editor->setMarginsForegroundColor(lineNumberColor);
    m_editor->setSelectionForegroundColor(selectionForeground);
    m_editor->setSelectionBackgroundColor(selectionBackground);
    m_editor->setIndicatorForegroundColor(findMatchColor, m_searchIndicator);
    m_editor->setMatchedBraceForegroundColor(QColor(editorColors.value(QStringLiteral("bracket-match-fg")).toString()));
    m_editor->setMatchedBraceBackgroundColor(QColor(editorColors.value(QStringLiteral("bracket-match-bg")).toString()));

    applyLexerTheme();
}

void ScintillaEditor::loadHighlighter()
{
    const QString filePath = m_editor->property("filepath").toString();
    const QString forcedDefinitionName = m_editor->property("forcedSyntaxDefinitionName").toString();
    const bool forceNoHighlight = m_editor->property("forceDisableSyntaxHighlight").toBool();

    delete m_lexer;
    m_lexer = nullptr;

    if (forceNoHighlight) {
        m_editor->setProperty("currentSyntaxDefinitionName", QString());
        m_editor->setLexer(nullptr);
        return;
    }

    const QString definitionName = forcedDefinitionName.isEmpty()
            ? SyntaxUtils::detectSyntaxDefinitionName(KSyntaxHighlighting::Repository(), filePath, text().left(4096))
            : forcedDefinitionName;

    m_editor->setProperty("currentSyntaxDefinitionName", definitionName);

    if (SyntaxUtils::shouldSkipSyntaxHighlightForLargeDocuments(text().size())) {
        m_editor->setLexer(nullptr);
        return;
    }

    m_lexer = createLexerForLanguage(EditorLanguage::fromSyntaxDefinitionName(definitionName), m_editor);

    applyLexerTheme();
    m_editor->setLexer(m_lexer);
}

void ScintillaEditor::beginBulkLoad()
{
    m_editor->setUpdatesEnabled(false);
}

void ScintillaEditor::endBulkLoad()
{
    m_editor->setUpdatesEnabled(true);
    m_editor->update();
}

void ScintillaEditor::clearSelection()
{
    int line = 0;
    int index = 0;
    m_editor->getCursorPosition(&line, &index);
    m_editor->setCursorPosition(line, index);
}

void ScintillaEditor::applyLexerTheme()
{
    if (!m_lexer || m_themePath.isEmpty()) {
        return;
    }

    const QVariantMap jsonMap = Utils::getThemeMapFromPath(m_themePath);
    const QVariantMap editorColors = jsonMap.value(QStringLiteral("editor-colors")).toMap();
    const QVariantMap textStyles = jsonMap.value(QStringLiteral("text-styles")).toMap();
    const QVariantMap normalText = textStyles.value(QStringLiteral("Normal")).toMap();

    const QColor backgroundColor(editorColors.value(QStringLiteral("background-color")).toString());
    const QColor textColor(normalText.value(QStringLiteral("text-color")).toString());

    m_lexer->setDefaultPaper(backgroundColor);
    m_lexer->setDefaultColor(textColor);
    m_lexer->setPaper(backgroundColor, -1);
    m_lexer->setColor(textColor, -1);
}

void ScintillaEditor::clearSearchIndicators()
{
    const int lines = lineCount();
    if (m_searchIndicator < 0 || lines <= 0) {
        return;
    }

    const int lastLine = lines - 1;
    const int lastIndex = m_editor->lineLength(lastLine);
    m_editor->clearIndicatorRange(0, 0, lastLine, lastIndex, m_searchIndicator);
}

void ScintillaEditor::updateSearchIndicators()
{
    clearSearchIndicators();

    if (m_searchIndicator < 0 || m_searchKeyword.isEmpty()) {
        return;
    }

    const QString content = text();
    int matchPosition = content.indexOf(m_searchKeyword);
    while (matchPosition >= 0) {
        int fromLine = 0;
        int fromIndex = 0;
        int toLine = 0;
        int toIndex = 0;

        positionToLineIndex(matchPosition, &fromLine, &fromIndex);
        positionToLineIndex(matchPosition + m_searchKeyword.size(), &toLine, &toIndex);
        m_editor->fillIndicatorRange(fromLine, fromIndex, toLine, toIndex, m_searchIndicator);

        matchPosition = content.indexOf(m_searchKeyword, matchPosition + m_searchKeyword.size());
    }
}

void ScintillaEditor::positionToLineIndex(int position, int *line, int *index) const
{
    m_editor->lineIndexFromPosition(boundedPosition(position, text().size()), line, index);
}

void ScintillaEditor::setSelectionByPosition(int start, int end)
{
    int fromLine = 0;
    int fromIndex = 0;
    int toLine = 0;
    int toIndex = 0;

    positionToLineIndex(start, &fromLine, &fromIndex);
    positionToLineIndex(end, &toLine, &toIndex);
    m_editor->setSelection(fromLine, fromIndex, toLine, toIndex);
}
