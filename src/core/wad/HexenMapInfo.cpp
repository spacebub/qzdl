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

#include <algorithm>
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ttk/system/Text.h"

#include "core/util/FileStamp.h"
#include "core/wad/HexenMapInfo.h"
#include "core/wad/MapFile.h"
#include "core/wad/Wad.h"

using namespace ttk;

namespace HexenMapInfo {

namespace {

constexpr int LAST_LISTED = 99;

// A longer token is cut there, and the rest of it is the next token.
constexpr size_t LONGEST_TOKEN = 63;

// The real Hexen MAPINFO is a few kilobytes.
constexpr std::int64_t LONGEST_MAPINFO = std::int64_t{1024} * 1024;

#ifdef _WIN32
constexpr char SEPARATOR = '\\';
#else
constexpr char SEPARATOR = '/';
#endif

struct Keyword {
    std::string_view name;
    bool takesName;
    bool takesNumber;
};

constexpr std::string_view WARPTRANS = "WARPTRANS";

constexpr std::array KEYWORDS = std::to_array<Keyword>({
        {"SKY1", true, true},
        {"SKY2", true, true},
        {"DOUBLESKY", false, false},
        {"LIGHTNING", false, false},
        {"FADETABLE", true, false},
        {"CLUSTER", false, true},
        {WARPTRANS, false, true},
        {"NEXT", false, true},
        {"CDTRACK", false, true},
        {"CD_START_TRACK", false, true},
        {"CD_END1_TRACK", false, true},
        {"CD_END2_TRACK", false, true},
        {"CD_END3_TRACK", false, true},
        {"CD_INTERMISSION_TRACK", false, true},
        {"CD_TITLE_TRACK", false, true},
});

// The port compares a plain char. On x86, where it mostly runs, char is signed, so bytes from 128 up are blank.
// Every build here reads them that way.
bool blank(const char letter) {
    return static_cast<signed char>(letter) <= ' ';
}

class Script {
public:
    explicit Script(const std::string_view text) : _text(text) {}

    // False at the end of the text.
    bool next() {
        if (_again) {
            _again = false;

            return true;
        }

        if (!skipToToken()) {
            return false;
        }

        _token.clear();

        if (_text[_at] == '"') {
            ++_at;

            while (_at < _text.size() && _text[_at] != '"') {
                _token += _text[_at++];

                if (_token.size() == LONGEST_TOKEN) {
                    break;
                }
            }

            ++_at;
        } else {
            while (_at < _text.size() && !blank(_text[_at]) && _text[_at] != ';') {
                _token += _text[_at++];

                if (_token.size() == LONGEST_TOKEN) {
                    break;
                }
            }
        }

        return true;
    }

    // A whole number as strtol reads it, so 0x and a leading zero pick the base. The cut to int matches the port
    // built for the same system.
    bool number(int &out) {
        if (!next()) {
            return false;
        }

        char *stop = nullptr;
        const long value = std::strtol(_token.c_str(), &stop, 0);

        out = static_cast<int>(value);

        return *stop == '\0';
    }

    // The port compares C strings, so a NUL ends the token there.
    [[nodiscard]] bool is(const std::string_view word) const {
        return Text::iequals(std::string_view(_token).substr(0, _token.find('\0')), word);
    }

    void unget() { _again = true; }

private:
    bool skipToToken() {
        while (true) {
            while (_at < _text.size() && blank(_text[_at])) {
                ++_at;
            }

            if (_at >= _text.size()) {
                return false;
            }

            if (_text[_at] != ';') {
                return true;
            }

            while (_text[_at++] != '\n') {
                if (_at >= _text.size()) {
                    return false;
                }
            }
        }
    }

    std::string_view _text;
    std::string _token;
    size_t _at{0};
    bool _again{false};
};

const Keyword *keywordOf(const Script &script) {
    for (const Keyword &each : KEYWORDS) {
        if (script.is(each.name)) {
            return &each;
        }
    }

    return nullptr;
}

// The warptrans of the map, or none when the port would stop on the entry.
std::optional<int> entryOf(Script &script, const int map) {
    int warpTrans = map;

    // The map name follows the number.
    if (!script.next()) {
        return std::nullopt;
    }

    while (script.next()) {
        if (script.is("MAP")) {
            script.unget();

            break;
        }

        const Keyword *keyword = keywordOf(script);
        int number = 0;

        if (keyword == nullptr || (keyword->takesName && !script.next())
            || (keyword->takesNumber && !script.number(number))) {
            return std::nullopt;
        }

        if (keyword->name == WARPTRANS) {
            warpTrans = number;
        }
    }

    return warpTrans;
}

// W_AddFile reads a file whose name ends in wad as a WAD.
bool wadNamed(const std::string_view file) {
    return Text::iends_with(file, "wad");
}

// Any other file is one lump, named as M_ExtractFileBase names it: up to the first dot, at most eight letters.
std::string_view lumpNameOf(const std::string_view file) {
    const size_t slash = file.rfind(SEPARATOR);
    const std::string_view base = slash == std::string_view::npos ? file : file.substr(slash + 1);

    return base.substr(0, std::min<size_t>(base.find('.'), 8));
}

// None when the file cannot be opened, so the port loads nothing from it.
std::optional<std::string> wholeFile(const std::string &file) {
    const std::filesystem::path path(file);
    std::ifstream in(path, std::ios::binary);

    if (!in) {
        return std::nullopt;
    }

    std::error_code code;
    const std::uintmax_t size = std::filesystem::file_size(path, code);

    if (code || std::cmp_greater(size, LONGEST_MAPINFO)) {
        return std::string();
    }

    std::string bytes(static_cast<size_t>(size), '\0');

    return in.read(bytes.data(), static_cast<std::streamsize>(size)) ? bytes : std::string();
}

struct Known {
    FileStamp stamp;
    std::bitset<LAST_WARPED + 1> maps;
    bool hasMapInfo{false};

    // Up to LAST_WARPED, since -warp never reaches a later map.
    std::vector<Listed> listed;
};

using Seen = std::map<std::string, Known, std::less<>>;

Known read(const std::string &file, const FileStamp &stamp) {
    Known made{.stamp = stamp, .maps = {}, .hasMapInfo = false, .listed = {}};
    std::optional<std::string> text;

    if (wadNamed(file)) {
        for (const std::string &name : MapFile::maps(file).names) {
            if (const int map = mapNumber(name); map > 0) {
                made.maps.set(static_cast<size_t>(map));
            }
        }

        text = Wad(file).lastLump("MAPINFO", LONGEST_MAPINFO);
    } else if (Text::iequals(lumpNameOf(file), "MAPINFO")) {
        text = wholeFile(file);
    }

    if (text) {
        made.hasMapInfo = true;

        for (const Listed &each : parse(*text).value_or(std::vector<Listed>())) {
            if (each.map <= LAST_WARPED) {
                made.listed.push_back(each);
            }
        }
    }

    return made;
}

const Known &known(Seen &seen, const std::string &file) {
    const FileStamp now = FileStamp::of(file);

    if (const auto found = seen.find(file); found != seen.end() && found->second.stamp == now) {
        return found->second;
    }

    return seen.insert_or_assign(file, read(file, now)).first->second;
}

}

std::optional<std::vector<Listed>> parse(const std::string_view text) {
    std::array<std::optional<int>, LAST_LISTED + 1> warpTrans{};
    Script script(text);

    while (script.next()) {
        int map = 0;

        if (!script.is("MAP") || !script.number(map) || map < 1 || map > LAST_LISTED) {
            return std::nullopt;
        }

        const std::optional<int> entry = entryOf(script, map);

        if (!entry) {
            return std::nullopt;
        }

        warpTrans.at(static_cast<size_t>(map)) = entry;
    }

    std::vector<Listed> out;

    for (int map = 1; map <= LAST_LISTED; ++map) {
        if (const std::optional<int> each = warpTrans.at(static_cast<size_t>(map))) {
            out.push_back({.map = map, .warpTrans = *each});
        }
    }

    return out;
}

int mapNumber(const std::string_view lump) {
    const auto digit = [](const char letter) { return letter >= '0' && letter <= '9'; };

    if (lump.size() != 5 || !Text::iequals(lump.substr(0, 3), "MAP") || !digit(lump[3]) || !digit(lump[4])) {
        return 0;
    }

    const int map = ((lump[3] - '0') * 10) + (lump[4] - '0');

    return map <= LAST_WARPED ? map : 0;
}

std::string lumpOf(const int map) {
    return (map < 10 ? "MAP0" : "MAP") + std::to_string(map);
}

Warps warps(const std::span<const std::string> files) {
    // Bounded like MapFile::maps. Emptied only here, so no entry goes away while this call holds it.
    constexpr size_t KEPT = 512;

    static Seen seen;

    if (seen.size() + files.size() > KEPT) {
        seen.clear();
    }

    std::bitset<LAST_WARPED + 1> maps;
    const Known *last = nullptr;

    for (const std::string &file : files) {
        const Known &each = known(seen, file);

        maps |= each.maps;

        if (each.hasMapInfo) {
            last = &each;
        }
    }

    Warps out;

    if (last == nullptr) {
        return out;
    }

    // A map MAPINFO leaves out has warptrans 0, and -warp takes the lowest map with the number.
    std::array<int, LAST_WARPED + 1> warpTrans{};

    for (const Listed &each : last->listed) {
        warpTrans.at(static_cast<size_t>(each.map)) = each.warpTrans;
    }

    for (const Listed &each : last->listed) {
        const auto map = static_cast<size_t>(each.map);

        // Ends at the map itself at the latest.
        size_t first = 1;

        while (warpTrans.at(first) != each.warpTrans) {
            ++first;
        }

        if (first == map && maps.test(map)) {
            out.reached.set(map);
            out.warpTrans.at(map) = each.warpTrans;
        }
    }

    return out;
}

}
