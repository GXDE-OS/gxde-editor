#include "scintillaeditor.h"

#ifdef HAVE_QSCINTILLA
#include <Qsci/qsciscintilla.h>
#else
#include <QPlainTextEdit>
#endif

ScintillaEditor::ScintillaEditor()
#ifdef HAVE_QSCINTILLA
    : m_editor(new QsciScintilla())
#else
    : m_editor(new QPlainTextEdit())
#endif
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
#ifdef HAVE_QSCINTILLA
    return m_editor->text();
#else
    return m_editor->toPlainText();
#endif
}

void ScintillaEditor::setText(const QString &text)
{
#ifdef HAVE_QSCINTILLA
    m_editor->setText(text);
#else
    m_editor->setPlainText(text);
#endif
}

bool ScintillaEditor::isReadOnly() const
{
    return m_editor->isReadOnly();
}

void ScintillaEditor::setReadOnly(bool readOnly)
{
    m_editor->setReadOnly(readOnly);
}
