#ifndef LEGACYTEXTEDITOR_H
#define LEGACYTEXTEDITOR_H

#include "abstracteditor.h"

class DTextEdit;
class QWidget;

class LegacyTextEditor : public AbstractEditor
{
public:
    explicit LegacyTextEditor(QWidget *parent = nullptr);
    ~LegacyTextEditor() override;

    QWidget *widget() override;
    QString text() const override;
    void setText(const QString &text) override;
    bool isReadOnly() const override;
    void setReadOnly(bool readOnly) override;

private:
    DTextEdit *m_editor;
};

#endif
