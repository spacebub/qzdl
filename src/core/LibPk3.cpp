/*
 * This file is part of qZDL
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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

#include <regex>
#include <utility>

#include "core/LibPk3.h"
#include "core/Text.h"
#include "miniz.h"

namespace {

/**
 * A zip entry always names its directory with forward slashes, whatever made
 * it, so an entry is split here rather than through the filesystem's idea of
 * what a separator is.
 */
struct Entry {
    std::string_view directory;
    std::string_view name;
    std::string_view stem;
};

Entry split(const std::string_view path) {
    Entry entry;
    const size_t slash = path.find_last_of('/');

    entry.directory = slash == std::string_view::npos ? std::string_view() : path.substr(0, slash);
    entry.name = slash == std::string_view::npos ? path : path.substr(slash + 1);

    const size_t dot = entry.name.find_last_of('.');
    entry.stem = dot == std::string_view::npos ? entry.name : entry.name.substr(0, dot);

    return entry;
}

/** Everything a zip entry holds, or nothing when it could not be unpacked. */
std::string extract(mz_zip_archive &archive, const mz_uint index) {
    size_t length = 0;
    void *buffer = mz_zip_reader_extract_to_heap(&archive, index, &length, 0);

    if (buffer == nullptr) {
        return {};
    }

    std::string text(static_cast<const char *>(buffer), length);

    mz_free(buffer);

    return text;
}

/** Opens the archive, hands each entry's name to the visitor, and closes it. */
template<typename Visitor>
void walk(const std::filesystem::path &file, Visitor &&visitor) {
    mz_zip_archive archive = {};

    if (mz_zip_reader_init_file(&archive, file.string().c_str(), 0) == 0) {
        return;
    }

    const mz_uint count = mz_zip_reader_get_num_files(&archive);

    for (mz_uint index = 0; index < count; index++) {
        mz_zip_archive_file_stat stat;

        if (mz_zip_reader_is_file_a_directory(&archive, index) != 0
            || mz_zip_reader_file_stat(&archive, index, &stat) == 0) {
            continue;
        }

        if (!visitor(archive, index, split(stat.m_filename))) {
            break;
        }
    }

    mz_zip_reader_end(&archive);
}

}

LibPk3::LibPk3(std::filesystem::path file) : _file(std::move(file)) {
}

std::vector<std::string> LibPk3::mapNames() {
    std::vector<std::string> names;

    /*
    Two ways a PK3 says what maps it holds: files under "maps", and a MAPINFO
    that names them. Both are read, and the plain MAPINFO only counts while no
    ZMAPINFO has turned up, since a port that understands the newer one ignores
    the older.
    */
    bool mapinfo = false;
    bool zmapinfo = false;
    mz_uint mapinfoIndex = 0;

    walk(_file, [&](mz_zip_archive &, const mz_uint index, const Entry &entry) {
        if (Text::iequals(entry.directory, "maps")) {
            names.push_back(Text::upper(entry.stem.substr(0, std::min<size_t>(entry.stem.size(), 8))));
        } else if (entry.directory.empty()) {
            if (!zmapinfo && Text::iequals(entry.stem, "zmapinfo")) {
                zmapinfo = true;
                mapinfoIndex = index;
            } else if (!mapinfo && !zmapinfo && Text::iequals(entry.stem, "mapinfo")) {
                mapinfo = true;
                mapinfoIndex = index;
            }
        }

        return true;
    });

    if (!mapinfo && !zmapinfo) {
        return names;
    }

    mz_zip_archive archive = {};

    if (mz_zip_reader_init_file(&archive, _file.string().c_str(), 0) == 0) {
        return names;
    }

    const std::string text = extract(archive, mapinfoIndex);

    mz_zip_reader_end(&archive);

    static const std::regex line(R"(^\s*map\s+(\S+)(\s+.*)?$)", std::regex::icase);

    for (const std::string &raw : Text::split(text, '\n')) {
        const std::string trimmed = Text::trim(raw);
        std::smatch match;

        if (std::regex_match(trimmed, match, line)) {
            const std::string name = match[1].str();

            names.push_back(Text::upper(name.substr(0, std::min<size_t>(name.size(), 8))));
        }
    }

    return names;
}

std::string LibPk3::iwadinfoName() {
    std::string name;

    walk(_file, [&](mz_zip_archive &archive, const mz_uint index, const Entry &entry) {
        if (!entry.directory.empty() || !Text::iequals(entry.stem, "iwadinfo")) {
            return true;
        }

        name = nameFromIwadinfo(extract(archive, index));

        return false;
    });

    return name;
}

bool LibPk3::isMapXX() {
    bool mapxx = false;

    walk(_file, [&](mz_zip_archive &, mz_uint, const Entry &entry) {
        if (Text::iequals(entry.directory, "maps")
            && (Text::iequals(entry.name, "map01.wad") || Text::iequals(entry.name, "map01.map"))) {
            mapxx = true;

            return false;
        }

        return true;
    });

    return mapxx;
}
