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
#include <array>
#include <cstdint>
#include <utility>

#include "ttk/system/Text.h"

#include "core/wad/LibPk3.h"
#include "core/wad/Wad.h"
#include "external/miniz/miniz.h"

using namespace ttk;

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

    return first == std::string_view::npos ? std::string_view()
                                           : text.substr(first, text.find_last_not_of(BLANKS) - first + 1);
}

// The name off a "map NAME ..." MAPINFO line.
std::string_view mapFromLine(const std::string_view line) {
    std::string_view rest = trimmed(line);

    if (rest.size() < 4 || !Text::iequals(rest.substr(0, 3), "map") || !BLANKS.contains(rest[3])) {
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

    if (!text.empty() && mz_zip_reader_extract_to_mem(&held->archive, index, text.data(), text.size(), 0) == 0) {
        return {};
    }

    return text;
}

// Bytes read or inflated across the nested WADs of one archive. The read runs on the UI thread,
// so it stays near a millisecond.
constexpr size_t NESTED_LIMIT = size_t{384} * 1024;

constexpr size_t LOCAL_HEADER_SIZE = 30;
constexpr std::uint32_t LOCAL_HEADER_SIGNATURE = 0x04034B50;

std::uint32_t little(const std::uint8_t *at, const size_t width) {
    std::uint32_t value = 0;

    for (size_t byte = 0; byte < width; ++byte) {
        value |= static_cast<std::uint32_t>(at[byte]) << (8 * byte);
    }

    return value;
}

bool readAt(const mz_zip_archive &archive, const std::uint64_t offset, void *into, const size_t size) {
    return archive.m_pRead(archive.m_pIO_opaque, offset, into, size) == size;
}

// A stored WAD needs no inflating: its header and lump table are read where they lie.
std::optional<std::string>
storedTable(const mz_zip_archive &archive, const mz_zip_archive_file_stat &stat, size_t &budget) {
    std::array<std::uint8_t, LOCAL_HEADER_SIZE> local{};

    if (stat.m_comp_size != stat.m_uncomp_size || !readAt(archive, stat.m_local_header_ofs, local.data(), local.size())
        || little(local.data(), 4) != LOCAL_HEADER_SIGNATURE) {
        return std::nullopt;
    }

    const std::uint64_t data =
            stat.m_local_header_ofs + LOCAL_HEADER_SIZE + little(local.data() + 26, 2) + little(local.data() + 28, 2);

    if (data > archive.m_archive_size || stat.m_uncomp_size > archive.m_archive_size - data) {
        return std::nullopt;
    }

    std::array<char, 12> header{};

    if (!readAt(archive, data, header.data(), header.size())) {
        return std::nullopt;
    }

    const std::optional<Wad::Table> table =
            Wad::tableOf({header.data(), header.size()}, static_cast<std::int64_t>(stat.m_uncomp_size));

    if (!table || std::cmp_greater(table->bytes, budget)) {
        return std::nullopt;
    }

    std::string bytes(static_cast<size_t>(table->bytes), '\0');

    if (!bytes.empty()
        && !readAt(archive, data + static_cast<std::uint64_t>(table->offset), bytes.data(), bytes.size())) {
        return std::nullopt;
    }

    budget -= bytes.size();

    return bytes;
}

// The raw lump entries of the WAD stored as this entry. Empty when it cannot be read within the budget.
std::optional<std::string> tableOfEntry(LibPk3::Zip *held, const mz_uint index, size_t &budget) {
    mz_zip_archive_file_stat stat;

    if (held == nullptr || mz_zip_reader_file_stat(&held->archive, index, &stat) == 0 || stat.m_is_supported == 0) {
        return std::nullopt;
    }

    if (stat.m_method == 0) {
        return storedTable(held->archive, stat, budget);
    }

    if (stat.m_uncomp_size > budget) {
        return std::nullopt;
    }

    const std::string whole = extract(held, index);
    const std::optional<Wad::Table> table = Wad::tableOf(whole, static_cast<std::int64_t>(whole.size()));

    if (whole.size() != stat.m_uncomp_size || !table) {
        return std::nullopt;
    }

    budget -= whole.size();

    return whole.substr(static_cast<size_t>(table->offset), static_cast<size_t>(table->bytes));
}

template <typename Visitor>
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

        // A stat copies a kilobyte per file. Only the name is needed.
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

const LibPk3::Listing &LibPk3::listing() {
    if (_listing) {
        return *_listing;
    }

    Listing found;
    size_t budget = NESTED_LIMIT;
    const std::string folder = _file.stem().string();

    // A plain MAPINFO only counts when there is no ZMAPINFO.
    bool zmapinfo = false;

    walk(zip(), [&](const mz_uint index, const Entry &entry) {
        if (Text::iequals(entry.directory, "maps")) {
            if (Text::iends_with(entry.name, ".wad") || Text::iends_with(entry.name, ".map")) {
                found.maps.push_back(lumpName(entry.stem));
            }

            if (Text::iequals(entry.name, "map01.wad") || Text::iequals(entry.name, "map01.map")) {
                found.mapxx = true;
            }
        }

        if (entry.directory.empty()) {
            if (!zmapinfo && Text::iequals(entry.stem, "zmapinfo")) {
                zmapinfo = true;
                found.hasInfo = true;
                found.infoIndex = index;
            } else if (!found.hasInfo && Text::iequals(entry.stem, "mapinfo")) {
                found.hasInfo = true;
                found.infoIndex = index;
            }
        }

        if ((!entry.directory.empty() && !Text::iequals(entry.directory, folder))
            || !Text::iends_with(entry.name, ".wad")) {
            return true;
        }

        const std::optional<std::string> table = tableOfEntry(zip(), index, budget);

        if (!table) {
            found.whole = false;

            return true;
        }

        Maps scanned = Wad::scan(*table);

        found.nested.insert(found.nested.end(),
                            std::make_move_iterator(scanned.names.begin()),
                            std::make_move_iterator(scanned.names.end()));
        found.mapxx = found.mapxx || scanned.mapxx;

        return true;
    });

    return _listing.emplace(std::move(found));
}

bool LibPk3::readable() {
    return zip() != nullptr && listing().whole;
}

std::vector<std::string> LibPk3::mapNames() {
    const Listing &found = listing();
    std::vector<std::string> names = found.maps;

    names.insert(names.end(), found.nested.begin(), found.nested.end());

    if (!found.hasInfo) {
        return names;
    }

    const std::string text = extract(zip(), found.infoIndex);

    for (size_t at = 0; at < text.size();) {
        const size_t end = text.find('\n', at);
        const std::string_view line = std::string_view(text).substr(at, end == std::string::npos ? end : end - at);

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
        if (const size_t at = rankOf(names, entry.stem); at < rank && drawable(entry.name, entry.directory)) {
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
    return listing().mapxx;
}
