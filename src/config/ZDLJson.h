/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <QString>
#include <QStringList>
#include <yyjson.h>

/**
 * Thin RAII layer over the yyjson C API.  This is the only place in qZDL that
 * talks to yyjson_* directly; everything else works through Doc/Builder and the
 * null tolerant accessors below.
 */
namespace ZDLJson {

/** Owns an immutable parsed document. */
class Doc {
public:
    Doc() = default;

    explicit Doc(yyjson_doc *doc) : doc(doc) {}

    ~Doc();

    Doc(const Doc &) = delete;

    Doc &operator=(const Doc &) = delete;

    Doc(Doc &&other) noexcept;

    Doc &operator=(Doc &&other) noexcept;

    [[nodiscard]] bool isValid() const {
        return doc != nullptr;
    }

    /** Root value, or nullptr if the document failed to parse. */
    [[nodiscard]] yyjson_val *root() const;

private:
    yyjson_doc *doc{nullptr};
};

/**
 * Parses a JSON file.  The path goes through QFile so that non-ASCII paths work
 * on Windows.  On failure an invalid Doc is returned and, if given, error is set
 * to a human readable reason.
 */
Doc readFile(const QString &path, QString *error = nullptr);

/** Parses a JSON document held in memory. */
Doc readData(const QByteArray &data, QString *error = nullptr);

/* All accessors tolerate a null or wrongly typed obj and fall back to def. */

yyjson_val *objGet(yyjson_val *obj, const char *key);

QString objGetString(yyjson_val *obj, const char *key, const QString &def = QString());

int objGetInt(yyjson_val *obj, const char *key, int def = 0);

bool objGetBool(yyjson_val *obj, const char *key, bool def = false);

/** Reads an array of strings; non-string elements are skipped. */
QStringList objGetStringList(yyjson_val *obj, const char *key);

/** Reads an array of ints of a known length, e.g. a "size": [w, h] pair. */
bool objGetIntArray(yyjson_val *obj, const char *key, int *out, int count);

/** Owns a mutable document being built for writing. */
class Builder {
public:
    Builder();

    ~Builder();

    Builder(const Builder &) = delete;

    Builder &operator=(const Builder &) = delete;

    Builder(Builder &&) = delete;

    Builder &operator=(Builder &&) = delete;

    [[nodiscard]] yyjson_mut_val *newObject() const;

    [[nodiscard]] yyjson_mut_val *newArray() const;

    void setRoot(yyjson_mut_val *val) const;

    /* Keys and string values are always copied into the document, so callers
     * never have to worry about the lifetime of a QByteArray temporary. */

    void addString(yyjson_mut_val *obj, const char *key, const QString &value) const;

    void addInt(yyjson_mut_val *obj, const char *key, int value) const;

    void addBool(yyjson_mut_val *obj, const char *key, bool value) const;

    void addValue(yyjson_mut_val *obj, const char *key, yyjson_mut_val *value) const;

    void appendString(yyjson_mut_val *arr, const QString &value) const;

    void appendInt(yyjson_mut_val *arr, int value) const;

    static void appendValue(yyjson_mut_val *arr, yyjson_mut_val *value);

    /** Serialises pretty printed and writes atomically via QSaveFile. */
    bool writeFile(const QString &path, QString *error = nullptr) const;

private:
    yyjson_mut_doc *doc;
};

}
