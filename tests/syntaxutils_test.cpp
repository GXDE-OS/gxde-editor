#include "../src/syntaxutils.h"

#include <QObject>
#include <QTest>

#include <KF5/KSyntaxHighlighting/KSyntaxHighlighting/definition.h>

class SyntaxUtilsTest : public QObject
{
    Q_OBJECT

private slots:
    void detectsSyntaxDefinitionFromShebang();
    void defersHighlightingForLargeDocuments();
};

void SyntaxUtilsTest::detectsSyntaxDefinitionFromShebang()
{
    KSyntaxHighlighting::Repository repository;
    const auto pythonDefinition = repository.definitionForMimeType(QStringLiteral("text/x-python"));

    QVERIFY2(pythonDefinition.isValid(), "Python syntax definition must be available for the shebang test.");

    const QString detected = SyntaxUtils::detectSyntaxDefinitionName(repository,
                                                                     QStringLiteral("/tmp/script"),
                                                                     QStringLiteral("#!/usr/bin/env python3\nprint('hello')\n"));

    QCOMPARE(detected, pythonDefinition.name());
}

void SyntaxUtilsTest::defersHighlightingForLargeDocuments()
{
    QVERIFY(!SyntaxUtils::shouldDeferSyntaxHighlight(4096));
    QVERIFY(SyntaxUtils::shouldDeferSyntaxHighlight(2 * 1024 * 1024));
}

QTEST_APPLESS_MAIN(SyntaxUtilsTest)

#include "syntaxutils_test.moc"
