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

#include <cstring>
#include <fstream>
#include <utility>

#include "ttk/system/Text.h"

#include "core/wad/Wad.h"

using namespace ttk;

namespace {

// More is a corrupt file.
constexpr std::int32_t LUMP_LIMIT = 1024 * 1024;

}

Wad::Wad(std::filesystem::path file) : _file(std::move(file)) {
}

std::string_view Wad::Lump::nameView() const {
    const size_t stop = std::string_view(name, sizeof(name)).find('\0');

    return {name, stop == std::string_view::npos ? sizeof(name) : stop};
}

bool Wad::open() {
    if (_opened) {
        return _stream.is_open();
    }

    _opened = true;

    std::error_code code;
    const std::uintmax_t size = std::filesystem::file_size(_file, code);

    if (code) {
        return false;
    }

    _size = static_cast<std::int64_t>(size);
    _stream.open(_file, std::ios::binary);

    if (!_stream.read(reinterpret_cast<char *>(&_header), sizeof(_header))) {
        _stream.close();

        return false;
    }

    return true;
}

std::optional<Wad::Table> Wad::tableOf(const std::string_view header, const std::int64_t size) {
    Header read{};

    if (header.size() < sizeof(read)) {
        return std::nullopt;
    }

    std::memcpy(&read, header.data(), sizeof(read));

    const std::string_view kind(read.type, sizeof(read.type));

    if ((kind != "IWAD" && kind != "PWAD") || read.lumps < 0 || read.lumps > LUMP_LIMIT) {
        return std::nullopt;
    }

    const std::int64_t bytes = static_cast<std::int64_t>(read.lumps) * static_cast<std::int64_t>(sizeof(Lump));

    if (bytes > 0 && (read.directory < 0 || read.directory + bytes > size)) {
        return std::nullopt;
    }

    return Table{.offset = read.directory, .bytes = bytes};
}

bool Wad::holds(const std::int64_t offset, const std::int64_t length) const {
    return offset >= 0 && length > 0 && offset + length <= _size;
}

const std::vector<Wad::Lump> &Wad::directory() {
    if (_listed) {
        return _lumps;
    }

    _listed = true;

    if (!open()) {
        return _lumps;
    }

    const std::optional<Table> table = tableOf({reinterpret_cast<const char *>(&_header), sizeof(_header)}, _size);

    if (!table) {
        return _lumps;
    }

    std::vector<Lump> lumps(static_cast<size_t>(table->bytes) / sizeof(Lump));

    if (!lumps.empty()) {
        _stream.clear();
        _stream.seekg(table->offset);

        if (!_stream.read(reinterpret_cast<char *>(lumps.data()), static_cast<std::streamsize>(table->bytes))) {
            return _lumps;
        }
    }

    _lumps = std::move(lumps);
    _sound = true;

    return _lumps;
}

MapFile::Maps Wad::scan(const std::string_view table) {
    Maps found;

    // A map is the lump named before its THINGS, or before its TEXTMAP in UDMF.
    Lump previous{};

    for (size_t at = 0; at + sizeof(Lump) <= table.size(); at += sizeof(Lump)) {
        Lump lump{};

        std::memcpy(&lump, table.data() + at, sizeof(lump));

        if (at > 0 && (lump.nameView() == "THINGS" || lump.nameView() == "TEXTMAP")) {
            found.names.emplace_back(previous.nameView());
        }

        found.mapxx = found.mapxx || lump.nameView() == "MAP01";
        previous = lump;
    }

    return found;
}

std::vector<std::string> Wad::mapNames() {
    return scan(rawDirectory()).names;
}

std::string Wad::lump(const std::string_view name) {
    for (const Lump &lump : directory()) {
        if (!Text::iequals(lump.nameView(), name) || !holds(lump.offset, lump.length)) {
            continue;
        }

        std::string bytes(static_cast<size_t>(lump.length), '\0');

        _stream.clear();
        _stream.seekg(lump.offset);

        return _stream.read(bytes.data(), lump.length) ? bytes : std::string();
    }

    return {};
}

std::string Wad::picture(const std::span<const std::string_view> names) {
    const Lump *best = nullptr;
    size_t rank = names.size();

    // The earliest name in the list wins.
    for (const Lump &lump : directory()) {
        const size_t at = rankOf(names, lump.nameView());

        if (at < rank && holds(lump.offset, lump.length)) {
            best = &lump;
            rank = at;
        }
    }

    if (best == nullptr) {
        return {};
    }

    std::string bytes(static_cast<size_t>(best->length), '\0');

    _stream.clear();
    _stream.seekg(best->offset);

    return _stream.read(bytes.data(), best->length) ? bytes : std::string();
}

std::vector<std::string> Wad::lumpNames() {
    std::vector<std::string> names;

    for (const Lump &lump : directory()) {
        names.push_back(Text::upper(lump.nameView()));
    }

    return names;
}

bool Wad::isGame() {
    return open() && std::string_view(_header.type, sizeof(_header.type)) == "IWAD";
}

std::string Wad::iwadinfoName() {
    return nameFromIwadinfo(lump("IWADINFO"));
}

bool Wad::isMapXX() {
    return scan(rawDirectory()).mapxx;
}

bool Wad::readable() {
    directory();

    return _sound;
}
