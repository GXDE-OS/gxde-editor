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
    virtual EditorLanguage::Type language() const = 0;
    virtual void setLanguage(EditorLanguage::Type language) = 0;
};

#endif
