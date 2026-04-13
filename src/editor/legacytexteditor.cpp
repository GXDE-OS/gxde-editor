#include "legacytexteditor.h"

#include "../dtextedit.h"

LegacyTextEditor::LegacyTextEditor(QWidget *parent)
    : m_editor(new DTextEdit(parent)),
      m_language(EditorLanguage::PlainText)
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

EditorLanguage::Type LegacyTextEditor::language() const
{
    return m_language;
}

void LegacyTextEditor::setLanguage(EditorLanguage::Type language)
{
    m_language = language;
}
