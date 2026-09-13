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
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <blend2d/blend2d.h>

#include "core/config/Config.h"
#include "support/Corpus.h"
#include "support/Fixtures.h"
#include "support/Sandbox.h"

namespace {

void put16(std::string &out, const unsigned value) {
    out.push_back(static_cast<char>(value & 0xFF));
    out.push_back(static_cast<char>((value >> 8) & 0xFF));
}

void put32(std::string &out, const std::uint32_t value) {
    put16(out, value & 0xFFFF);
    put16(out, (value >> 16) & 0xFFFF);
}

void putName(std::string &out, const std::string_view name) {
    for (size_t at = 0; at < 8; ++at) {
        out.push_back(at < name.size() ? name[at] : '\0');
    }
}

std::uint32_t crc32Of(const std::string &bytes) {
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

void write(const std::filesystem::path &path, const std::string &bytes) {
    std::filesystem::create_directories(path.parent_path());

    std::ofstream out(path, std::ios::binary | std::ios::trunc);

    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

// A Doom picture: one full-height post per column.
std::string patch(const int width, const int height) {
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

std::string playpal() {
    std::string out;

    for (int entry = 0; entry < 256; ++entry) {
        out.push_back(static_cast<char>(entry));
        out.push_back(static_cast<char>(255 - entry));
        out.push_back(static_cast<char>((entry * 3) & 0xFF));
    }

    return out;
}

struct Lump {
    std::string name;
    std::string bytes;
};

std::vector<Lump> mapLumps(const int maps) {
    std::vector<Lump> out;

    for (int at = 1; at <= maps; ++at) {
        char named[8] = {};

        std::snprintf(named, sizeof(named), "MAP%02d", at);

        out.push_back({named, {}});
        out.push_back({"THINGS", std::string(200, '\0')});
        out.push_back({"LINEDEFS", std::string(1400, '\1')});
        out.push_back({"SIDEDEFS", std::string(1800, '\2')});
        out.push_back({"VERTEXES", std::string(600, '\3')});
        out.push_back({"SECTORS", std::string(520, '\4')});
    }

    return out;
}

std::string wadBytes(const char *kind, const std::vector<Lump> &lumps) {
    std::string data;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> placed;

    placed.reserve(lumps.size());

    for (const Lump &lump : lumps) {
        placed.emplace_back(static_cast<std::uint32_t>(12 + data.size()),
                            static_cast<std::uint32_t>(lump.bytes.size()));

        data += lump.bytes;
    }

    std::string out(kind);

    put32(out, static_cast<std::uint32_t>(lumps.size()));
    put32(out, static_cast<std::uint32_t>(12 + data.size()));

    out += data;

    for (size_t at = 0; at < lumps.size(); ++at) {
        put32(out, placed[at].first);
        put32(out, placed[at].second);
        putName(out, lumps[at].name);
    }

    return out;
}

std::string zipBytes(const std::vector<std::pair<std::string, std::string>> &entries) {
    std::string local;
    std::string central;

    for (const auto &[name, bytes] : entries) {
        const std::uint32_t at = static_cast<std::uint32_t>(local.size());
        const std::uint32_t crc = crc32Of(bytes);
        const auto size = static_cast<std::uint32_t>(bytes.size());

        put32(local, 0x04034B50);
        put16(local, 20);
        put16(local, 0);
        put16(local, 0);
        put32(local, 0);
        put32(local, crc);
        put32(local, size);
        put32(local, size);
        put16(local, static_cast<unsigned>(name.size()));
        put16(local, 0);

        local += name;
        local += bytes;

        put32(central, 0x02014B50);
        put16(central, 20);
        put16(central, 20);
        put16(central, 0);
        put16(central, 0);
        put32(central, 0);
        put32(central, crc);
        put32(central, size);
        put32(central, size);
        put16(central, static_cast<unsigned>(name.size()));
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put32(central, 0);
        put32(central, at);

        central += name;
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

std::string pngBytes(const int width, const int height) {
    BLImage image(width, height, BL_FORMAT_PRGB32);

    {
        BLContext context(image);

        BLGradient sweep(BLLinearGradientValues(0, 0, width, height));

        sweep.add_stop(0.0, BLRgba32(0xFF2A1A10));
        sweep.add_stop(1.0, BLRgba32(0xFFD2691E));

        context.fill_all(sweep);
        context.end();
    }

    BLArray<std::uint8_t> encoded;
    BLImageCodec codec;

    codec.find_by_name("PNG");
    image.write_to_data(encoded, codec);

    return {reinterpret_cast<const char *>(encoded.data()), encoded.size()};
}

std::filesystem::path &shelf() {
    static std::filesystem::path path = [] {
        const std::filesystem::path at = bench::Sandbox::root() / "corpus";

        std::filesystem::create_directories(at);

        return at;
    }();

    return path;
}

}

namespace bench::Corpus {

const std::filesystem::path &iwad() {
    static const std::filesystem::path path = [] {
        std::vector<Lump> lumps = {
            {"PLAYPAL", playpal()},
            {"IWADINFO", "IWadInfo\n{\n\tName = \"Benchmark Doom\"\n}\n"},
            {"TITLEPIC", patch(320, 200)},
            {"CREDIT", patch(320, 200)},
        };

        for (const Lump &lump : mapLumps(32)) {
            lumps.push_back(lump);
        }

        const std::filesystem::path at = shelf() / "bench.wad";

        write(at, wadBytes("IWAD", lumps));

        return at;
    }();

    return path;
}

const std::filesystem::path &pwad(const int maps) {
    static std::map<int, std::filesystem::path> made;

    if (const auto found = made.find(maps); found != made.end()) {
        return found->second;
    }

    const std::filesystem::path at = shelf() / ("bench-" + std::to_string(maps) + ".wad");

    write(at, wadBytes("PWAD", mapLumps(maps)));

    return made.emplace(maps, at).first->second;
}

const std::filesystem::path &pk3() {
    static const std::filesystem::path path = [] {
        std::vector<std::pair<std::string, std::string>> entries = {
            {"iwadinfo.txt", "IWadInfo\n{\n\tName = \"Benchmark PK3\"\n}\n"},
            {"graphics/TITLE.png", pngBytes(640, 400)},
            {"playpal.lmp", playpal()},
        };

        for (int at = 1; at <= 32; ++at) {
            char named[24] = {};

            std::snprintf(named, sizeof(named), "maps/MAP%02d.wad", at);

            entries.emplace_back(named, wadBytes("PWAD", mapLumps(1)));
        }

        const std::filesystem::path file = shelf() / "bench.pk3";

        write(file, zipBytes(entries));

        return file;
    }();

    return path;
}

const std::filesystem::path &ini() {
    static const std::filesystem::path path = [] {
        std::string text =
            "[zdl.general]\n"
            "engine=0\n"
            "iwad=0\n"
            "skill=3\n"
            "warp=MAP07\n"
            "[zdl.save]\n"
            "skill=3\n"
            "[zdl.iwads]\n";

        for (int at = 0; at < 12; ++at) {
            text += "i" + std::to_string(at) + "n=Game " + std::to_string(at) + "\n";
            text += "i" + std::to_string(at) + "f=/games/game" + std::to_string(at) + ".wad\n";
        }

        text += "[zdl.ports]\n";

        for (int at = 0; at < 12; ++at) {
            text += "p" + std::to_string(at) + "n=Port " + std::to_string(at) + "\n";
            text += "p" + std::to_string(at) + "f=/ports/port" + std::to_string(at) + "\n";
        }

        text += "[zdl.files]\n";

        for (int at = 0; at < 64; ++at) {
            text += "f" + std::to_string(at) + "=/addons/addon" + std::to_string(at) + ".wad\n";
        }

        const std::filesystem::path file = shelf() / "legacy.zdl";

        write(file, text);

        return file;
    }();

    return path;
}

const std::filesystem::path &json(const int profiles, const int files) {
    static std::map<std::pair<int, int>, std::filesystem::path> made;

    const std::pair key{profiles, files};

    if (const auto found = made.find(key); found != made.end()) {
        return found->second;
    }

    const std::filesystem::path at =
        shelf() / ("config-" + std::to_string(profiles) + "x" + std::to_string(files) + ".json");

    Fixtures::config(profiles, files).save(at);

    return made.emplace(key, at).first->second;
}

const std::vector<std::string> &addons(const int count) {
    static std::map<int, std::vector<std::string>> made;

    if (const auto found = made.find(count); found != made.end()) {
        return found->second;
    }

    std::vector<std::string> paths;

    paths.reserve(static_cast<size_t>(count));

    const std::string bytes = wadBytes("PWAD", mapLumps(1));

    for (int at = 0; at < count; ++at) {
        const std::filesystem::path file =
            shelf() / "addons" / ("addon" + std::to_string(at) + ".wad");

        write(file, bytes);

        paths.push_back(file.string());
    }

    return made.emplace(count, std::move(paths)).first->second;
}

}
