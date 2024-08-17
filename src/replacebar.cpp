/* -*- Mode: C++; indent-tabs-mode: nil; tab-width: 4 -*-
 * -*- coding: utf-8 -*-
 *
 * Copyright (C) 2011 ~ 2018 Deepin, Inc.
 *
 * Author:     Wang Yong <wangyong@deepin.com>
 * Maintainer: Rekols    <rekols@foxmail.com>
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

#include "replacebar.h"
#include "utils.h"

#include <QPainterPath>
#include <QDebug>

ReplaceBar::ReplaceBar(QWidget *parent)
    : QWidget(parent)
{
    // Init.
    setWindowFlags(Qt::FramelessWindowHint | Qt::X11BypassWindowManagerHint);
    setFixedHeight(45);

    // Init layout and widgets.
    m_layout = new QHBoxLayout(this);
    m_replaceLabel = new QLabel(tr("Replace: "));
    m_replaceLine = new LineBar();
    m_withLabel = new QLabel(tr("With: "));
    m_withLine = new LineBar();
    m_replaceButton = new QPushButton(tr("Replace"));
    m_replaceSkipButton = new QPushButton(tr("Skip"));
    m_replaceRestButton = new QPushButton(tr("Replace Rest"));
    m_replaceAllButton = new QPushButton(tr("Replace All"));
    m_closeButton = new DImageButton();
    m_closeButton->setFixedSize(16, 16);

    m_layout->addWidget(m_replaceLabel);
    m_layout->addWidget(m_replaceLine);
    m_layout->addWidget(m_withLabel);
    m_layout->addWidget(m_withLine);
    m_layout->addWidget(m_replaceButton);
    m_layout->addWidget(m_replaceSkipButton);
    m_layout->addWidget(m_replaceRestButton);
    m_layout->addWidget(m_replaceAllButton);
    m_layout->addWidget(m_closeButton);

    // Make button don't grab keyboard focus after click it.
    m_replaceButton->setFocusPolicy(Qt::NoFocus);
    m_replaceSkipButton->setFocusPolicy(Qt::NoFocus);
    m_replaceRestButton->setFocusPolicy(Qt::NoFocus);
    m_replaceAllButton->setFocusPolicy(Qt::NoFocus);
    m_closeButton->setFocusPolicy(Qt::NoFocus);

    connect(m_replaceLine, &LineBar::pressEsc, this, &ReplaceBar::replaceCancel, Qt::QueuedConnection);
    connect(m_withLine, &LineBar::pressEsc, this, &ReplaceBar::replaceCancel, Qt::QueuedConnection);

    connect(m_replaceLine, &LineBar::pressEnter, this, &ReplaceBar::handleReplaceNext, Qt::QueuedConnection);
    connect(m_withLine, &LineBar::pressEnter, this, &ReplaceBar::handleReplaceNext, Qt::QueuedConnection);

    connect(m_replaceLine, &LineBar::pressCtrlEnter, this, &ReplaceBar::replaceSkip, Qt::QueuedConnection);
    connect(m_withLine, &LineBar::pressCtrlEnter, this, &ReplaceBar::replaceSkip, Qt::QueuedConnection);

    connect(m_replaceLine, &LineBar::pressAltEnter, this, &ReplaceBar::handleReplaceRest, Qt::QueuedConnection);
    connect(m_withLine, &LineBar::pressAltEnter, this, &ReplaceBar::handleReplaceRest, Qt::QueuedConnection);

    connect(m_replaceLine, &LineBar::pressMetaEnter, this, &ReplaceBar::handleReplaceAll, Qt::QueuedConnection);
    connect(m_withLine, &LineBar::pressMetaEnter, this, &ReplaceBar::handleReplaceAll, Qt::QueuedConnection);

    connect(m_replaceLine, &LineBar::contentChanged, this, &ReplaceBar::handleContentChanged, Qt::QueuedConnection);

    connect(m_replaceButton, &QPushButton::clicked, this, &ReplaceBar::handleReplaceNext, Qt::QueuedConnection);
    connect(m_replaceSkipButton, &QPushButton::clicked, this, &ReplaceBar::replaceSkip, Qt::QueuedConnection);
    connect(m_replaceRestButton, &QPushButton::clicked, this, &ReplaceBar::handleReplaceRest, Qt::QueuedConnection);
    connect(m_replaceAllButton, &QPushButton::clicked, this, &ReplaceBar::handleReplaceAll, Qt::QueuedConnection);

    connect(m_closeButton, &DImageButton::clicked, this, &ReplaceBar::replaceCancel, Qt::QueuedConnection);
}

bool ReplaceBar::isFocus()
{
    return m_replaceLine->hasFocus();
}

void ReplaceBar::focus()
{
    m_replaceLine->setFocus();
}

void ReplaceBar::activeInput(QString text, QString file, int row, int column, int scrollOffset)
{
    // Try fill keyword with select text.
    m_replaceLine->clear();
    m_replaceLine->insert(text);
    m_replaceLine->selectAll();

    // Show.
    show();

    // Save file info for back to position.
    m_replaceFile = file;
    m_replaceFileRow = row;
    m_replaceFileColumn = column;
    m_replaceFileSrollOffset = scrollOffset;

    // Focus.
    focus();
}

void ReplaceBar::replaceCancel()
{
    hide();
}

void ReplaceBar::handleContentChanged()
{
    updateSearchKeyword(m_replaceFile, m_replaceLine->text());
}

void ReplaceBar::handleReplaceNext()
{
    replaceNext(m_replaceLine->text(), m_withLine->text());
}

void ReplaceBar::handleReplaceRest()
{
    replaceRest(m_replaceLine->text(), m_withLine->text());
}

void ReplaceBar::handleReplaceAll()
{
    replaceAll(m_replaceLine->text(), m_withLine->text());
}

void ReplaceBar::hideEvent(QHideEvent *)
{
    removeSearchKeyword();
}

void ReplaceBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setOpacity(1);
    QPainterPath path;
    path.addRect(rect());
    painter.fillPath(path, m_backgroundColor);

    QColor splitLineColor;
    if (m_backgroundColor.lightness() < 128) {
        splitLineColor = QColor("#ffffff");
    } else {
        splitLineColor = QColor("#000000");
    }
    QPainterPath framePath;
    framePath.addRect(QRect(rect().x(), rect().y(), rect().width(), 1));
    painter.setOpacity(0.05);
    painter.fillPath(framePath, splitLineColor);
}

void ReplaceBar::setBackground(QString color)
{
    m_backgroundColor = QColor(color);

    if (QColor(m_backgroundColor).lightness() < 128) {
        m_replaceLabel->setStyleSheet(QString("QLabel { background-color: %1; color: %2; }").arg(color).arg("#AAAAAA"));
        m_withLabel->setStyleSheet(QString("QLabel { background-color: %1; color: %2; }").arg(color).arg("#AAAAAA"));

        m_closeButton->setNormalPic(Utils::getQrcPath("bar_close_normal_dark.svg"));
        m_closeButton->setHoverPic(Utils::getQrcPath("bar_close_hover_dark.svg"));
        m_closeButton->setPressPic(Utils::getQrcPath("bar_close_press_dark.svg"));
    } else {
        m_replaceLabel->setStyleSheet(QString("QLabel { background-color: %1; color: %2; }").arg(color).arg("#000000"));
        m_withLabel->setStyleSheet(QString("QLabel { background-color: %1; color: %2; }").arg(color).arg("#000000"));

        m_closeButton->setNormalPic(Utils::getQrcPath("bar_close_normal_light.svg"));
        m_closeButton->setHoverPic(Utils::getQrcPath("bar_close_hover_light.svg"));
        m_closeButton->setPressPic(Utils::getQrcPath("bar_close_press_light.svg"));
    }

    repaint();
}

bool ReplaceBar::focusNextPrevChild(bool)
{
    // Make keyword jump between two EditLine widgets.
    auto *editWidget = qobject_cast<LineBar*>(focusWidget());
    if (editWidget != nullptr) {
        if (editWidget == m_replaceLine) {
            m_withLine->setFocus();

            return true;
        } else if (editWidget == m_withLine) {
            m_replaceLine->setFocus();

            return true;
        }
    }

    return false;
}

void ReplaceBar::setMismatchAlert(bool isAlert)
{
    m_replaceLine->setAlert(isAlert);
}
