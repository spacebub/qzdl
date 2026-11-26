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

#include <type_traits>

#include "ttk/system/Json.h"

// A key at its default is left out of zdl.json. A changed default needs a step in Versions.cpp.
class NonDefault {
public:
    explicit NonDefault(const ttk::Json::Builder &out) : _out(out) {}

    // The builder keeps a view of text, so the value must outlive the write.
    template <typename Value>
    void field(yyjson_mut_val *obj, const char *key, const Value &value, const Value &fallback) const {
        static_assert(!std::is_pointer_v<Value>, "A pointer would compare by address.");

        if (value == fallback) {
            return;
        }

        if constexpr (std::is_same_v<Value, bool>) {
            _out.add_bool(obj, key, value);
        } else if constexpr (std::is_integral_v<Value>) {
            _out.add_int(obj, key, value);
        } else {
            _out.add_string(obj, key, value);
        }
    }

    void filled(yyjson_mut_val *parent, const char *key, yyjson_mut_val *value) const {
        if (yyjson_mut_get_len(value) != 0) {
            _out.add_value(parent, key, value);
        }
    }

private:
    const ttk::Json::Builder &_out;
};
