/*
 * Copyright (C) 2017 ~ 2018 Deepin Technology Co., Ltd.
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

#include "jsonformatter.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QPair>
#include <QRegularExpression>
#include <QStringList>

namespace {

// Guard against pathological documents that would blow up the parser stack.
constexpr int kMaxDepth = 512;

inline QString tr(const char *text)
{
    return QCoreApplication::translate("JsonFormatter", text);
}

struct JsonNode
{
    enum Type {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    Type type = Null;
    // Number: the verbatim literal, Bool: "true"/"false", String: the decoded value.
    QString raw;
    QList<JsonNode> elements;
    QList<QPair<QString, JsonNode>> members;
};

class JsonParser
{
public:
    explicit JsonParser(const QString &text)
        : m_text(text)
    {
    }

    bool parse(JsonNode &root, JsonFormatter::Error *error)
    {
        // Skip an eventual UTF-8/UTF-16 byte order mark.
        if (!m_text.isEmpty() && m_text.at(0).unicode() == 0xFEFF) {
            m_pos = 1;
        }

        skipWhitespace();

        if (m_pos >= m_text.size()) {
            setError(tr("The document does not contain any JSON value."));
            fillError(error);
            return false;
        }

        if (!parseValue(root, 0)) {
            fillError(error);
            return false;
        }

        skipWhitespace();

        if (m_pos < m_text.size()) {
            setError(tr("Unexpected content after the top-level value."));
            fillError(error);
            return false;
        }

        return true;
    }

private:
    void fillError(JsonFormatter::Error *error) const
    {
        if (error == nullptr) {
            return;
        }

        error->hasError = true;
        error->message = m_message;
        error->offset = m_errorOffset;

        int line = 1;
        int column = 1;
        const int limit = qMin(m_errorOffset < 0 ? 0 : m_errorOffset, m_text.size());
        for (int i = 0; i < limit; ++i) {
            if (m_text.at(i) == QLatin1Char('\n')) {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }

        error->line = line;
        error->column = column;
    }

    void setError(const QString &message)
    {
        if (m_message.isEmpty()) {
            m_message = message;
            m_errorOffset = qMin(m_pos, m_text.size() - 1);
        }
    }

    QChar peek() const
    {
        return m_pos < m_text.size() ? m_text.at(m_pos) : QChar();
    }

    void skipWhitespace()
    {
        while (m_pos < m_text.size()) {
            const ushort c = m_text.at(m_pos).unicode();
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++m_pos;
            } else {
                break;
            }
        }
    }

    bool parseValue(JsonNode &node, int depth)
    {
        if (depth > kMaxDepth) {
            setError(tr("The document is nested too deeply."));
            return false;
        }

        skipWhitespace();

        const QChar c = peek();
        if (c.isNull()) {
            setError(tr("Unexpected end of document."));
            return false;
        }

        switch (c.unicode()) {
        case '{':
            return parseObject(node, depth);
        case '[':
            return parseArray(node, depth);
        case '"':
            return parseString(node);
        case 't':
            node.type = JsonNode::Bool;
            return parseLiteral(node, QStringLiteral("true"));
        case 'f':
            node.type = JsonNode::Bool;
            return parseLiteral(node, QStringLiteral("false"));
        case 'n':
            node.type = JsonNode::Null;
            return parseLiteral(node, QStringLiteral("null"));
        default:
            if (c == QLatin1Char('-') || (c >= QLatin1Char('0') && c <= QLatin1Char('9'))) {
                return parseNumber(node);
            }

            setError(tr("Unexpected character '%1'.").arg(c));
            return false;
        }
    }

    bool parseLiteral(JsonNode &node, const QString &literal)
    {
        const QStringView rest = QStringView(m_text).mid(m_pos, literal.size());
        if (rest != QStringView(literal)) {
            setError(tr("Invalid literal, '%1' was expected.").arg(literal));
            return false;
        }

        m_pos += literal.size();
        node.raw = literal;
        return true;
    }

    bool parseNumber(JsonNode &node)
    {
        static const QRegularExpression numberPattern(
            QStringLiteral("-?(?:0|[1-9][0-9]*)(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?"));

        const QRegularExpressionMatch match =
            numberPattern.match(m_text, m_pos, QRegularExpression::NormalMatch,
                                QRegularExpression::AnchorAtOffsetMatchOption);

        if (!match.hasMatch() || match.capturedLength() == 0) {
            setError(tr("Invalid number."));
            return false;
        }

        node.type = JsonNode::Number;
        node.raw = match.captured();
        m_pos += node.raw.size();
        return true;
    }

    bool readHex4(uint &value)
    {
        if (m_pos + 4 > m_text.size()) {
            setError(tr("Incomplete unicode escape sequence."));
            return false;
        }

        value = 0;
        for (int i = 0; i < 4; ++i) {
            const QChar c = m_text.at(m_pos + i);
            int digit = -1;

            if (c >= QLatin1Char('0') && c <= QLatin1Char('9')) {
                digit = c.unicode() - '0';
            } else if (c >= QLatin1Char('a') && c <= QLatin1Char('f')) {
                digit = c.unicode() - 'a' + 10;
            } else if (c >= QLatin1Char('A') && c <= QLatin1Char('F')) {
                digit = c.unicode() - 'A' + 10;
            }

            if (digit < 0) {
                setError(tr("Invalid unicode escape sequence."));
                return false;
            }

            value = value * 16 + static_cast<uint>(digit);
        }

        m_pos += 4;
        return true;
    }

    bool parseString(JsonNode &node)
    {
        // The opening quote has been consumed by the caller.
        ++m_pos;

        QString value;
        bool closed = false;

        while (m_pos < m_text.size()) {
            const QChar c = m_text.at(m_pos++);

            if (c == QLatin1Char('"')) {
                closed = true;
                break;
            }

            if (c != QLatin1Char('\\')) {
                if (c.unicode() < 0x20) {
                    setError(tr("Unescaped control character inside a string."));
                    return false;
                }

                value += c;
                continue;
            }

            if (m_pos >= m_text.size()) {
                break;
            }

            const QChar escape = m_text.at(m_pos++);
            switch (escape.unicode()) {
            case '"':
                value += QLatin1Char('"');
                break;
            case '\\':
                value += QLatin1Char('\\');
                break;
            case '/':
                value += QLatin1Char('/');
                break;
            case 'b':
                value += QLatin1Char('\b');
                break;
            case 'f':
                value += QLatin1Char('\f');
                break;
            case 'n':
                value += QLatin1Char('\n');
                break;
            case 'r':
                value += QLatin1Char('\r');
                break;
            case 't':
                value += QLatin1Char('\t');
                break;
            case 'u': {
                uint code = 0;
                if (!readHex4(code)) {
                    return false;
                }

                // Decode an eventual UTF-16 surrogate pair.
                if (code >= 0xD800 && code <= 0xDBFF && m_pos + 1 < m_text.size()
                    && m_text.at(m_pos) == QLatin1Char('\\')
                    && m_text.at(m_pos + 1) == QLatin1Char('u')) {
                    const int savedPos = m_pos;
                    m_pos += 2;

                    uint low = 0;
                    if (!readHex4(low)) {
                        return false;
                    }

                    if (low >= 0xDC00 && low <= 0xDFFF) {
                        code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                    } else {
                        m_pos = savedPos;
                    }
                }

                value += QChar::fromUcs4(static_cast<char32_t>(code));
                break;
            }
            default:
                setError(tr("Invalid escape sequence '\\%1'.").arg(escape));
                return false;
            }
        }

        if (!closed) {
            setError(tr("Unterminated string."));
            return false;
        }

        node.type = JsonNode::String;
        node.raw = value;
        return true;
    }

    bool parseArray(JsonNode &node, int depth)
    {
        ++m_pos;  // '['
        node.type = JsonNode::Array;

        skipWhitespace();
        if (peek() == QLatin1Char(']')) {
            ++m_pos;
            return true;
        }

        while (true) {
            JsonNode element;
            if (!parseValue(element, depth + 1)) {
                return false;
            }
            node.elements.append(element);

            skipWhitespace();
            const QChar c = peek();
            if (c == QLatin1Char(',')) {
                ++m_pos;
                continue;
            }
            if (c == QLatin1Char(']')) {
                ++m_pos;
                return true;
            }

            setError(c.isNull() ? tr("Unterminated array, ']' is missing.")
                                : tr("Expected ',' or ']' inside the array."));
            return false;
        }
    }

    bool parseObject(JsonNode &node, int depth)
    {
        ++m_pos;  // '{'
        node.type = JsonNode::Object;

        skipWhitespace();
        if (peek() == QLatin1Char('}')) {
            ++m_pos;
            return true;
        }

        while (true) {
            skipWhitespace();
            if (peek() != QLatin1Char('"')) {
                setError(tr("Expected a quoted object key."));
                return false;
            }

            JsonNode key;
            if (!parseString(key)) {
                return false;
            }

            skipWhitespace();
            if (peek() != QLatin1Char(':')) {
                setError(tr("Expected ':' after the object key."));
                return false;
            }
            ++m_pos;

            JsonNode value;
            if (!parseValue(value, depth + 1)) {
                return false;
            }

            node.members.append(qMakePair(key.raw, value));

            skipWhitespace();
            const QChar c = peek();
            if (c == QLatin1Char(',')) {
                ++m_pos;
                continue;
            }
            if (c == QLatin1Char('}')) {
                ++m_pos;
                return true;
            }

            setError(c.isNull() ? tr("Unterminated object, '}' is missing.")
                                : tr("Expected ',' or '}' inside the object."));
            return false;
        }
    }

    QString m_text;
    int m_pos = 0;
    QString m_message;
    int m_errorOffset = -1;
};

class JsonWriter
{
public:
    explicit JsonWriter(const QString &indent)
        : m_indent(indent)
    {
    }

    QString write(const JsonNode &root) const
    {
        QString out;
        writeValue(root, 0, out);
        return out;
    }

private:
    void writeIndent(int level, QString &out) const
    {
        for (int i = 0; i < level; ++i) {
            out += m_indent;
        }
    }

    static void writeString(const QString &value, QString &out)
    {
        out += QLatin1Char('"');

        for (const QChar c : value) {
            switch (c.unicode()) {
            case '"':
                out += QStringLiteral("\\\"");
                break;
            case '\\':
                out += QStringLiteral("\\\\");
                break;
            case '\b':
                out += QStringLiteral("\\b");
                break;
            case '\f':
                out += QStringLiteral("\\f");
                break;
            case '\n':
                out += QStringLiteral("\\n");
                break;
            case '\r':
                out += QStringLiteral("\\r");
                break;
            case '\t':
                out += QStringLiteral("\\t");
                break;
            default:
                if (c.unicode() < 0x20) {
                    out += QStringLiteral("\\u%1").arg(c.unicode(), 4, 16, QLatin1Char('0'));
                } else {
                    out += c;
                }
            }
        }

        out += QLatin1Char('"');
    }

    void writeValue(const JsonNode &node, int level, QString &out) const
    {
        switch (node.type) {
        case JsonNode::Null:
            out += QStringLiteral("null");
            break;
        case JsonNode::Bool:
        case JsonNode::Number:
            out += node.raw;
            break;
        case JsonNode::String:
            writeString(node.raw, out);
            break;
        case JsonNode::Array:
            if (node.elements.isEmpty()) {
                out += QStringLiteral("[]");
                break;
            }

            out += QLatin1Char('[');
            out += QLatin1Char('\n');
            for (int i = 0; i < node.elements.size(); ++i) {
                writeIndent(level + 1, out);
                writeValue(node.elements.at(i), level + 1, out);
                if (i + 1 < node.elements.size()) {
                    out += QLatin1Char(',');
                }
                out += QLatin1Char('\n');
            }
            writeIndent(level, out);
            out += QLatin1Char(']');
            break;
        case JsonNode::Object:
            if (node.members.isEmpty()) {
                out += QStringLiteral("{}");
                break;
            }

            out += QLatin1Char('{');
            out += QLatin1Char('\n');
            for (int i = 0; i < node.members.size(); ++i) {
                writeIndent(level + 1, out);
                writeString(node.members.at(i).first, out);
                out += QStringLiteral(": ");
                writeValue(node.members.at(i).second, level + 1, out);
                if (i + 1 < node.members.size()) {
                    out += QLatin1Char(',');
                }
                out += QLatin1Char('\n');
            }
            writeIndent(level, out);
            out += QLatin1Char('}');
            break;
        }
    }

    QString m_indent;
};

}  // namespace

QString JsonFormatter::indentUnit(int tabSpaceNumber, bool useTabs)
{
    if (useTabs) {
        return QStringLiteral("\t");
    }

    return QString(qBound(1, tabSpaceNumber, 24), QLatin1Char(' '));
}

bool JsonFormatter::isJsonFileName(const QString &filePath)
{
    static const QStringList suffixes {
        QStringLiteral(".json"),
        QStringLiteral(".jsonc"),
        QStringLiteral(".geojson"),
        QStringLiteral(".webmanifest"),
        QStringLiteral(".har"),
    };

    const QString lower = QFileInfo(filePath).fileName().toLower();
    for (const QString &suffix : suffixes) {
        if (lower.endsWith(suffix)) {
            return true;
        }
    }

    return false;
}

bool JsonFormatter::isJsonSyntaxName(const QString &syntaxDefinitionName)
{
    return syntaxDefinitionName.compare(QStringLiteral("JSON"), Qt::CaseInsensitive) == 0;
}

bool JsonFormatter::isJsonDocument(const QString &filePath, const QString &syntaxDefinitionName)
{
    return isJsonSyntaxName(syntaxDefinitionName) || isJsonFileName(filePath);
}

QString JsonFormatter::format(const QString &text, const QString &indent, Error *error)
{
    if (error != nullptr) {
        *error = Error();
    }

    JsonNode root;
    JsonParser parser(text);

    if (!parser.parse(root, error)) {
        return QString();
    }

    return JsonWriter(indent).write(root);
}

bool JsonFormatter::isValid(const QString &text, Error *error)
{
    if (error != nullptr) {
        *error = Error();
    }

    JsonNode root;
    return JsonParser(text).parse(root, error);
}
