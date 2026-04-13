#ifndef SCINTILLAEDITOR_H
#define SCINTILLAEDITOR_H

#include "abstracteditor.h"

class QWidget;
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

private:
    ScintillaEditor(const ScintillaEditor &) = delete;
    ScintillaEditor &operator=(const ScintillaEditor &) = delete;

    QsciScintilla *m_editor;
};

#endif
