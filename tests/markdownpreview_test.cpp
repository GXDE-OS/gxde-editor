/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
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

#include "../src/markdownpreview.h"

#include <QObject>
#include <QTest>
#include <QUrl>

class MarkdownPreviewTest : public QObject
{
    Q_OBJECT

private slots:
    void isSupportedFollowsBuild();
    void detectsMarkdownFiles();
    void updatesPreview();
    void ignoresLinkNavigation();
};

class TestableMarkdownPreview : public MarkdownPreview
{
public:
    using MarkdownPreview::doSetSource;
    using QAbstractScrollArea::viewportMargins;
};

void MarkdownPreviewTest::isSupportedFollowsBuild()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QVERIFY(MarkdownPreview::isSupported());
#else
    QVERIFY(!MarkdownPreview::isSupported());
#endif
}

void MarkdownPreviewTest::detectsMarkdownFiles()
{
    QVERIFY(MarkdownPreview::isMarkdownFile(QStringLiteral("/tmp/test.md")));
    QVERIFY(MarkdownPreview::isMarkdownFile(QStringLiteral("/tmp/README.MD")));
    QVERIFY(MarkdownPreview::isMarkdownFile(QStringLiteral("/tmp/doc.markdown")));
    QVERIFY(!MarkdownPreview::isMarkdownFile(QStringLiteral("/tmp/test.txt")));
    QVERIFY(!MarkdownPreview::isMarkdownFile(QStringLiteral("/tmp/test")));
    QVERIFY(!MarkdownPreview::isMarkdownFile(QString()));
}

void MarkdownPreviewTest::updatesPreview()
{
    if (!MarkdownPreview::isSupported()) {
        QSKIP("Markdown preview requires Qt >= 6.5");
    }

    TestableMarkdownPreview preview;
    QCOMPARE(preview.font().pointSizeF(), 12.0);
    QVERIFY(preview.viewportMargins().left() >= 20);
    preview.updatePreview(QStringLiteral("# Title\n\n**bold** text\n\n- item1\n- item2"));

    const QString plain = preview.toPlainText();
    QVERIFY(plain.contains(QStringLiteral("Title")));
    QVERIFY(plain.contains(QStringLiteral("bold")));
    QVERIFY(plain.contains(QStringLiteral("item1")));
    QVERIFY(plain.contains(QStringLiteral("item2")));
    QCOMPARE(preview.document()->defaultFont().pointSizeF(), 12.0);
}

void MarkdownPreviewTest::ignoresLinkNavigation()
{
    TestableMarkdownPreview preview;
    preview.doSetSource(QUrl(QStringLiteral("https://example.com")));
    QVERIFY(preview.source().isEmpty());
}

QTEST_MAIN(MarkdownPreviewTest)

#include "markdownpreview_test.moc"
