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

#ifndef JSONFORMATTER_H
#define JSONFORMATTER_H

#include <QString>

/**
 * \brief Turns arbitrary (possibly minified or messy) JSON into a canonical,
 *        nicely indented document.
 *
 * The formatter uses its own recursive descent parser instead of
 * QJsonDocument on purpose:
 *
 *  - the original order of the object members is kept;
 *  - numbers are re-emitted verbatim, so big integers and high precision
 *    floating point values never lose precision;
 *  - a precise line/column is reported when the document is not valid JSON.
 */
class JsonFormatter
{
public:
    struct Error
    {
        bool hasError = false;
        QString message;
        int offset = -1;  // character offset inside the source document.
        int line = 0;     // 1 based, 0 when unknown.
        int column = 0;   // 1 based, 0 when unknown.
    };

    /** \brief One indentation level, either a single TAB or \a tabSpaceNumber spaces. */
    static QString indentUnit(int tabSpaceNumber, bool useTabs = false);

    /** \brief True when the file name looks like a JSON file. */
    static bool isJsonFileName(const QString &filePath);

    /** \brief True when the syntax definition used for highlighting is JSON. */
    static bool isJsonSyntaxName(const QString &syntaxDefinitionName);

    /** \brief True when the document should be handled as JSON. */
    static bool isJsonDocument(const QString &filePath, const QString &syntaxDefinitionName);

    /**
     * \brief Pretty prints \a text.
     *
     * \return the formatted document, or a null string when \a text is not
     *         valid JSON (in that case \a error is filled in).
     */
    static QString format(const QString &text, const QString &indent, Error *error = nullptr);

    /** \brief Convenience helper that only checks the syntax of \a text. */
    static bool isValid(const QString &text, Error *error = nullptr);
};

#endif
