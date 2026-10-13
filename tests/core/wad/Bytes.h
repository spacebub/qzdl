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

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <blend2d/blend2d.h>

// Game files byte by byte, for the cases the corpus does not cover: odd names,
// broken headers, zips in other methods.
namespace tests::Bytes {

inline void put16(std::string &out, const unsigned value) {
    out.push_back(static_cast<char>(value & 0xFFU));
    out.push_back(static_cast<char>((value >> 8U) & 0xFFU));
}

inline void put32(std::string &out, const std::uint32_t value) {
    put16(out, value & 0xFFFFU);
    put16(out, (value >> 16U) & 0xFFFFU);
}

inline void write(const std::filesystem::path &path, const std::string_view bytes) {
    std::filesystem::create_directories(path.parent_path());

    std::ofstream out(path, std::ios::binary | std::ios::trunc);

    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

struct Lump {
    std::string name;
    std::string bytes;
};

struct Placed {
    std::int32_t offset;
    std::int32_t length;
    std::string name;
};

inline std::string header(const std::string_view kind, const std::int32_t lumps, const std::int32_t directory) {
    std::string out(kind);

    put32(out, static_cast<std::uint32_t>(lumps));
    put32(out, static_cast<std::uint32_t>(directory));

    return out;
}

inline void entry(std::string &out, const Placed &placed) {
    put32(out, static_cast<std::uint32_t>(placed.offset));
    put32(out, static_cast<std::uint32_t>(placed.length));

    for (size_t at = 0; at < 8; ++at) {
        out.push_back(at < placed.name.size() ? placed.name[at] : '\0');
    }
}

inline std::string wad(const std::string_view kind, const std::vector<Lump> &lumps) {
    std::string data;
    std::string directory;

    for (const Lump &lump : lumps) {
        entry(directory,
              {.offset = static_cast<std::int32_t>(12 + data.size()),
               .length = static_cast<std::int32_t>(lump.bytes.size()),
               .name = lump.name});

        data += lump.bytes;
    }

    return header(kind, static_cast<std::int32_t>(lumps.size()), static_cast<std::int32_t>(12 + data.size())) + data
           + directory;
}

// A map marker and the lumps a map is told by.
inline std::vector<Lump> map(const std::string &name) {
    return {
            {name, {}},
            {"THINGS", std::string(20, '\0')},
            {"LINEDEFS", std::string(28, '\1')},
            {"SIDEDEFS", std::string(30, '\2')},
            {"VERTEXES", std::string(8, '\3')},
            {"SECTORS", std::string(26, '\4')},
    };
}

inline std::vector<Lump> maps(const std::vector<std::string> &names) {
    std::vector<Lump> out;

    for (const std::string &name : names) {
        for (Lump &lump : map(name)) {
            out.push_back(std::move(lump));
        }
    }

    return out;
}

// Entry i is (i, 255 - i, 3i).
inline std::string playpal() {
    std::string out;

    for (int entry = 0; entry < 256; ++entry) {
        out.push_back(static_cast<char>(entry));
        out.push_back(static_cast<char>(255 - entry));
        out.push_back(static_cast<char>((entry * 3) & 0xFF));
    }

    return out;
}

inline std::array<std::uint8_t, 3> colourOf(const int index) {
    return {static_cast<std::uint8_t>(index),
            static_cast<std::uint8_t>(255 - index),
            static_cast<std::uint8_t>((index * 3) & 0xFF)};
}

// A Doom picture, one full-height post per column, pixel (x, y) at index x + y.
inline std::string patch(const int width, const int height) {
    std::string columns;
    std::string offsets;

    const size_t base = 8 + (static_cast<size_t>(width) * 4);

    for (int x = 0; x < width; ++x) {
        put32(offsets, static_cast<std::uint32_t>(base + columns.size()));

        columns.push_back('\0');
        columns.push_back(static_cast<char>(height));
        columns.push_back('\0');

        for (int y = 0; y < height; ++y) {
            columns.push_back(static_cast<char>((x + y) & 0xFF));
        }

        columns.push_back('\0');
        columns.push_back(static_cast<char>(0xFF));
    }

    std::string out;

    put16(out, static_cast<unsigned>(width));
    put16(out, static_cast<unsigned>(height));
    put16(out, 0);
    put16(out, 0);

    return out + offsets + columns;
}

inline std::uint32_t crc32(const std::string_view bytes) {
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> made{};

        for (std::uint32_t at = 0; at < 256; ++at) {
            std::uint32_t value = at;

            for (int bit = 0; bit < 8; ++bit) {
                value = (value & 1U) != 0U ? 0xEDB88320U ^ (value >> 1U) : value >> 1U;
            }

            made[at] = value;
        }

        return made;
    }();

    std::uint32_t value = 0xFFFFFFFFU;

    for (const char byte : bytes) {
        value = table[(value ^ static_cast<unsigned char>(byte)) & 0xFFU] ^ (value >> 8U);
    }

    return value ^ 0xFFFFFFFFU;
}

// A deflate stream of stored blocks: method 8 without a compressor.
inline std::string deflated(const std::string_view bytes) {
    constexpr size_t BLOCK = 0xFFFF;

    std::string out;
    size_t at = 0;

    do {
        const size_t size = std::min(BLOCK, bytes.size() - at);
        const bool last = at + size == bytes.size();

        out.push_back(last ? '\1' : '\0');
        put16(out, static_cast<unsigned>(size));
        put16(out, static_cast<unsigned>(~size & 0xFFFFU));
        out += bytes.substr(at, size);

        at += size;
    } while (at < bytes.size());

    return out;
}

constexpr unsigned STORED = 0;
constexpr unsigned DEFLATED = 8;
constexpr unsigned LZMA = 14;

struct Entry {
    std::string name;
    std::string bytes;
    unsigned method = STORED;
};

inline std::string zip(const std::vector<Entry> &entries) {
    std::string local;
    std::string central;

    for (const Entry &each : entries) {
        const auto at = static_cast<std::uint32_t>(local.size());
        const std::uint32_t crc = crc32(each.bytes);
        const std::string packed = each.method == DEFLATED ? deflated(each.bytes) : each.bytes;
        const auto size = static_cast<std::uint32_t>(each.bytes.size());
        const auto stored = static_cast<std::uint32_t>(packed.size());

        put32(local, 0x04034B50);
        put16(local, 20);
        put16(local, 0);
        put16(local, each.method);
        put32(local, 0);
        put32(local, crc);
        put32(local, stored);
        put32(local, size);
        put16(local, static_cast<unsigned>(each.name.size()));
        put16(local, 0);

        local += each.name;
        local += packed;

        put32(central, 0x02014B50);
        put16(central, 20);
        put16(central, 20);
        put16(central, 0);
        put16(central, each.method);
        put32(central, 0);
        put32(central, crc);
        put32(central, stored);
        put32(central, size);
        put16(central, static_cast<unsigned>(each.name.size()));
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put32(central, each.name.ends_with('/') ? 0x10U : 0U);
        put32(central, at);

        central += each.name;
    }

    std::string out = local;
    const auto directory = static_cast<std::uint32_t>(out.size());

    out += central;

    put32(out, 0x06054B50);
    put16(out, 0);
    put16(out, 0);
    put16(out, static_cast<unsigned>(entries.size()));
    put16(out, static_cast<unsigned>(entries.size()));
    put32(out, static_cast<std::uint32_t>(central.size()));
    put32(out, directory);
    put16(out, 0);

    return out;
}

inline std::string png(const int width, const int height, const std::uint32_t argb = 0xFF2A1A10) {
    BLImage image(width, height, BL_FORMAT_PRGB32);

    {
        BLContext context(image);

        context.fill_all(BLRgba32(argb));
        context.end();
    }

    BLArray<std::uint8_t> encoded;
    BLImageCodec codec;

    codec.find_by_name("PNG");
    image.write_to_data(encoded, codec);

    return {reinterpret_cast<const char *>(encoded.data()), encoded.size()};
}

}
