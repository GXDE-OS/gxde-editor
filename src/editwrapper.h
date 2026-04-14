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

#ifndef EDITORBUFFER_H
#define EDITORBUFFER_H

#include "dbusinterface.h"
#include "dtextedit.h"
#include "editor/abstracteditor.h"
#include "widgets/bottombar.h"
#include "widgets/toast.h"
#ifdef USE_WEBENGINE
#include "widgets/markdownpreviewwidget.h"
#endif

#include <memory>
#include <QVBoxLayout>
#include <QWidget>

class EditWrapper : public QWidget
{
    Q_OBJECT

public:
    // end of line mode.
    enum EndOfLineMode {
        eolUnknown = -1,
        eolUnix = 0,
        eolDos = 1,
        eolMac = 2
    };

    struct FileStateItem {
        QDateTime modified;
        QFile::Permissions permissions;
    };

    EditWrapper(QWidget *parent = 0);
    EditWrapper(std::unique_ptr<AbstractEditor> editorBackend, QWidget *parent = 0);
    ~EditWrapper();

    void openFile(const QString &filepath);
    bool saveFile();
    void updatePath(const QString &file);
    void refresh();
    bool isLoadFinished() { return m_isLoadFinished; }

    EndOfLineMode endOfLineMode();
    void setEndOfLineMode(EndOfLineMode eol);
    void setTextCodec(QByteArray encodeName, bool reload = false);

    BottomBar *bottomBar() { return m_bottomBar; }
    QString filePath() const { return m_filePath; }
    AbstractEditor *editorBackend() const { return m_editorBackend.get(); }
    QWidget *editorWidget() const { return m_editorBackend ? m_editorBackend->widget() : nullptr; }
    DTextEdit *textEditor() { return m_textEdit; }
    bool toastVisible() { return m_toast->isVisible(); }
    void hideToast();

    void checkForReload();
    void initToastPosition();
    void setDarkTheme(bool enabled);

signals:
    void requestSaveAs();

private:
    void updateBottomBarForBackend();
    void updateBottomBarHighlight();
    void detectEndOfLine();
    void appendPendingTextLoadChunk();
    void finishPendingTextLoad();
    void handleCursorModeChanged(DTextEdit::CursorMode mode);
    void handleHightlightChanged(const QString &name);
    void handleFileLoadFinished(const QByteArray &encode, const QString &content);
    void setTextCodec(QTextCodec *codec, bool reload = false);

protected:
    void resizeEvent(QResizeEvent *);

private:
    QHBoxLayout *m_layout;
    QString m_filePath;
    std::unique_ptr<AbstractEditor> m_editorBackend;
    DTextEdit *m_textEdit;
    BottomBar *m_bottomBar;
    QTextCodec *m_textCodec;
#ifdef USE_WEBENGINE
    MarkdownPreviewWidget *m_markdownPreview = nullptr;
#endif

    EndOfLineMode m_endOfLineMode;
    bool m_isLoadFinished;
    QDateTime m_modified;
    Toast *m_toast;

    bool m_isRefreshing;
    QString m_pendingLoadContent;
    int m_pendingLoadOffset = 0;
    QTimer *m_pendingLoadTimer = nullptr;
    bool m_restorePendingLoadPosition = false;
    int m_pendingRestoreRow = 1;
    int m_pendingRestoreColumn = 0;
    int m_pendingRestoreScrollOffset = 0;
};

#endif
