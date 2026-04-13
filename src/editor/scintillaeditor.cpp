#include "scintillaeditor.h"

#include <Qsci/qsciscintilla.h>

ScintillaEditor::ScintillaEditor()
    : m_editor(new QsciScintilla())
{
}

ScintillaEditor::~ScintillaEditor()
{
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
