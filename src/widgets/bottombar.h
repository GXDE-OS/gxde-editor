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

#ifndef BOTTOMBAR_H
#define BOTTOMBAR_H

#include <QWidget>
#include <QLabel>
#include "ddropdownmenu.h"
#include "../markdownlogic.h"

class EditWrapper;
class BottomBar : public QWidget
{
    Q_OBJECT

public:
    BottomBar(QWidget *parent = nullptr);
    ~BottomBar();

    void updatePosition(int row, int column);
    void updateWordCount(int charactorCount);
    void setEncodeName(const QString &name);
    void setCursorStatus(const QString &text);
    void setHighlightMenu(QMenu *menu);
    void setHightlightName(const QString &name);
    void setViewMode(ViewMode mode);
    void setMarkdownAvailable(bool available);
    void setPalette(const QPalette &palette);

private:
    void handleEncodeChanged(const QString &name);

protected:
    void paintEvent(QPaintEvent *);

signals:
    void viewModeRequested(ViewMode mode);

private:
    EditWrapper *m_wrapper;
    QLabel *m_positionLabel;
    QLabel *m_charCountLabel;
    QLabel *m_cursorStatus;
    DDropdownMenu *m_encodeMenu;
    DDropdownMenu *m_highlightMenu;
    DDropdownMenu *m_viewModeMenu;
    QAction *m_editViewAction;
    QAction *m_readViewAction;
    QAction *m_livePreviewAction;
    QString m_rowStr;
    QString m_columnStr;
    QString m_chrCountStr;
};

#endif
