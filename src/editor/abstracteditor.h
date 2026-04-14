#ifndef ABSTRACTEDITOR_H
#define ABSTRACTEDITOR_H

#include "editorlanguage.h"

#include <QString>

class QWidget;

class AbstractEditor
{
public:
    virtual ~AbstractEditor() {}

    virtual QWidget *widget() = 0;
    virtual QString text() const = 0;
    virtual void setText(const QString &text) = 0;
    virtual bool isReadOnly() const = 0;
    virtual void setReadOnly(bool readOnly) = 0;
    virtual QString selectedText() const = 0;
    virtual void setSelection(int fromLine, int fromIndex, int toLine, int toIndex) = 0;
    virtual int currentLine() const = 0;
    virtual int currentColumn() const = 0;
    virtual int cursorPosition() const = 0;
    virtual int scrollOffset() const = 0;
    virtual int lineCount() const = 0;
    virtual bool isModified() const = 0;
    virtual void setModified(bool modified) = 0;
    virtual void moveToStart() = 0;
    virtual void jumpToLine(int line, bool keepLineAtCenter) = 0;
    virtual void scrollToLine(int scrollOffset, int row, int column) = 0;
    virtual void setLineWrapMode(bool enable) = 0;
    virtual void setFontFamily(const QString &fontName) = 0;
    virtual void setFontSize(int fontSize) = 0;
    virtual void replaceAll(const QString &replaceText, const QString &withText) = 0;
    virtual void replaceNext(const QString &replaceText, const QString &withText) = 0;
    virtual void replaceRest(const QString &replaceText, const QString &withText) = 0;
    virtual bool findKeywordForward(const QString &keyword) = 0;
    virtual void removeKeywords() = 0;
    virtual void highlightKeyword(const QString &keyword, int position) = 0;
    virtual void updateCursorKeywordSelection(int position, bool findNext) = 0;
    virtual void renderAllSelections() = 0;
    virtual void saveMarkStatus() = 0;
    virtual void restoreMarkStatus() = 0;
    virtual void setThemeWithPath(const QString &path) = 0;
    virtual void loadHighlighter() = 0;
    virtual void beginBulkLoad() = 0;
    virtual void endBulkLoad() = 0;
};

#endif
