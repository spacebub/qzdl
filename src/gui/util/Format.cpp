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

#include <array>
#include <format>
#include <utility>

#include "core/ports/Detect.h"
#include "core/system/Paths.h"
#include "gui/util/Format.h"

namespace {

const std::string &homePrefix() {
    static const std::string home = Format::fromPath(Paths::homeDirectory()) + "/";

    return home;
}

}

namespace Format {

std::string prettyPath(const std::string &path) {
    const std::string &home = homePrefix();

    return home.size() > 1 && path.starts_with(home) ? "~" + path.substr(home.size() - 1) : path;
}

std::string fitPath(const std::string &path, const int room) {
    std::string pretty = prettyPath(path);

    if (room <= 0 || std::cmp_less_equal(pretty.size(), room)) {
        return pretty;
    }

    for (size_t at = pretty.find('/'); at != std::string::npos; at = pretty.find('/', at + 1)) {
        // The ellipsis is three bytes but one column.
        if (std::string candidate = "…/" + pretty.substr(at + 1);
            std::cmp_less_equal(candidate.size() - 2, room - 1)) {
            return candidate;
        }
    }

    return pretty;
}

std::string directoryOf(const std::string &path) {
    return fromPath(std::filesystem::path(path).parent_path());
}

std::string fileName(const std::string &path) {
    return fromPath(std::filesystem::path(path).filename());
}

bool isFile(const std::string &path) {
    std::error_code code;

    return std::filesystem::is_regular_file(std::filesystem::path(path), code);
}

bool isDirectory(const std::string &path) {
    std::error_code code;

    return std::filesystem::is_directory(std::filesystem::path(path), code);
}

bool sameFile(const std::string &left, const std::string &right) {
    return Detect::same(std::filesystem::path(left), std::filesystem::path(right));
}

std::string upper(const std::string &value) {
    std::string out = value;

    for (char &letter : out) {
        if (letter >= 'a' && letter <= 'z') {
            letter = static_cast<char>(letter - 'a' + 'A');
        }
    }

    return out;
}

std::string bytes(const unsigned long long size) {
    constexpr std::array<const char *, 4> UNITS = {"B", "KB", "MB", "GB"};

    auto shown = static_cast<double>(size);
    size_t unit = 0;

    while (shown >= 1024.0 && unit + 1 < UNITS.size()) {
        shown /= 1024.0;
        ++unit;
    }

    return unit == 0 ? std::format("{:.0f} {}", shown, UNITS[unit])
                     : std::format("{:.1f} {}", shown, UNITS[unit]);
}

}
