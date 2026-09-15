/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
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
#include <string_view>
#include <utility>
#include <vector>

#include <yyjson.h>

namespace Json {

// Lenient conversions: numbers and booleans a hand edited config wrote as strings are accepted.
std::string asString(const yyjson_val *val, const std::string &def = {});
int asInt(const yyjson_val *val, int def = 0);
bool asBool(const yyjson_val *val, bool def = false);
std::vector<std::string> asStringList(const yyjson_val *val);
bool asIntArray(const yyjson_val *val, int *out, int count);

template<class F>
void eachField(yyjson_val *obj, F &&visit) {
    if (obj == nullptr || !yyjson_is_obj(obj)) {
        return;
    }

    size_t idx = 0;
    size_t max = 0;
    yyjson_val *key = nullptr;
    yyjson_val *val = nullptr;

    yyjson_obj_foreach(obj, idx, max, key, val) {
        visit(std::string_view(yyjson_get_str(key), yyjson_get_len(key)), val);
    }
}

template<class F>
void eachItem(yyjson_val *arr, F &&visit) {
    if (arr == nullptr || !yyjson_is_arr(arr)) {
        return;
    }

    size_t idx = 0;
    size_t max = 0;
    yyjson_val *item = nullptr;

    yyjson_arr_foreach(arr, idx, max, item) {
        visit(item);
    }
}

yyjson_val *objGet(yyjson_val *obj, const char *key);
std::string objGetString(yyjson_val *obj, const char *key, const std::string &def = {});
int objGetInt(yyjson_val *obj, const char *key, int def = 0);

class Doc {
public:
    Doc() = default;

    explicit Doc(yyjson_doc *doc) : _doc(doc) {}

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

    // Backing text of an in-situ parse. The document's strings point into it.
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

    // Keys and values are referenced, not copied. They must outlive writeFile().
    void addString(yyjson_mut_val *obj, const char *key, std::string_view value) const;
    void addInt(yyjson_mut_val *obj, const char *key, int value) const;
    void addBool(yyjson_mut_val *obj, const char *key, bool value) const;
    void addValue(yyjson_mut_val *obj, const char *key, yyjson_mut_val *value) const;

    void appendString(yyjson_mut_val *arr, std::string_view value) const;
    void appendInt(yyjson_mut_val *arr, int value) const;
    static void appendValue(yyjson_mut_val *arr, yyjson_mut_val *value);

    bool writeFile(const std::filesystem::path &path, std::string *error = nullptr) const;

private:
    [[nodiscard]] yyjson_mut_val *keyOf(const char *key) const;

    yyjson_mut_doc *_doc;
};

}
