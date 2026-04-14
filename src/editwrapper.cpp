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

#include "widgets/toast.h"
#include "fileloadthread.h"
#include "editwrapper.h"
#include "editor/editorfactory.h"
#include "syntaxutils.h"
#include "utils.h"
#ifdef USE_WEBENGINE
#include "widgets/markdownpreviewwidget.h"
#endif
#include <unistd.h>

#include <QCoreApplication>
#include <QApplication>
#include <QSaveFile>
#include <QScrollBar>
#include <QScroller>
#include <QDebug>
#include <QTimer>
#include <QDir>

#include "drecentmanager.h"

DCORE_USE_NAMESPACE

EditWrapper::EditWrapper(QWidget *parent)
    : EditWrapper(EditorFactory::create(QStringLiteral("legacy")), parent)
{
}

EditWrapper::EditWrapper(std::unique_ptr<AbstractEditor> editorBackend, QWidget *parent)
    : QWidget(parent),
      m_layout(new QHBoxLayout),
      m_editorBackend(std::move(editorBackend)),
      m_textEdit(qobject_cast<DTextEdit *>(m_editorBackend ? m_editorBackend->widget() : nullptr)),
      m_bottomBar(new BottomBar(this)),
      m_textCodec(QTextCodec::codecForName("UTF-8")),
      m_endOfLineMode(eolUnix),
      m_isLoadFinished(true),
      m_toast(new Toast(this)),
      m_isRefreshing(false)
{
    m_pendingLoadTimer = new QTimer(this);
    m_pendingLoadTimer->setInterval(0);

#ifdef USE_WEBENGINE
    // 判断是否支持 Markdown 预览，如果支持则显示
    if (MarkdownPreviewWidget::isSupport()) {
        m_markdownPreview = new MarkdownPreviewWidget();
    }
#endif

    // Init layout and widgets.
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    if (m_textEdit) {
        m_layout->addWidget(m_textEdit->lineNumberArea);
    }
    if (QWidget *widget = editorWidget()) {
        m_layout->addWidget(widget);
    }
#ifdef USE_WEBENGINE
    // 加载 Markdown 预览框
    if (m_markdownPreview) {
        m_layout->addWidget(m_markdownPreview);
        m_markdownPreview->setVisible(false);
        m_markdownPreview->setSourceEditor(NULL);
        m_layout->setStretch(1, 1);
        m_layout->setStretch(2, 1);
    }
#endif

    if (m_textEdit) {
        m_bottomBar->setHighlightMenu(m_textEdit->getHighlightMenu());
        m_textEdit->setWrapper(this);
    }

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(m_layout);
    mainLayout->addWidget(m_bottomBar);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    setLayout(mainLayout);

    m_toast->setOnlyShow(true);
    m_toast->setIcon(":/images/warning.svg");

    if (m_textEdit) {
        connect(m_textEdit, &DTextEdit::cursorModeChanged, this, &EditWrapper::handleCursorModeChanged);
        connect(m_textEdit, &DTextEdit::hightlightChanged, this, &EditWrapper::handleHightlightChanged);
    }
    connect(m_toast, &Toast::reloadBtnClicked, this, &EditWrapper::refresh);
    connect(m_toast, &Toast::closeBtnClicked, this, [=] {
        QFileInfo fi(filePath());
        m_modified = fi.lastModified();
    });

    connect(m_toast, &Toast::saveAsBtnClicked, this, &EditWrapper::requestSaveAs);
    connect(m_pendingLoadTimer, &QTimer::timeout, this, &EditWrapper::appendPendingTextLoadChunk);

    setDarkTheme(DThemeManager::instance()->theme() == "dark");
}

EditWrapper::~EditWrapper()
{
    delete m_toast;
}

void EditWrapper::openFile(const QString &filepath)
{
    // update file path.
    updatePath(filepath);
    detectEndOfLine();

    m_isLoadFinished = false;

    // begin to load the file.
    FileLoadThread *thread = new FileLoadThread(filepath);
    connect(thread, &FileLoadThread::loadFinished, this, &EditWrapper::handleFileLoadFinished);
    connect(thread, &FileLoadThread::finished, thread, &FileLoadThread::deleteLater);

    // start the thread.
    thread->start();
}

bool EditWrapper::saveFile()
{
    if (!m_editorBackend) {
        return false;
    }

    // use QSaveFile for safely save files.
    QSaveFile saveFile(filePath());
    saveFile.setDirectWriteFallback(true);

    if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    QFile file(filePath());
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QRegularExpression eolRegex("\r?\n|\r");
    QString eol = QStringLiteral("\n");
    if (m_endOfLineMode == eolDos) {
        eol = QStringLiteral("\r\n");
    } else if (m_endOfLineMode == eolMac) {
        eol = QStringLiteral("\r");
    }

    QTextStream stream(&file);
    stream.setCodec(m_textCodec);
    stream << m_editorBackend->text().replace(eolRegex, eol);

    // flush stream.
    stream.flush();

    // close and delete file.
    file.close();

    // flush file.
    if (!saveFile.flush()) {
        return false;
    }

    // ensure that the file is written to disk
    fsync(saveFile.handle());

    QFileInfo fi(filePath());
    m_modified = fi.lastModified();

    // did save work?
    // only finalize if stream status == OK
    bool ok = (stream.status() == QTextStream::Ok);

    // update status.
    if (ok) {
        m_editorBackend->setModified(false);
        m_isLoadFinished = true;
    }

    qDebug() << "Saved file:" << filePath()
             << "with codec:" << m_textCodec->name()
             << "Line Endings:" << m_endOfLineMode
             << "State:" << ok;

    return ok;
}

void EditWrapper::updatePath(const QString &file)
{
    QFileInfo fi(file);
    m_modified = fi.lastModified();
    m_filePath = file;

    if (m_textEdit) {
        m_textEdit->filepath = file;
    }
    detectEndOfLine();
}

void EditWrapper::refresh()
{
    if (!m_editorBackend || filePath().isEmpty() || Utils::isDraftFile(filePath()) || m_isRefreshing) {
        return;
    }

    QFile file(filePath());
    int row = m_editorBackend->currentLine();
    int column = m_editorBackend->currentColumn();
    int yoffset = m_editorBackend->scrollOffset();

    if (file.open(QIODevice::ReadOnly)) {
        m_isRefreshing = true;

        QTextStream out(&file);
        out.setCodec(m_textCodec);
        QString content = out.readAll();

        m_editorBackend->setText(content);
        m_editorBackend->setModified(false);
        m_editorBackend->scrollToLine(yoffset, row, column);

        QFileInfo fi(filePath());
        m_modified = fi.lastModified();

        file.close();
        m_toast->hideAnimation();

        if (QWidget *widget = editorWidget()) {
            widget->setUpdatesEnabled(false);
        }

        QTimer::singleShot(10, this, [=] {
            if (QWidget *widget = editorWidget()) {
                widget->setUpdatesEnabled(true);
            }
            m_isRefreshing = false;
        });
    } else {
        m_isRefreshing = false;
    }
}

EditWrapper::EndOfLineMode EditWrapper::endOfLineMode()
{
    return m_endOfLineMode;
}

void EditWrapper::setEndOfLineMode(EndOfLineMode eol)
{
    m_endOfLineMode = eol;
}

void EditWrapper::setTextCodec(QTextCodec *codec, bool reload)
{
    m_textCodec = codec;

    if (!reload)
        return;

    refresh();

    // TODO: enforce bom for some encodings
}

void EditWrapper::setTextCodec(QByteArray encodeName, bool reload)
{
    QTextCodec* codec = QTextCodec::codecForName(encodeName);

    if (!codec) {
        qWarning() << "Codec for" << encodeName << "not found! Fallback to UTF-8";
        codec = QTextCodec::codecForName("UTF-8");
    }

    setTextCodec(codec);
}

void EditWrapper::hideToast()
{
    if (m_toast->isVisible()) {
        m_toast->hideAnimation();
    }
}

void EditWrapper::checkForReload()
{
    if (filePath().isEmpty() || Utils::isDraftFile(filePath()))
        return;

    QFileInfo fi(filePath());

    if (fi.lastModified() == m_modified || m_toast->isVisible())
        return;

    if (fi.exists()) {
        m_toast->setText(tr("File has changed on disk. Reload?"));
        m_toast->setReloadState(true);
    } else {
        m_toast->setText(tr("File removed on the disk. Save it now?"));
        m_toast->setReloadState(false);
    }

    initToastPosition();
    m_toast->showAnimation();
}

void EditWrapper::initToastPosition()
{
    int avaliableHeight = this->height() - m_toast->height() + m_bottomBar->height();

    int toastPaddingBottom = qMin(avaliableHeight / 2, 100);
    m_toast->adjustSize();
    m_toast->setFixedWidth(this->width() / 2);
    m_toast->move((this->width() - m_toast->width()) / 2,
                  avaliableHeight - toastPaddingBottom);
}

void EditWrapper::detectEndOfLine()
{
    if (filePath().isEmpty()) {
        return;
    }

    QFile file(filePath());

    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QByteArray line = file.readLine();
    if (line.indexOf("\r\n") != -1) {
        m_endOfLineMode = eolDos;
    } else if (line.indexOf("\r") != -1) {
        m_endOfLineMode = eolMac;
    } else {
        m_endOfLineMode = eolUnix;
    }

    file.close();
}

void EditWrapper::handleCursorModeChanged(DTextEdit::CursorMode mode)
{
    switch (mode) {
    case DTextEdit::Insert:
        m_bottomBar->setCursorStatus(tr("INSERT"));
        break;
    case DTextEdit::Overwrite:
        m_bottomBar->setCursorStatus(tr("OVERWRITE"));
        break;
    case DTextEdit::Readonly:
        m_bottomBar->setCursorStatus(tr("R/O"));
        break;
    default:
        break;
    }
}

void EditWrapper::handleHightlightChanged(const QString &name)
{
#ifdef USE_WEBENGINE
    if (m_markdownPreview) {
        m_markdownPreview->setVisible(name == "Markdown");
        m_markdownPreview->setSourceEditor(name == "Markdown" ? qobject_cast<QTextEdit *>(m_textEdit) : NULL);
    }
#endif
    m_bottomBar->setHightlightName(name);
}

void EditWrapper::setDarkTheme(bool enabled)
{
#ifdef USE_WEBENGINE
    if (m_markdownPreview) {
        m_markdownPreview->setDarkTheme(enabled);
    }
#endif
}

void EditWrapper::handleFileLoadFinished(const QByteArray &encode, const QString &content)
{
    if (!m_editorBackend) {
        m_isLoadFinished = true;
        return;
    }

    // restore mouse style.
    // QApplication::restoreOverrideCursor();

    qDebug() << "load finished: " << filePath() << ", " << encode << "endOfLine: " << m_endOfLineMode;

    if (!Utils::isDraftFile(filePath())) {
        DRecentData data;
        data.appName = "Deepin Editor";
        data.appExec = "deepin-editor";
        DRecentManager::addItem(filePath(), data);
    }

    setTextCodec(encode);
    m_bottomBar->setEncodeName(m_textCodec->name());

    if (SyntaxUtils::shouldLoadTextIncrementally(content.size())) {
        m_pendingLoadContent = content;
        m_pendingLoadOffset = 0;
        if (m_textEdit) {
            m_textEdit->clear();
            m_editorBackend->beginBulkLoad();
            m_pendingLoadTimer->start();
            return;
        }
    }

    m_isLoadFinished = true;
    m_editorBackend->setText(content);
    m_editorBackend->setModified(false);
    m_editorBackend->moveToStart();
    QTimer::singleShot(100, this, [=] { m_editorBackend->loadHighlighter(); });
}

void EditWrapper::appendPendingTextLoadChunk()
{
    if (!m_textEdit) {
        return;
    }

    const int chunkSize = SyntaxUtils::incrementalTextLoadChunkSize();
    const QString chunk = m_pendingLoadContent.mid(m_pendingLoadOffset, chunkSize);

    QTextCursor cursor(m_textEdit->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(chunk);

    m_pendingLoadOffset += chunk.size();

    if (m_pendingLoadOffset >= m_pendingLoadContent.size()) {
        finishPendingTextLoad();
    }
}

void EditWrapper::finishPendingTextLoad()
{
    if (!m_textEdit) {
        m_pendingLoadTimer->stop();
        m_pendingLoadContent.clear();
        m_pendingLoadOffset = 0;
        m_isLoadFinished = true;
        return;
    }

    m_pendingLoadTimer->stop();
    m_editorBackend->endBulkLoad();
    m_editorBackend->setModified(false);
    m_editorBackend->moveToStart();
    m_isLoadFinished = true;

    m_pendingLoadContent.clear();
    m_pendingLoadOffset = 0;

    QTimer::singleShot(100, this, [=] { m_editorBackend->loadHighlighter(); });
}

void EditWrapper::resizeEvent(QResizeEvent *e)
{
    initToastPosition();

    QWidget::resizeEvent(e);
}
