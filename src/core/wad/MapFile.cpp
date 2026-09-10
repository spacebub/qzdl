/*
 * This file is part of qZDL
 * Copyright (C) 2007-2012  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <map>

#include "core/util/Text.h"
#include "core/wad/LibDir.h"
#include "core/wad/LibPk3.h"
#include "core/wad/MapFile.h"
#include "core/wad/Wad.h"

namespace {

struct Stamp {
    std::uintmax_t size{0};
    std::filesystem::file_time_type when;

    friend bool operator==(const Stamp &, const Stamp &) = default;
};

Stamp stampOf(const std::filesystem::path &file) {
    std::error_code code;
    Stamp now;

    now.size = std::filesystem::file_size(file, code);
    now.when = std::filesystem::last_write_time(file, code);

    return now;
}

struct Known {
    Stamp stamp;
    MapFile::Maps maps;
};

}

const MapFile::Maps &MapFile::maps(const std::string &file) {
    static std::map<std::string, Known> seen;

    const Stamp now = stampOf(file);

    if (const auto found = seen.find(file); found != seen.end() && found->second.stamp == now) {
        return found->second.maps;
    }

    Known made;

    made.stamp = now;

    if (const std::unique_ptr<MapFile> opened = MapFile::open(file)) {
        made.maps.opened = true;
        made.maps.mapxx = opened->isMapXX();
        made.maps.names = opened->mapNames();
    }

    return seen.insert_or_assign(file, std::move(made)).first->second.maps;
}

namespace {

constexpr std::array PICTURE_EXTENSIONS = {
    ".lmp", ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".tga", ".pcx",
};

// Never map files.
constexpr std::array BANNED_EXTENSIONS = {
    ".lmp", ".txt", ".cfg", ".ini", ".deh", ".bex", ".zdl", ".zds", ".dsg", ".esg",
};

bool banned(const std::filesystem::path &file) {
    const std::string extension = Text::lower(file.extension().string());

    return std::ranges::any_of(BANNED_EXTENSIONS, [&extension](const char *candidate) {
        return extension == candidate;
    });
}

bool blank(const char letter) {
    return std::isspace(static_cast<unsigned char>(letter)) != 0;
}

bool wordAt(const std::string_view text, const size_t at, const std::string_view word) {
    if (at > 0 && (std::isalnum(static_cast<unsigned char>(text[at - 1])) != 0
                   || text[at - 1] == '_')) {
        return false;
    }

    return Text::iequals(text.substr(at, word.size()), word);
}

size_t past(const std::string_view text, size_t at) {
    while (at < text.size() && blank(text[at])) {
        ++at;
    }

    return at;
}

}

size_t MapFile::rankOf(const std::span<const std::string_view> names,
                       const std::string_view stem) {
    for (size_t at = 0; at < names.size(); ++at) {
        if (Text::iequals(names[at], stem)) {
            return at;
        }
    }

    return names.size();
}

bool MapFile::drawable(const std::string_view name, const std::string_view directory) {
    if (Text::iequals(directory, "graphics")) {
        return true;
    }

    const size_t dot = name.find_last_of('.');

    if (dot == std::string_view::npos) {
        return true;
    }

    const std::string extension = Text::lower(name.substr(dot));

    return std::ranges::any_of(PICTURE_EXTENSIONS, [&extension](const char *candidate) {
        return extension == candidate;
    });
}

std::string MapFile::nameFromIwadinfo(const std::string_view text) {
    constexpr std::string_view key = "name";

    for (size_t at = 0; at + key.size() <= text.size(); ++at) {
        if (!wordAt(text, at, key)) {
            continue;
        }

        const size_t equals = past(text, at + key.size());

        if (equals >= text.size() || text[equals] != '=') {
            continue;
        }

        const size_t opening = past(text, equals + 1);

        if (opening >= text.size() || text[opening] != '"') {
            continue;
        }

        const size_t closing = text.find('"', opening + 1);

        if (closing == std::string_view::npos) {
            return {};
        }

        return std::string(text.substr(opening + 1, closing - opening - 1));
    }

    return {};
}

std::unique_ptr<MapFile> MapFile::open(const std::filesystem::path &file) {
    std::error_code code;

    if (std::filesystem::is_directory(file, code)) {
        return std::make_unique<LibDir>(file);
    }

    if (file.extension().empty() || banned(file) || !std::filesystem::is_regular_file(file, code)) {
        return nullptr;
    }

    std::ifstream stream(file, std::ios::binary);

    if (!stream) {
        return nullptr;
    }

    std::array<char, 4> magic{};

    if (!stream.read(magic.data(), magic.size())) {
        return nullptr;
    }

    if (magic == std::array{'I', 'W', 'A', 'D'} || magic == std::array{'P', 'W', 'A', 'D'}) {
        return std::make_unique<Wad>(file);
    }

    if (magic == std::array{'P', 'K', '\x03', '\x04'}) {
        return std::make_unique<LibPk3>(file);
    }

    return nullptr;
}
