/*
 * Copyright (C) 2017 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     rekols <rekols@foxmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ddropdownmenu.h"
#include "../utils.h"
#include <QHBoxLayout>
#include <QMouseEvent>

DDropdownMenu::DDropdownMenu(QWidget *parent)
    : QFrame(parent),
      m_menu(new QMenu(this)),
      m_text(new QLabel("undefined")),
      m_arrowLabel(new QLabel)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_arrowLabel->setFixedSize(9, 5);
    QPixmap arrowPixmap = Utils::renderSVG(":/images/dropdown_arrow_light.svg", QSize(9, 5));
    m_arrowLabel->setPixmap(arrowPixmap);

    layout->addStretch();
    layout->addWidget(m_text, 0, Qt::AlignHCenter);
    layout->addSpacing(5);
    layout->addWidget(m_arrowLabel);
    layout->addStretch();

    connect(m_menu, &QMenu::triggered, this, [=] (QAction *action) {
        setText(action->text());
        setCurrentAction(action);
        Q_EMIT this->triggered(action);
        Q_EMIT this->currentTextChanged(action->text());
    });

    connect(this, &DDropdownMenu::requestContextMenu, this, [=] {
        const QSize menuSize = m_menu->sizeHint();
        const QPoint buttonTopCenter = mapToGlobal(QPoint(width() / 2, 0));
        const QPoint popupPosition(buttonTopCenter.x() - menuSize.width() / 2,
                                   buttonTopCenter.y() - menuSize.height());

        // On Wayland a client cannot position a popup by moving its top-level
        // window before it is mapped. Pass the anchor to QMenu so Qt can create
        // the xdg_popup at the requested edge of the bottom bar.
        m_menu->exec(popupPosition);
    });
}

DDropdownMenu::~DDropdownMenu()
{
}

QList<QAction *> DDropdownMenu::actions() const
{
    return m_menu->actions();
}

QAction *DDropdownMenu::addAction(const QString &text)
{
    QAction *action = m_menu->addAction(text);
    action->setCheckable(true);
    setText(action->text());
    return action;
}

void DDropdownMenu::addActions(QStringList list)
{
    for (QString text : list) {
        QAction *action = m_menu->addAction(text);
        action->setCheckable(true);
        setText(action->text());
    }
}

void DDropdownMenu::setCurrentAction(QAction *action)
{
    if (action) {
        for (QAction *action : m_menu->actions()) {
            action->setChecked(false);
        }

        m_text->setText(action->text());
        action->setChecked(true);
    } else {
        for (QAction *action : m_menu->actions()) {
            action->setChecked(false);
        }
    }
}

void DDropdownMenu::setCurrentText(const QString &text)
{
    for (QAction *action : m_menu->actions()) {
        if (action->text() == text) {
            setCurrentAction(action);
            setText(text);
        }
    }
}

void DDropdownMenu::setCurrentTextOnly(const QString &text)
{
    setText(text);
}

void DDropdownMenu::setText(const QString &text)
{
    m_text->setText(text);

    QFontMetrics fm(font());
    setFixedWidth(fm.horizontalAdvance(text) + 40);
}

void DDropdownMenu::setMenu(QMenu *menu)
{
    if (menu == m_menu)
        return;

    if (m_menu) {
        delete m_menu;
    }

    m_menu = menu;
    if (m_menu)
        m_menu->setParent(this, m_menu->windowFlags());
}

void DDropdownMenu::setTheme(const QString &theme)
{
    QString arrowSvgPath = QString(":/images/dropdown_arrow_%1.svg").arg(theme);
    QPixmap arrowPixmap = Utils::renderSVG(arrowSvgPath, QSize(9, 5));
    m_arrowLabel->setPixmap(arrowPixmap);
}

void DDropdownMenu::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        Q_EMIT requestContextMenu();
    }

    QFrame::mouseReleaseEvent(e);
}
