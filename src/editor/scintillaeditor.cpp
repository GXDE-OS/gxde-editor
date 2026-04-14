#include "scintillaeditor.h"

#include "editorlanguage.h"
#include "../syntaxutils.h"
#include "../utils.h"

#include <Qsci/qscilexer.h>
#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexermarkdown.h>
#include <Qsci/qscilexerpython.h>
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
    clearSelection();
}

void ScintillaEditor::highlightKeyword(const QString &keyword, int position)
{
    m_searchKeyword = keyword;

    if (m_searchKeyword.trimmed().isEmpty()) {
        clearSelection();
        return;
    }

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
    int fromLine = 0;
    int fromIndex = 0;
    int toLine = 0;
    int toIndex = 0;
    m_hasSavedSelection = m_editor->hasSelectedText();

    if (m_hasSavedSelection) {
        m_editor->getSelection(&fromLine, &fromIndex, &toLine, &toIndex);
    }

    if (m_hasSavedSelection) {
        m_savedSelectionFromLine = fromLine;
        m_savedSelectionFromIndex = fromIndex;
        m_savedSelectionToLine = toLine;
        m_savedSelectionToIndex = toIndex;
    }
}

void ScintillaEditor::restoreMarkStatus()
{
    if (!m_hasSavedSelection) {
        return;
    }

    m_editor->setSelection(m_savedSelectionFromLine,
                           m_savedSelectionFromIndex,
                           m_savedSelectionToLine,
                           m_savedSelectionToIndex);
}

void ScintillaEditor::setThemeWithPath(const QString &path)
{
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

    m_editor->setPaper(backgroundColor);
    m_editor->setColor(textColor);
    m_editor->setCaretLineBackgroundColor(currentLineColor);
    m_editor->setMarginsBackgroundColor(backgroundColor);
    m_editor->setMarginsForegroundColor(lineNumberColor);
    m_editor->setSelectionForegroundColor(selectionForeground);
    m_editor->setSelectionBackgroundColor(selectionBackground);
    m_editor->setMatchedBraceForegroundColor(QColor(editorColors.value(QStringLiteral("bracket-match-fg")).toString()));
    m_editor->setMatchedBraceBackgroundColor(QColor(editorColors.value(QStringLiteral("bracket-match-bg")).toString()));
}

void ScintillaEditor::loadHighlighter()
{
    const QString filePath = m_editor->property("filepath").toString();
    const QString definitionName = SyntaxUtils::detectSyntaxDefinitionName(
                KSyntaxHighlighting::Repository(),
                filePath,
                text().left(4096));

    delete m_lexer;
    m_lexer = nullptr;

    switch (EditorLanguage::fromSyntaxDefinitionName(definitionName)) {
    case EditorLanguage::Cpp:
        m_lexer = new QsciLexerCPP(m_editor);
        break;
    case EditorLanguage::Markdown:
        m_lexer = new QsciLexerMarkdown(m_editor);
        break;
    case EditorLanguage::Python:
        m_lexer = new QsciLexerPython(m_editor);
        break;
    case EditorLanguage::PlainText:
    default:
        break;
    }

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
