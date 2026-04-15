#ifndef SCINTILLAEDITOR_H
#define SCINTILLAEDITOR_H

#include "abstracteditor.h"

#include <QFont>
#include <QString>

class QWidget;
class QsciLexer;
class QsciScintilla;

class ScintillaEditor : public AbstractEditor
{
public:
    ScintillaEditor();
    ~ScintillaEditor() override;

    QWidget *widget() override;
    QString text() const override;
    void setText(const QString &text) override;
    bool isReadOnly() const override;
    void setReadOnly(bool readOnly) override;
    QString selectedText() const override;
    void setSelection(int fromLine, int fromIndex, int toLine, int toIndex) override;
    int currentLine() const override;
    int currentColumn() const override;
    int cursorPosition() const override;
    int scrollOffset() const override;
    int lineCount() const override;
    bool isModified() const override;
    void setModified(bool modified) override;
    void moveToStart() override;
    void jumpToLine(int line, bool keepLineAtCenter) override;
    void scrollToLine(int scrollOffset, int row, int column) override;
    void setLineWrapMode(bool enable) override;
    void setFontFamily(const QString &fontName) override;
    void setFontSize(int fontSize) override;
    void replaceAll(const QString &replaceText, const QString &withText) override;
    void replaceNext(const QString &replaceText, const QString &withText) override;
    void replaceRest(const QString &replaceText, const QString &withText) override;
    bool findKeywordForward(const QString &keyword) override;
    void removeKeywords() override;
    void highlightKeyword(const QString &keyword, int position) override;
    void updateCursorKeywordSelection(int position, bool findNext) override;
    void renderAllSelections() override;
    void saveMarkStatus() override;
    void restoreMarkStatus() override;
    void setThemeWithPath(const QString &path) override;
    void loadHighlighter() override;
    void beginBulkLoad() override;
    void endBulkLoad() override;

private:
    void clearSearchIndicators();
    void updateSearchIndicators();
    void clearSelection();
    void applyLexerTheme();
    void positionToLineIndex(int position, int *line, int *index) const;
    void setSelectionByPosition(int start, int end);

    ScintillaEditor(const ScintillaEditor &) = delete;
    ScintillaEditor &operator=(const ScintillaEditor &) = delete;

    QsciScintilla *m_editor;
    QString m_searchKeyword;
    bool m_hasSavedSelection = false;
    int m_savedSelectionFromLine = 0;
    int m_savedSelectionFromIndex = 0;
    int m_savedSelectionToLine = 0;
    int m_savedSelectionToIndex = 0;
    QFont m_font;
    QString m_themePath;
    QsciLexer *m_lexer = nullptr;
    int m_searchIndicator = -1;
};

#endif
