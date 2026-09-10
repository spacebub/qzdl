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

#include <slint.h>

namespace Convert {

// Slint panics on invalid UTF-8; stray bytes become U+FFFD.
[[nodiscard]] slint::SharedString text(std::string_view value);

[[nodiscard]] inline std::string plain(const slint::SharedString &value) {
    return {value.data(), value.size()};
}

[[nodiscard]] inline std::filesystem::path toPath(const slint::SharedString &value) {
    return {plain(value)};
}

// The interface expects forward slashes.
[[nodiscard]] inline slint::SharedString fromPath(const std::filesystem::path &path) {
    return text(path.generic_string());
}

template <typename Strings>
[[nodiscard]] std::shared_ptr<slint::VectorModel<slint::SharedString>>
strings(const Strings &values) {
    std::vector<slint::SharedString> out;

    out.reserve(std::size(values));

    for (const std::string_view each : values) {
        out.push_back(text(each));
    }

    return std::make_shared<slint::VectorModel<slint::SharedString>>(std::move(out));
}

}
