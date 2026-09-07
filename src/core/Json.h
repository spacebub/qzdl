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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>
#include <yyjson.h>

namespace Json {

yyjson_val *objGet(yyjson_val *obj, const char *key);
std::string objGetString(yyjson_val *obj, const char *key, const std::string &def = {});
int objGetInt(yyjson_val *obj, const char *key, int def = 0);
bool objGetBool(yyjson_val *obj, const char *key, bool def = false);
std::vector<std::string> objGetStringList(yyjson_val *obj, const char *key);
bool objGetIntArray(yyjson_val *obj, const char *key, int *out, int count);

class Doc {
public:
    Doc() = default;

    explicit Doc(yyjson_doc *doc) : _doc(doc) {}

    // Parsed in place: the strings in it point into `text`, which is why the
    // two go together.
    Doc(yyjson_doc *doc, std::string text) : _doc(doc), _text(std::move(text)) {}

    ~Doc();

    Doc(const Doc &) = delete;

    Doc &operator=(const Doc &) = delete;

    Doc(Doc &&other) noexcept;

    Doc &operator=(Doc &&other) noexcept;

    [[nodiscard]] bool valid() const {
        return _doc != nullptr;
    }

    [[nodiscard]] yyjson_val *root() const;

private:
    yyjson_doc *_doc{nullptr};

    // What an in-place parse read out of. The document points into it rather
    // than holding text of its own, so it lives exactly as long as the document.
    std::string _text;
};

Doc readFile(const std::filesystem::path &path, std::string *error = nullptr);
Doc readData(const std::string &data, std::string *error = nullptr);

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

    void addString(yyjson_mut_val *obj, const char *key, const std::string &value) const;
    void addInt(yyjson_mut_val *obj, const char *key, int value) const;
    void addBool(yyjson_mut_val *obj, const char *key, bool value) const;
    void addValue(yyjson_mut_val *obj, const char *key, yyjson_mut_val *value) const;

    void appendString(yyjson_mut_val *arr, const std::string &value) const;
    void appendInt(yyjson_mut_val *arr, int value) const;
    static void appendValue(yyjson_mut_val *arr, yyjson_mut_val *value);

    bool writeFile(const std::filesystem::path &path, std::string *error = nullptr) const;

private:
    yyjson_mut_doc *_doc;
};

}
