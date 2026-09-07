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

#include <cstdlib>
#include <fstream>
#include <utility>

#include "core/Json.h"
#include "core/Text.h"

namespace Json {

Doc::~Doc() {
    if (_doc != nullptr) {
        yyjson_doc_free(_doc);
    }
}

Doc::Doc(Doc &&other) noexcept: _doc(other._doc) {
    other._doc = nullptr;
}

Doc &Doc::operator=(Doc &&other) noexcept {
    if (this != &other) {
        if (_doc != nullptr) {
            yyjson_doc_free(_doc);
        }

        _doc = other._doc;
        other._doc = nullptr;
    }

    return *this;
}

yyjson_val *Doc::root() const {
    return _doc != nullptr ? yyjson_doc_get_root(_doc) : nullptr;
}

namespace {

Doc parse(char *data, const size_t size, const yyjson_read_flag flags, std::string *error) {
    yyjson_read_err err{};
    yyjson_doc *doc = yyjson_read_opts(data, size, flags, nullptr, &err);

    if (doc == nullptr) {
        if (error != nullptr) {
            *error = std::string(err.msg != nullptr ? err.msg : "unknown parse error")
                + " (at offset " + std::to_string(err.pos) + ")";
        }

        return {};
    }

    return Doc(doc);
}

}

Doc readData(const std::string &data, std::string *error) {
    return parse(const_cast<char *>(data.data()), data.size(), 0, error);
}

// Read once, into the buffer the parser then works in. The padding at the end is
// what lets it take the text apart where it lies rather than copying again.
Doc readFile(const std::filesystem::path &path, std::string *error) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file) {
        if (error != nullptr) {
            *error = "cannot open " + path.string();
        }

        return {};
    }

    const std::streamoff size = file.tellg();

    if (size < 0) {
        if (error != nullptr) {
            *error = "cannot read " + path.string();
        }

        return {};
    }

    std::string data(static_cast<size_t>(size) + YYJSON_PADDING_SIZE, '\0');

    file.seekg(0);

    if (size > 0 && !file.read(data.data(), size)) {
        if (error != nullptr) {
            *error = "cannot read " + path.string();
        }

        return {};
    }

    return parse(data.data(), static_cast<size_t>(size), YYJSON_READ_INSITU, error);
}

yyjson_val *objGet(yyjson_val *obj, const char *key) {
    if (obj == nullptr || !yyjson_is_obj(obj)) {
        return nullptr;
    }

    return yyjson_obj_get(obj, key);
}

std::string objGetString(yyjson_val *obj, const char *key, const std::string &def) {
    yyjson_val *val = objGet(obj, key);

    if (val == nullptr || !yyjson_is_str(val)) {
        return def;
    }

    return {yyjson_get_str(val), yyjson_get_len(val)};
}

int objGetInt(yyjson_val *obj, const char *key, const int def) {
    yyjson_val *val = objGet(obj, key);

    if (val == nullptr) {
        return def;
    }

    if (yyjson_is_int(val)) {
        return yyjson_get_int(val);
    }

    if (yyjson_is_real(val)) {
        return static_cast<int>(yyjson_get_real(val));
    }

    // Tolerate numbers that a hand edited config wrote as strings.
    if (yyjson_is_str(val)) {
        return Text::toInt(yyjson_get_str(val), def);
    }

    return def;
}

bool objGetBool(yyjson_val *obj, const char *key, const bool def) {
    yyjson_val *val = objGet(obj, key);

    if (val == nullptr) {
        return def;
    }

    if (yyjson_is_bool(val)) {
        return yyjson_get_bool(val);
    }

    // Legacy configs stored flags as "1"/"0"; accept those too.
    if (yyjson_is_int(val)) {
        return yyjson_get_int(val) != 0;
    }

    if (yyjson_is_str(val)) {
        const std::string str = yyjson_get_str(val);

        return str == "1" || Text::iequals(str, "true");
    }

    return def;
}

std::vector<std::string> objGetStringList(yyjson_val *obj, const char *key) {
    std::vector<std::string> out;
    yyjson_val *arr = objGet(obj, key);

    if (arr == nullptr || !yyjson_is_arr(arr)) {
        return out;
    }

    size_t idx = 0;
    size_t max = 0;
    yyjson_val *item = nullptr;

    yyjson_arr_foreach(arr, idx, max, item) {
        if (yyjson_is_str(item)) {
            out.emplace_back(yyjson_get_str(item), yyjson_get_len(item));
        }
    }

    return out;
}

bool objGetIntArray(yyjson_val *obj, const char *key, int *out, const int count) {
    yyjson_val *arr = objGet(obj, key);

    if (arr == nullptr || !yyjson_is_arr(arr) || std::cmp_less(yyjson_arr_size(arr), count)) {
        return false;
    }

    for (int i = 0; i < count; i++) {
        yyjson_val *item = yyjson_arr_get(arr, static_cast<size_t>(i));

        if (item == nullptr || !yyjson_is_num(item)) {
            return false;
        }

        out[i] = static_cast<int>(yyjson_get_num(item));
    }

    return true;
}

Builder::Builder() : _doc(yyjson_mut_doc_new(nullptr)) {
}

Builder::~Builder() {
    if (_doc != nullptr) {
        yyjson_mut_doc_free(_doc);
    }
}

yyjson_mut_val *Builder::newObject() const {
    return yyjson_mut_obj(_doc);
}

yyjson_mut_val *Builder::newArray() const {
    return yyjson_mut_arr(_doc);
}

void Builder::setRoot(yyjson_mut_val *val) const {
    yyjson_mut_doc_set_root(_doc, val);
}

void Builder::addString(yyjson_mut_val *obj, const char *key, const std::string &value) const {
    if (obj == nullptr) {
        return;
    }

    yyjson_mut_obj_add(obj, yyjson_mut_strcpy(_doc, key),
                       yyjson_mut_strncpy(_doc, value.data(), value.size()));
}

void Builder::addInt(yyjson_mut_val *obj, const char *key, const int value) const {
    if (obj == nullptr) {
        return;
    }

    yyjson_mut_obj_add(obj, yyjson_mut_strcpy(_doc, key), yyjson_mut_int(_doc, value));
}

void Builder::addBool(yyjson_mut_val *obj, const char *key, const bool value) const {
    if (obj == nullptr) {
        return;
    }

    yyjson_mut_obj_add(obj, yyjson_mut_strcpy(_doc, key), yyjson_mut_bool(_doc, value));
}

void Builder::addValue(yyjson_mut_val *obj, const char *key, yyjson_mut_val *value) const {
    if (obj == nullptr || value == nullptr) {
        return;
    }

    yyjson_mut_obj_add(obj, yyjson_mut_strcpy(_doc, key), value);
}

void Builder::appendString(yyjson_mut_val *arr, const std::string &value) const {
    if (arr == nullptr) {
        return;
    }

    yyjson_mut_arr_append(arr, yyjson_mut_strncpy(_doc, value.data(), value.size()));
}

void Builder::appendInt(yyjson_mut_val *arr, const int value) const {
    if (arr == nullptr) {
        return;
    }

    yyjson_mut_arr_append(arr, yyjson_mut_int(_doc, value));
}

void Builder::appendValue(yyjson_mut_val *arr, yyjson_mut_val *value) {
    if (arr == nullptr || value == nullptr) {
        return;
    }

    yyjson_mut_arr_append(arr, value);
}

bool Builder::writeFile(const std::filesystem::path &path, std::string *error) const {
    yyjson_write_err werr{};
    size_t len = 0;
    char *json = yyjson_mut_write_opts(_doc, YYJSON_WRITE_PRETTY_TWO_SPACES, nullptr, &len, &werr);

    if (json == nullptr) {
        if (error != nullptr) {
            *error = werr.msg != nullptr ? werr.msg : "unknown serialisation error";
        }

        return false;
    }

    // The config directory may not exist yet on a first run.
    std::error_code code;
    const std::filesystem::path directory = path.parent_path();

    if (!directory.empty() && !std::filesystem::is_directory(directory, code)) {
        std::filesystem::create_directories(directory, code);

        if (code) {
            if (error != nullptr) {
                *error = "cannot create directory " + directory.string() + ": " + code.message();
            }

            free(json);

            return false;
        }
    }

    /*
    Written beside the target rather than over it, so a config that already
    reads is only replaced once the new one is whole. The two have to sit on
    the same filesystem for the rename to be the atomic one, which a sibling
    is by definition.
    */
    std::filesystem::path temporary = path;
    temporary += ".new";

    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);

        if (!file) {
            if (error != nullptr) {
                *error = "cannot open " + temporary.string() + " for writing";
            }

            free(json);

            return false;
        }

        file.write(json, static_cast<std::streamsize>(len));
        file.put('\n');
        file.close();

        if (!file) {
            if (error != nullptr) {
                *error = "could not write " + temporary.string();
            }

            free(json);
            std::filesystem::remove(temporary, code);

            return false;
        }
    }

    free(json);

    std::filesystem::rename(temporary, path, code);

    if (code) {
        if (error != nullptr) {
            *error = "could not replace " + path.string() + ": " + code.message();
        }

        std::filesystem::remove(temporary, code);

        return false;
    }

    return true;
}

}
