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
#include "markdownpreview.h"
#include "syntaxutils.h"
#include "utils.h"
#include <unistd.h>

#include <QCoreApplication>
#include <QApplication>
#include <QSaveFile>
#include <QScrollBar>
#include <QScroller>
#include <QDebug>
#include <QSplitter>
#include <QTimer>
#include <QDir>

#include "drecentmanager.h"

DCORE_USE_NAMESPACE

EditWrapper::EditWrapper(QWidget *parent)
    : QWidget(parent),
      m_textEdit(new DTextEdit),
      m_bottomBar(new BottomBar(this)),
      m_textCodec(QTextCodec::codecForName("UTF-8")),
      m_endOfLineMode(eolUnix),
      m_isLoadFinished(true),
      m_toast(new Toast(this)),
      m_isRefreshing(false)
{
    m_pendingLoadTimer = new QTimer(this);
    m_pendingLoadTimer->setInterval(0);

    // 编辑器容器：行号区 + 文本编辑区，保持原有布局。
    QWidget *editorContainer = new QWidget(this);
    QHBoxLayout *editorLayout = new QHBoxLayout(editorContainer);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);
    editorLayout->addWidget(m_textEdit->lineNumberArea);
    editorLayout->addWidget(m_textEdit);

    m_bottomBar->setHighlightMenu(m_textEdit->getHighlightMenu());
    m_textEdit->setWrapper(this);

    // 编辑区与 Markdown 预览分栏，默认只显示编辑区。
    m_editSplitter = new QSplitter(Qt::Horizontal, this);
    m_editSplitter->setObjectName("EditSplitter");
    m_editSplitter->setChildrenCollapsible(false);
    m_editSplitter->setHandleWidth(6);
    // 让编辑区与预览区之间的分隔手柄可见，深/浅色主题下都使用调色板角色自适应。
    m_editSplitter->setStyleSheet(QStringLiteral(
        "QSplitter::handle { background: palette(mid); }"
        "QSplitter::handle:hover { background: palette(highlight); }"
        "QSplitter::handle:pressed { background: palette(dark); }"));
    m_editSplitter->addWidget(editorContainer);
    m_editSplitter->setStretchFactor(0, 1);

    // Qt < 6.5 构建下预览不可用（QTextDocument::setMarkdown 需要 Qt >= 6.5）。
    if (MarkdownPreview::isSupported()) {
        m_markdownPreview = new MarkdownPreview(this);
        m_markdownPreview->setVisible(false);
        m_editSplitter->addWidget(m_markdownPreview);
        m_editSplitter->setStretchFactor(1, 1);

        // 编辑内容变化时实时刷新预览。
        connect(m_textEdit, &DTextEdit::textChanged, this, [this] {
            if (m_markdownPreview != nullptr && m_markdownPreview->isVisible()) {
                m_markdownPreview->updatePreview(m_textEdit->toPlainText());
            }
        });
    }

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_editSplitter);
    mainLayout->addWidget(m_bottomBar);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    setLayout(mainLayout);

    m_toast->setOnlyShow(true);
    m_toast->setIcon(":/images/warning.svg");

    connect(m_textEdit, &DTextEdit::cursorModeChanged, this, &EditWrapper::handleCursorModeChanged);
    connect(m_textEdit, &DTextEdit::hightlightChanged, this, &EditWrapper::handleHightlightChanged);
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
    delete m_textEdit;
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
    // use QSaveFile for safely save files.
    QSaveFile saveFile(m_textEdit->filepath);
    saveFile.setDirectWriteFallback(true);

    if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    QFile file(m_textEdit->filepath);
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
    stream << m_textEdit->toPlainText().replace(eolRegex, eol);

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
        m_textEdit->setModified(false);
        m_isLoadFinished = true;
    }

    qDebug() << "Saved file:" << m_textEdit->filepath
             << "with codec:" << m_textCodec->name()
             << "Line Endings:" << m_endOfLineMode
             << "State:" << ok;

    return ok;
}

void EditWrapper::updatePath(const QString &file)
{
    QFileInfo fi(file);
    m_modified = fi.lastModified();

    m_textEdit->filepath = file;
    detectEndOfLine();

    // Markdown 文件打开时自动显示可视化预览。
    if (m_markdownPreview != nullptr) {
        setMarkdownPreviewVisible(MarkdownPreview::isMarkdownFile(file));
    }
}

void EditWrapper::setMarkdownPreviewVisible(bool visible)
{
    if (m_markdownPreview == nullptr) {
        return;
    }

    m_markdownPreview->setVisible(visible);
    if (visible) {
        m_markdownPreview->updatePreview(m_textEdit->toPlainText());

        // 初次显示时编辑区与预览区各占一半。
        if (m_editSplitter != nullptr && m_editSplitter->width() > 0) {
            const int total = m_editSplitter->width();
            m_editSplitter->setSizes({total / 2, total - total / 2});
        }
    }
}

bool EditWrapper::isMarkdownPreviewVisible() const
{
    return m_markdownPreview != nullptr && m_markdownPreview->isVisible();
}

void EditWrapper::refresh()
{
    if (filePath().isEmpty() || Utils::isDraftFile(filePath()) || m_isRefreshing) {
        return;
    }

    QFile file(filePath());
    int curPos = m_textEdit->textCursor().position();
    int yoffset = m_textEdit->verticalScrollBar()->value();
    int xoffset = m_textEdit->horizontalScrollBar()->value();

    if (file.open(QIODevice::ReadOnly)) {
        m_isRefreshing = true;

        QTextStream out(&file);
        QString content = out.readAll();

        m_textEdit->setPlainText(QString());
        m_textEdit->setPlainText(content);
        m_textEdit->setModified(false);

        QTextCursor textcur = m_textEdit->textCursor();
        textcur.setPosition(curPos);
        m_textEdit->setTextCursor(textcur);
        m_textEdit->verticalScrollBar()->setValue(yoffset);
        m_textEdit->horizontalScrollBar()->setValue(xoffset);

        QFileInfo fi(filePath());
        m_modified = fi.lastModified();

        file.close();
        m_toast->hideAnimation();

        m_textEdit->setUpdatesEnabled(false);

        QTimer::singleShot(10, this, [=] {
            m_textEdit->setUpdatesEnabled(true);
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
    if (Utils::isDraftFile(m_textEdit->filepath))
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
    QFile file(m_textEdit->filepath);

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
    m_bottomBar->setHightlightName(name);
}

void EditWrapper::setDarkTheme(bool enabled)
{
    Q_UNUSED(enabled)
}

void EditWrapper::handleFileLoadFinished(const QByteArray &encode, const QString &content)
{
    // restore mouse style.
    // QApplication::restoreOverrideCursor();

    qDebug() << "load finished: " << m_textEdit->filepath << ", " << encode << "endOfLine: " << m_endOfLineMode;

    if (!Utils::isDraftFile(m_textEdit->filepath)) {
        DRecentData data;
        data.appName = "Deepin Editor";
        data.appExec = "deepin-editor";
        DRecentManager::addItem(m_textEdit->filepath, data);
    }

    setTextCodec(encode);
    m_bottomBar->setEncodeName(m_textCodec->name());

    if (SyntaxUtils::shouldLoadTextIncrementally(content.size())) {
        m_pendingLoadContent = content;
        m_pendingLoadOffset = 0;
        m_textEdit->clear();
        m_textEdit->beginBulkLoad();
        m_pendingLoadTimer->start();
        return;
    }

    m_isLoadFinished = true;
    m_textEdit->setPlainText(content);
    m_textEdit->setModified(false);
    m_textEdit->moveToStart();
    QTimer::singleShot(100, this, [=] { m_textEdit->loadHighlighter(); });
}

void EditWrapper::appendPendingTextLoadChunk()
{
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
    m_pendingLoadTimer->stop();
    m_textEdit->endBulkLoad();
    m_textEdit->setModified(false);
    m_textEdit->moveToStart();
    m_isLoadFinished = true;

    m_pendingLoadContent.clear();
    m_pendingLoadOffset = 0;

    QTimer::singleShot(100, this, [=] { m_textEdit->loadHighlighter(); });
}

void EditWrapper::resizeEvent(QResizeEvent *e)
{
    initToastPosition();

    QWidget::resizeEvent(e);
}
