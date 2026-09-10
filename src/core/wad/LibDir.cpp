/*
 * This file is part of qZDL
 * Copyright (C) 2019  Lcferrum
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
#include <fstream>
#include <sstream>
#include <utility>

#include "core/util/Text.h"
#include "core/wad/LibDir.h"

LibDir::LibDir(std::filesystem::path file) : _file(std::move(file)) {
}

std::filesystem::path LibDir::mapsDirectory() const {
    std::error_code code;

    for (const auto &entry : std::filesystem::directory_iterator(_file, code)) {
        if (entry.is_directory(code) && Text::iequals(entry.path().filename().string(), "maps")) {
            return entry.path();
        }
    }

    return {};
}

std::vector<std::string> LibDir::mapNames() {
    std::vector<std::string> names;
    std::error_code code;

    for (const auto &entry : std::filesystem::directory_iterator(_file, code)) {
        if (!entry.is_regular_file(code)) {
            continue;
        }

        if (const std::unique_ptr<MapFile> map = MapFile::open(entry.path())) {
            std::vector<std::string> found = map->mapNames();

            names.insert(names.end(), found.begin(), found.end());
        }
    }

    const std::filesystem::path maps = mapsDirectory();

    if (maps.empty()) {
        return names;
    }

    // The file name is the map name.
    for (const auto &entry : std::filesystem::directory_iterator(maps, code)) {
        if (!entry.is_regular_file(code)) {
            continue;
        }

        names.push_back(Text::upper(entry.path().stem().string().substr(0, 8)));
    }

    return names;
}

std::string LibDir::iwadinfoName() {
    std::error_code code;

    for (const auto &entry : std::filesystem::directory_iterator(_file, code)) {
        if (!entry.is_regular_file(code) || !Text::iequals(entry.path().stem().string(), "iwadinfo")) {
            continue;
        }

        std::ifstream const file(entry.path(), std::ios::binary);

        if (!file) {
            continue;
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();

        return nameFromIwadinfo(buffer.str());
    }

    return {};
}

namespace {

std::string readWhole(const std::filesystem::path &file) {
    std::ifstream const stream(file, std::ios::binary);

    if (!stream) {
        return {};
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();

    return buffer.str();
}

std::string fileNamed(const std::filesystem::path &directory, const std::string_view name) {
    std::error_code code;

    for (const auto &entry : std::filesystem::directory_iterator(directory, code)) {
        if (!entry.is_regular_file(code) || !Text::iequals(entry.path().stem().string(), name)) {
            continue;
        }

        if (std::string bytes = readWhole(entry.path()); !bytes.empty()) {
            return bytes;
        }
    }

    return {};
}

}

std::string LibDir::lump(const std::string_view name) {
    if (std::string bytes = fileNamed(_file, name); !bytes.empty()) {
        return bytes;
    }

    std::error_code code;

    for (const auto &entry : std::filesystem::directory_iterator(_file, code)) {
        if (!entry.is_directory(code)) {
            continue;
        }

        if (std::string bytes = fileNamed(entry.path(), name); !bytes.empty()) {
            return bytes;
        }
    }

    return {};
}

void LibDir::bestIn(const std::filesystem::path &directory, const std::string_view under,
                    const std::span<const std::string_view> names,
                    std::filesystem::path &best, size_t &rank) {
    std::error_code code;

    for (const auto &entry : std::filesystem::directory_iterator(directory, code)) {
        if (!entry.is_regular_file(code)) {
            continue;
        }

        const std::string name = entry.path().filename().string();

        if (const size_t at = rankOf(names, entry.path().stem().string());
            at < rank && drawable(name, under)) {
            best = entry.path();
            rank = at;
        }
    }
}

std::string LibDir::picture(const std::span<const std::string_view> names) {
    std::filesystem::path best;
    size_t rank = names.size();
    std::error_code code;

    bestIn(_file, {}, names, best, rank);

    for (const auto &entry : std::filesystem::directory_iterator(_file, code)) {
        if (entry.is_directory(code)) {
            bestIn(entry.path(), entry.path().filename().string(), names, best, rank);
        }
    }

    return rank < names.size() ? readWhole(best) : std::string();
}

std::vector<std::string> LibDir::lumpNames() {
    std::vector<std::string> names;
    std::error_code code;

    for (const auto &entry : std::filesystem::directory_iterator(_file, code)) {
        if (entry.is_regular_file(code)) {
            names.push_back(Text::upper(entry.path().stem().string()));
            continue;
        }

        if (!entry.is_directory(code)) {
            continue;
        }

        for (const auto &nested : std::filesystem::directory_iterator(entry.path(), code)) {
            if (nested.is_regular_file(code)) {
                names.push_back(Text::upper(nested.path().stem().string()));
            }
        }
    }

    return names;
}

bool LibDir::isGame() {
    return !lump("IWADINFO").empty();
}

bool LibDir::isMapXX() {
    const std::filesystem::path maps = mapsDirectory();

    if (maps.empty()) {
        return false;
    }

    std::error_code code;

    return std::ranges::any_of(
            std::filesystem::directory_iterator(maps, code),
            [](const std::filesystem::directory_entry &entry) {
                const std::string name = entry.path().filename().string();

                return Text::iequals(name, "map01.wad") || Text::iequals(name, "map01.map");
            });
}
