/*
 * This file is part of qZDL
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
#include <utility>

#include "core/util/Text.h"
#include "core/wad/LibPk3.h"
#include "external/miniz/miniz.h"

struct LibPk3::Zip {
    mz_zip_archive archive{};
    bool opened{false};

    Zip() = default;

    ~Zip() {
        if (opened) {
            mz_zip_reader_end(&archive);
        }
    }

    Zip(const Zip &) = delete;
    Zip &operator=(const Zip &) = delete;
    Zip(Zip &&) = delete;
    Zip &operator=(Zip &&) = delete;
};

namespace {

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

constexpr std::string_view BLANKS = " \t\r\n\f\v";

std::string_view trimmed(const std::string_view text) {
    const size_t first = text.find_first_not_of(BLANKS);

    return first == std::string_view::npos
        ? std::string_view()
        : text.substr(first, text.find_last_not_of(BLANKS) - first + 1);
}

// The name off a "map NAME ..." MAPINFO line.
std::string_view mapFromLine(const std::string_view line) {
    std::string_view rest = trimmed(line);

    if (rest.size() < 4 || !Text::iequals(rest.substr(0, 3), "map")
        || !BLANKS.contains(rest[3])) {
        return {};
    }

    rest.remove_prefix(4);

    const size_t first = rest.find_first_not_of(BLANKS);

    if (first == std::string_view::npos) {
        return {};
    }

    rest.remove_prefix(first);

    return rest.substr(0, std::min(rest.find_first_of(BLANKS), rest.size()));
}

std::string lumpName(const std::string_view name) {
    return Text::upper(name.substr(0, std::min<size_t>(name.size(), 8)));
}

std::string extract(LibPk3::Zip *held, const mz_uint index) {
    mz_zip_archive_file_stat stat;

    if (held == nullptr || mz_zip_reader_file_stat(&held->archive, index, &stat) == 0) {
        return {};
    }

    std::string text(static_cast<size_t>(stat.m_uncomp_size), '\0');

    if (!text.empty()
        && mz_zip_reader_extract_to_mem(&held->archive, index, text.data(), text.size(), 0) == 0) {
        return {};
    }

    return text;
}

template<typename Visitor>
void walk(LibPk3::Zip *held, Visitor &&visitor) {
    if (held == nullptr) {
        return;
    }

    mz_zip_archive &archive = held->archive;
    const mz_uint count = mz_zip_reader_get_num_files(&archive);

    for (mz_uint index = 0; index < count; index++) {
        if (mz_zip_reader_is_file_a_directory(&archive, index) != 0) {
            continue;
        }

        // A stat copies a kilobyte per file; only the name is needed.
        char named[MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE];
        const mz_uint length = mz_zip_reader_get_filename(&archive, index, named, sizeof(named));

        if (length <= 1) {
            continue;
        }

        if (!visitor(index, split(std::string_view(named, length - 1)))) {
            break;
        }
    }
}

}

LibPk3::LibPk3(std::filesystem::path file) : _file(std::move(file)) {
}

LibPk3::~LibPk3() = default;

LibPk3::Zip *LibPk3::zip() {
    if (!_zip) {
        _zip = std::make_unique<Zip>();
        _zip->opened = mz_zip_reader_init_file(&_zip->archive, _file.string().c_str(), 0) != 0;
    }

    return _zip->opened ? _zip.get() : nullptr;
}

std::vector<std::string> LibPk3::mapNames() {
    std::vector<std::string> names;

    // A plain MAPINFO only counts when there is no ZMAPINFO.
    bool mapinfo = false;
    bool zmapinfo = false;
    mz_uint mapinfoIndex = 0;

    walk(zip(), [&](const mz_uint index, const Entry &entry) {
        if (Text::iequals(entry.directory, "maps")) {
            names.push_back(lumpName(entry.stem));
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

    const std::string text = extract(zip(), mapinfoIndex);

    for (size_t at = 0; at < text.size();) {
        const size_t end = text.find('\n', at);
        const std::string_view line =
            std::string_view(text).substr(at, end == std::string::npos ? end : end - at);

        if (const std::string_view named = mapFromLine(line); !named.empty()) {
            names.push_back(lumpName(named));
        }

        if (end == std::string::npos) {
            break;
        }

        at = end + 1;
    }

    return names;
}

std::string LibPk3::iwadinfoName() {
    std::string name;

    walk(zip(), [&](const mz_uint index, const Entry &entry) {
        if (!entry.directory.empty() || !Text::iequals(entry.stem, "iwadinfo")) {
            return true;
        }

        name = nameFromIwadinfo(extract(zip(), index));

        return false;
    });

    return name;
}

std::string LibPk3::lump(const std::string_view name) {
    std::string bytes;

    walk(zip(), [&](const mz_uint index, const Entry &entry) {
        if (!Text::iequals(entry.stem, name)) {
            return true;
        }

        bytes = extract(zip(), index);

        return false;
    });

    return bytes;
}

std::string LibPk3::picture(const std::span<const std::string_view> names) {
    mz_uint best = 0;
    size_t rank = names.size();

    // drawable() keeps a music/title.ogg from matching.
    walk(zip(), [&](const mz_uint index, const Entry &entry) {
        if (const size_t at = rankOf(names, entry.stem);
            at < rank && drawable(entry.name, entry.directory)) {
            best = index;
            rank = at;
        }

        return rank > 0;
    });

    return rank < names.size() ? extract(zip(), best) : std::string();
}

std::vector<std::string> LibPk3::lumpNames() {
    std::vector<std::string> names;

    walk(zip(), [&](mz_uint, const Entry &entry) {
        if (!entry.stem.empty()) {
            names.push_back(Text::upper(entry.stem));
        }

        return true;
    });

    return names;
}

bool LibPk3::isGame() {
    return !lump("IWADINFO").empty();
}

bool LibPk3::isMapXX() {
    bool mapxx = false;

    walk(zip(), [&](mz_uint, const Entry &entry) {
        if (Text::iequals(entry.directory, "maps")
            && (Text::iequals(entry.name, "map01.wad") || Text::iequals(entry.name, "map01.map"))) {
            mapxx = true;

            return false;
        }

        return true;
    });

    return mapxx;
}
