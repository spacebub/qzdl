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

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace ports {

namespace fs = std::filesystem;

struct Entry {
    std::string name;
    std::string bytes;

    // Unix permission bits, kept in the high half of the external attributes.
    std::uint32_t mode{0};
};

inline void put16(std::string &out, const std::uint32_t value) {
    out.push_back(static_cast<char>(value & 0xFFU));
    out.push_back(static_cast<char>((value >> 8U) & 0xFFU));
}

inline void put32(std::string &out, const std::uint32_t value) {
    put16(out, value & 0xFFFFU);
    put16(out, (value >> 16U) & 0xFFFFU);
}

inline std::uint32_t crc32(const std::string_view bytes) {
    std::uint32_t value = 0xFFFFFFFFU;

    for (const char byte : bytes) {
        value ^= static_cast<unsigned char>(byte);

        for (int bit = 0; bit < 8; ++bit) {
            value = (value & 1U) != 0U ? 0xEDB88320U ^ (value >> 1U) : value >> 1U;
        }
    }

    return value ^ 0xFFFFFFFFU;
}

// Stored, so the bytes are readable in the archive as they are.
inline std::string zip(const std::vector<Entry> &entries) {
    std::string local;
    std::string central;

    for (const Entry &entry : entries) {
        const auto at = static_cast<std::uint32_t>(local.size());
        const std::uint32_t crc = crc32(entry.bytes);
        const auto size = static_cast<std::uint32_t>(entry.bytes.size());
        const auto length = static_cast<std::uint32_t>(entry.name.size());

        put32(local, 0x04034B50);
        put16(local, 20);
        put16(local, 0);
        put16(local, 0);
        put32(local, 0x00210000);
        put32(local, crc);
        put32(local, size);
        put32(local, size);
        put16(local, length);
        put16(local, 0);

        local += entry.name;
        local += entry.bytes;

        put32(central, 0x02014B50);
        put16(central, (3U << 8U) | 20U);
        put16(central, 20);
        put16(central, 0);
        put16(central, 0);
        put32(central, 0x00210000);
        put32(central, crc);
        put32(central, size);
        put32(central, size);
        put16(central, length);
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put32(central, entry.mode << 16U);
        put32(central, at);

        central += entry.name;
    }

    std::string out = local;
    const auto directory = static_cast<std::uint32_t>(out.size());

    out += central;

    put32(out, 0x06054B50);
    put16(out, 0);
    put16(out, 0);
    put16(out, static_cast<std::uint32_t>(entries.size()));
    put16(out, static_cast<std::uint32_t>(entries.size()));
    put32(out, static_cast<std::uint32_t>(central.size()));
    put32(out, directory);
    put16(out, 0);

    return out;
}

inline void write(const fs::path &path, const std::string_view bytes) {
    fs::create_directories(path.parent_path());

    std::ofstream out(path, std::ios::binary | std::ios::trunc);

    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

inline std::string slurp(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);

    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

inline bool executable(const fs::path &path) {
    return (fs::status(path).permissions() & fs::perms::owner_exec) != fs::perms::none;
}

}
