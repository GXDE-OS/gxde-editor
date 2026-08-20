#include "../src/markdownlogic.h"

#include <QObject>
#include <QTest>

class MarkdownLogicTest : public QObject
{
    Q_OBJECT

private slots:
    void recognizesMarkdown();
    void appliesViewModeTransitions();
    void resolvesRelativeImagePaths();
    void clampsScrollRatios();
};

void MarkdownLogicTest::recognizesMarkdown()
{
    QVERIFY(MarkdownLogic::isMarkdown(QStringLiteral("README.MD"), QString()));
    QVERIFY(MarkdownLogic::isMarkdown(QStringLiteral("notes.txt"), QStringLiteral("Markdown")));
    QVERIFY(MarkdownLogic::isMarkdown(QStringLiteral("notes.mdown"), QString()));
    QVERIFY(!MarkdownLogic::isMarkdown(QStringLiteral("notes.txt"), QStringLiteral("None")));
}

void MarkdownLogicTest::appliesViewModeTransitions()
{
    QCOMPARE(ViewModeFsm::resolveDefaultMode(true, true), ViewMode::LivePreview);
    QCOMPARE(ViewModeFsm::resolveDefaultMode(true, false), ViewMode::Edit);
    QVERIFY(!ViewModeFsm::canSwitchTo(ViewMode::LivePreview, false, true));
    QCOMPARE(ViewModeFsm::fallbackWhenMarkdownLost(ViewMode::LivePreview), ViewMode::Edit);
    QCOMPARE(ViewModeFsm::elevateWhenMarkdownGained(ViewMode::Edit, true), ViewMode::LivePreview);
    QVERIFY(ViewModeFsm::isReadOnlyTextMode(ViewMode::ReadView, true, false));
}

void MarkdownLogicTest::resolvesRelativeImagePaths()
{
    const QString markdown = QStringLiteral(
        "![local](images/a b.png \"title\")\n![remote](https://example.com/a.png)");
    const QString resolved = MarkdownLogic::resolveImagePaths(markdown, QStringLiteral("/tmp/docs"));

    QVERIFY(resolved.contains(QStringLiteral(
        "![local](file:///tmp/docs/images/a%20b.png \"title\")")));
    QVERIFY(resolved.contains(QStringLiteral("![remote](https://example.com/a.png)")));
}

void MarkdownLogicTest::clampsScrollRatios()
{
    QCOMPARE(ScrollSync::ratioFromScrollBar(50, 0, 100), 0.5);
    QCOMPARE(ScrollSync::ratioFromScrollBar(5, 5, 5), 0.0);
    QCOMPARE(ScrollSync::clampRatio(-0.2), 0.0);
    QCOMPARE(ScrollSync::clampRatio(1.2), 1.0);
}

QTEST_APPLESS_MAIN(MarkdownLogicTest)

#include "markdownlogic_test.moc"
