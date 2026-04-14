#include "legacytexteditor.h"

#include "../dtextedit.h"

#include <QTextBlock>
#include <QTextCursor>

LegacyTextEditor::LegacyTextEditor()
    : m_editor(new DTextEdit())
{
}

LegacyTextEditor::~LegacyTextEditor()
{
    delete m_editor;
}

QWidget *LegacyTextEditor::widget()
{
    return m_editor;
}

QString LegacyTextEditor::text() const
{
    return m_editor->toPlainText();
}

void LegacyTextEditor::setText(const QString &text)
{
    m_editor->setPlainText(text);
}

bool LegacyTextEditor::isReadOnly() const
{
    return m_editor->isReadOnly();
}

void LegacyTextEditor::setReadOnly(bool readOnly)
{
    m_editor->setReadOnly(readOnly);
}

QString LegacyTextEditor::selectedText() const
{
    return m_editor->textCursor().selectedText();
}

void LegacyTextEditor::setSelection(int fromLine, int fromIndex, int toLine, int toIndex)
{
    QTextCursor cursor(m_editor->document()->findBlockByLineNumber(fromLine));
    cursor.setPosition(cursor.position() + fromIndex);

    QTextCursor endCursor(m_editor->document()->findBlockByLineNumber(toLine));
    cursor.setPosition(endCursor.position() + toIndex, QTextCursor::KeepAnchor);
    m_editor->setTextCursor(cursor);
}

int LegacyTextEditor::currentLine() const
{
    return m_editor->getCurrentLine();
}

int LegacyTextEditor::currentColumn() const
{
    return m_editor->getCurrentColumn();
}

int LegacyTextEditor::cursorPosition() const
{
    return m_editor->getPosition();
}

int LegacyTextEditor::scrollOffset() const
{
    return m_editor->getScrollOffset();
}

int LegacyTextEditor::lineCount() const
{
    return m_editor->blockCount();
}

bool LegacyTextEditor::isModified() const
{
    return m_editor->document()->isModified();
}

void LegacyTextEditor::setModified(bool modified)
{
    m_editor->setModified(modified);
}

void LegacyTextEditor::moveToStart()
{
    m_editor->moveToStart();
}

void LegacyTextEditor::jumpToLine(int line, bool keepLineAtCenter)
{
    m_editor->jumpToLine(line, keepLineAtCenter);
}

void LegacyTextEditor::scrollToLine(int scrollOffset, int row, int column)
{
    m_editor->scrollToLine(scrollOffset, row, column);
}

void LegacyTextEditor::setLineWrapMode(bool enable)
{
    m_editor->setLineWrapMode(enable);
}

void LegacyTextEditor::setFontFamily(const QString &fontName)
{
    m_editor->setFontFamily(fontName);
}

void LegacyTextEditor::setFontSize(int fontSize)
{
    m_editor->setFontSize(fontSize);
}

void LegacyTextEditor::replaceAll(const QString &replaceText, const QString &withText)
{
    m_editor->replaceAll(replaceText, withText);
}

void LegacyTextEditor::replaceNext(const QString &replaceText, const QString &withText)
{
    m_editor->replaceNext(replaceText, withText);
}

void LegacyTextEditor::replaceRest(const QString &replaceText, const QString &withText)
{
    m_editor->replaceRest(replaceText, withText);
}

bool LegacyTextEditor::findKeywordForward(const QString &keyword)
{
    return m_editor->findKeywordForward(keyword);
}

void LegacyTextEditor::removeKeywords()
{
    m_editor->removeKeywords();
}

void LegacyTextEditor::highlightKeyword(const QString &keyword, int position)
{
    m_editor->highlightKeyword(keyword, position);
}

void LegacyTextEditor::updateCursorKeywordSelection(int position, bool findNext)
{
    m_editor->updateCursorKeywordSelection(position, findNext);
}

void LegacyTextEditor::renderAllSelections()
{
    m_editor->renderAllSelections();
}

void LegacyTextEditor::saveMarkStatus()
{
    m_editor->saveMarkStatus();
}

void LegacyTextEditor::restoreMarkStatus()
{
    m_editor->restoreMarkStatus();
}

void LegacyTextEditor::setThemeWithPath(const QString &path)
{
    m_editor->setThemeWithPath(path);
}

void LegacyTextEditor::loadHighlighter()
{
    m_editor->loadHighlighter();
}

void LegacyTextEditor::beginBulkLoad()
{
    m_editor->beginBulkLoad();
}

void LegacyTextEditor::endBulkLoad()
{
    m_editor->endBulkLoad();
}
