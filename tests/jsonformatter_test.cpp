#include "../src/jsonformatter.h"

#include <QObject>
#include <QTest>

class JsonFormatterTest : public QObject
{
    Q_OBJECT

private slots:
    void prettyPrintsMinifiedObject();
    void keepsMemberOrderAndNumberLiterals();
    void handlesNestedAndEmptyContainers();
    void keepsUnicodeAndEscapes();
    void reportsInvalidDocuments();
    void detectsJsonDocuments();
    void buildsIndentUnit();
};

void JsonFormatterTest::prettyPrintsMinifiedObject()
{
    const QString source = QStringLiteral("{\"name\":\"gxde\",\"version\":2}");
    const QString expected = QStringLiteral("{\n"
                                            "    \"name\": \"gxde\",\n"
                                            "    \"version\": 2\n"
                                            "}");

    JsonFormatter::Error error;
    QCOMPARE(JsonFormatter::format(source, JsonFormatter::indentUnit(4), &error), expected);
    QVERIFY(!error.hasError);
}

void JsonFormatterTest::keepsMemberOrderAndNumberLiterals()
{
    // The formatter must not reorder members nor rewrite numbers through a double.
    const QString source = QStringLiteral("{\"z\":12345678901234567890,\"a\":1.500,\"m\":1e-7}");
    const QString expected = QStringLiteral("{\n"
                                            "    \"z\": 12345678901234567890,\n"
                                            "    \"a\": 1.500,\n"
                                            "    \"m\": 1e-7\n"
                                            "}");

    QCOMPARE(JsonFormatter::format(source, JsonFormatter::indentUnit(4)), expected);
}

void JsonFormatterTest::handlesNestedAndEmptyContainers()
{
    const QString source = QStringLiteral("{\"b\":[1,2],\"a\":{\"c\":true,\"d\":null,\"e\":{},\"f\":[]}}");
    const QString expected = QStringLiteral("{\n"
                                            "    \"b\": [\n"
                                            "        1,\n"
                                            "        2\n"
                                            "    ],\n"
                                            "    \"a\": {\n"
                                            "        \"c\": true,\n"
                                            "        \"d\": null,\n"
                                            "        \"e\": {},\n"
                                            "        \"f\": []\n"
                                            "    }\n"
                                            "}");

    const QString formatted = JsonFormatter::format(source, JsonFormatter::indentUnit(4));
    QCOMPARE(formatted, expected);

    // Formatting twice must be a no-op.
    QCOMPARE(JsonFormatter::format(formatted, JsonFormatter::indentUnit(4)), formatted);
}

void JsonFormatterTest::keepsUnicodeAndEscapes()
{
    const QString source = QStringLiteral("{\"a\":\"\\u4f60\\u597d\",\"b\":\"x\\ty\"}");
    const QString expected = QStringLiteral("{\n"
                                            "    \"a\": \"你好\",\n"
                                            "    \"b\": \"x\\ty\"\n"
                                            "}");

    QCOMPARE(JsonFormatter::format(source, JsonFormatter::indentUnit(2)), QStringLiteral("{\n"
                                                                                          "  \"a\": \"你好\",\n"
                                                                                          "  \"b\": \"x\\ty\"\n"
                                                                                          "}"));
    QCOMPARE(JsonFormatter::format(source, JsonFormatter::indentUnit(4)), expected);
}

void JsonFormatterTest::reportsInvalidDocuments()
{
    JsonFormatter::Error error;

    QVERIFY(!JsonFormatter::isValid(QStringLiteral(""), &error));
    QVERIFY(error.hasError);

    QVERIFY(!JsonFormatter::isValid(QStringLiteral("{\"a\": }"), &error));
    QCOMPARE(error.line, 1);
    QCOMPARE(error.column, 7);

    QVERIFY(!JsonFormatter::isValid(QStringLiteral("{\"a\": 1,}"), &error));
    QVERIFY(error.hasError);

    QVERIFY(!JsonFormatter::isValid(QStringLiteral("{\"a\" 1}"), &error));
    QVERIFY(error.hasError);

    QVERIFY(JsonFormatter::format(QStringLiteral("{\"a\": 1}"), QStringLiteral("    "), &error)
            == QStringLiteral("{\n    \"a\": 1\n}"));
    QVERIFY(!error.hasError);
}

void JsonFormatterTest::detectsJsonDocuments()
{
    QVERIFY(JsonFormatter::isJsonDocument(QStringLiteral("/tmp/package.json"), QString()));
    QVERIFY(JsonFormatter::isJsonDocument(QStringLiteral("/tmp/no-suffix"), QStringLiteral("JSON")));
    QVERIFY(!JsonFormatter::isJsonDocument(QStringLiteral("/tmp/foo.cpp"), QStringLiteral("C++")));
}

void JsonFormatterTest::buildsIndentUnit()
{
    QCOMPARE(JsonFormatter::indentUnit(4), QStringLiteral("    "));
    QCOMPARE(JsonFormatter::indentUnit(2), QStringLiteral("  "));
    QCOMPARE(JsonFormatter::indentUnit(4, true), QStringLiteral("\t"));
}

QTEST_APPLESS_MAIN(JsonFormatterTest)

#include "jsonformatter_test.moc"
