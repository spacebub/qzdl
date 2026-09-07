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
#include <string_view>

#include <slint.h>

// The interface writes every path with forward slashes; std::filesystem hands
// back the native form, which prettyPath and PathLabel cannot read.
namespace Convert {

// Slint panics on anything but UTF-8, so a stray byte becomes U+FFFD here.
[[nodiscard]] slint::SharedString text(std::string_view value);

[[nodiscard]] inline std::string plain(const slint::SharedString &value) {
    return {value.data(), value.size()};
}

[[nodiscard]] inline std::filesystem::path toPath(const slint::SharedString &value) {
    return {plain(value)};
}

[[nodiscard]] inline slint::SharedString fromPath(const std::filesystem::path &path) {
    return text(path.generic_string());
}

// Taken from anything holding strings, so a static list need not become a vector.
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
