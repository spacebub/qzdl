/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
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
#include <cstdint>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <map>
#include <mutex>
#include <ranges>
#include <string_view>
#include <system_error>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <shlobj.h>
#endif

#include "ttk/system/Env.h"
#include "ttk/system/Json.h"
#include "ttk/system/Paths.h"
#include "ttk/system/Text.h"

#include "core/launch/Dialect.h"
#include "core/launch/PortSaves.h"
#include "core/launch/Storage.h"
#include "core/ports/Detect.h"
#include "core/util/FileStamp.h"
#include "core/util/Ini.h"
#include "core/wad/MapFile.h"
#include "external/miniz/miniz.h"

using namespace ttk;

namespace PortSaves {

namespace {

enum class Kind : std::uint8_t {
    none,
    uzdoom,
    gzdoom,
    vkdoom,
    zdoom,
    zandronum,
    prboom,
    helion,
    woof,
    nugget,
    dsda,
    nyan,
    chocolate,
    crispy,
};

Kind kindOf(const std::filesystem::path &program) {
    if (Detect::flatpakExport(program)) {
        const std::string_view runs = Detect::flatpakProgram(program.filename().string());

        return runs.empty() ? Kind::none : kindOf(std::filesystem::path(runs));
    }

    const std::string name = Text::lower(program.stem().string());

    // Most specific first: gzdoom and uzdoom both contain zdoom.
    if (name.contains("uzdoom")) {
        return Kind::uzdoom;
    }

    if (name.contains("gzdoom")) {
        return Kind::gzdoom;
    }

    if (name.contains("vkdoom")) {
        return Kind::vkdoom;
    }

    if (name == "zdoom") {
        return Kind::zdoom;
    }

    if (name.contains("zandronum")) {
        return Kind::zandronum;
    }

    if (name.contains("prboom-plus") || name.contains("glboom-plus")) {
        return Kind::prboom;
    }

    if (name.contains("helion")) {
        return Kind::helion;
    }

    if (name.contains("nugget")) {
        return Kind::nugget;
    }

    if (name.contains("woof")) {
        return Kind::woof;
    }

    if (name.contains("dsda")) {
        return Kind::dsda;
    }

    if (name.contains("nyan")) {
        return Kind::nyan;
    }

    if (name.contains("crispy-doom")) {
        return Kind::crispy;
    }

    if (name.contains("chocolate-doom")) {
        return Kind::chocolate;
    }

    return Kind::none;
}

std::optional<Dialect::Id> dialectOf(const Kind kind) {
    switch (kind) {
        case Kind::uzdoom:
            return Dialect::Id::uzdoom;
        case Kind::gzdoom:
        case Kind::vkdoom:
            return Dialect::Id::gzdoom;
        case Kind::zdoom:
            return Dialect::Id::zdoom;
        case Kind::zandronum:
            return Dialect::Id::zandronum;
        case Kind::prboom:
            return Dialect::Id::prboom;
        case Kind::helion:
            return Dialect::Id::helion;
        case Kind::woof:
        case Kind::nugget:
            return Dialect::Id::woof;
        case Kind::dsda:
        case Kind::nyan:
            return Dialect::Id::dsda;
        case Kind::chocolate:
        case Kind::crispy:
            return Dialect::Id::chocolate;
        case Kind::none:
            break;
    }

    return std::nullopt;
}

// The port the profile launches, when the name says which one and the launch speaks its dialect.
Kind kindOf(const Config &config) {
    const PortEntry *entry = config.findPort(config.activeProfile().port);

    if (entry == nullptr || entry->dosbox) {
        return Kind::none;
    }

    const Kind kind = kindOf(std::filesystem::path(entry->file));

    return Dialect::of(*entry).id == dialectOf(kind) ? kind : Kind::none;
}

#ifndef _WIN32
std::filesystem::path sandbox(const std::filesystem::path &program) {
    return Detect::flatpakExport(program) ? Paths::home_directory() / ".var" / "app" / program.filename()
                                          : std::filesystem::path();
}

// An XDG base directory as the port sees it. A Flatpak points them into its own folder.
std::filesystem::path xdg(const std::filesystem::path &program, const char *variable, const char *fallback) {
    if (const std::filesystem::path inside = sandbox(program); !inside.empty()) {
        return inside / (std::string_view(variable) == "XDG_DATA_HOME" ? "data" : "config");
    }

    if (std::string set = Env::get(variable); !set.empty()) {
        return set;
    }

    return Paths::home_directory() / fallback;
}

std::filesystem::path dataHome(const std::filesystem::path &program) {
    return xdg(program, "XDG_DATA_HOME", ".local/share");
}

std::filesystem::path configHome(const std::filesystem::path &program) {
    return xdg(program, "XDG_CONFIG_HOME", ".config");
}
#else
bool underProgramFiles(const std::filesystem::path &directory) {
    const std::string at = Text::lower(directory.generic_string()) + "/";

    return std::ranges::any_of(std::array{"ProgramFiles", "ProgramFiles(x86)"}, [&at](const char *variable) {
        const std::string root = Env::get(variable);

        return !root.empty() && at.starts_with(Text::lower(std::filesystem::path(root).generic_string()) + "/");
    });
}

std::filesystem::path knownFolder(REFKNOWNFOLDERID id, const char *fallback) {
    PWSTR raw = nullptr;
    std::filesystem::path folder;

    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &raw)) && raw != nullptr) {
        folder = raw;
    }

    CoTaskMemFree(raw);

    return folder.empty() ? Paths::home_directory() / fallback : folder;
}

std::filesystem::path savedGames() {
    static const std::filesystem::path folder = knownFolder(FOLDERID_SavedGames, "Saved Games");

    return folder;
}

std::filesystem::path documents() {
    static const std::filesystem::path folder = knownFolder(FOLDERID_Documents, "Documents");

    return folder;
}
#endif

bool isFile(const std::filesystem::path &path) {
    std::error_code code;

    return std::filesystem::is_regular_file(path, code);
}

// Read again only once the file or folder changes, since every profile edit asks for the folder.
template <typename Read>
std::string remembered(const std::filesystem::path &path, const std::string_view what, const Read &read) {
    static std::mutex guard;
    using Answers = std::map<std::string, std::pair<FileStamp, std::string>, std::less<>>;

    static std::map<std::filesystem::path::string_type, Answers, std::less<>> seen;

    const FileStamp now = FileStamp::of(path);

    {
        const std::scoped_lock held(guard);

        if (const auto file = seen.find(path.native()); file != seen.end()) {
            if (const auto found = file->second.find(what); found != file->second.end() && found->second.first == now) {
                return found->second.second;
            }
        }
    }

    std::string value = read();

    const std::scoped_lock held(guard);

    seen[path.native()].insert_or_assign(std::string(what), std::pair{now, value});

    return value;
}

// What GZDoom and its forks do to a save_dir: a leading ~, $PROGDIR and other variables are expanded.
std::filesystem::path expanded(const std::string &value, const std::filesystem::path &program) {
    std::string out;

    for (size_t at = 0; at < value.size();) {
        size_t end = at + 1;

        while (value[at] == '$' && end < value.size()
               && (std::isalnum(static_cast<unsigned char>(value[end])) != 0 || value[end] == '_')) {
            end++;
        }

        if (at == 0 && value[0] == '~' && (value.size() == 1 || value[1] == '/' || value[1] == '\\')) {
            out += Paths::home_directory().string();
        } else if (value[at] == '$' && end > at + 1) {
            const std::string name = value.substr(at + 1, end - at - 1);

            out += Text::iequals(name, "PROGDIR") ? program.parent_path().string() : Env::get(name.c_str());
        } else {
            out += value.substr(at, end - at);
        }

        at = end;
    }

    return out;
}

// save_dir from the [GlobalSettings] of a ZDoom ini. Empty when it is not set.
std::filesystem::path saveDir(const std::filesystem::path &ini, const std::filesystem::path &program) {
    if (ini.empty()) {
        return {};
    }

    const std::string value = remembered(ini, "save_dir", [&ini] {
        Ini read;

        if (!read.read(ini)) {
            return std::string();
        }

        const Ini::Section *global = read.section("GlobalSettings");

        return global == nullptr ? std::string() : Text::trim(global->get("save_dir"));
    });

    return value.empty() ? std::filesystem::path() : expanded(value, program);
}

// The value of a key in a Boom style cfg, where each line is a name and a value. Quotes are dropped.
std::string cfgValue(const std::filesystem::path &cfg, const std::string_view key) {
    return remembered(cfg, key, [&cfg, key] {
        std::ifstream in(cfg);
        std::string line;

        while (std::getline(in, line)) {
            const std::string trimmed = Text::trim(line);
            const size_t gap = trimmed.find_first_of(" \t");

            if (gap == std::string::npos || !Text::iequals(std::string_view(trimmed).substr(0, gap), key)) {
                continue;
            }

            std::string value = Text::trim(trimmed.substr(gap));

            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }

            return value;
        }

        return std::string();
    });
}

// The ports that sort saves into a folder per game and load them by a name relative to it.
bool sortsByGame(const Kind kind) {
    return kind == Kind::uzdoom || kind == Kind::gzdoom || kind == Kind::vkdoom;
}

// GAMENAME in the version.h of a ZDoom port, which names its folders and, lowercased, its ini.
std::string_view zdoomName(const Kind kind) {
    switch (kind) {
        case Kind::uzdoom:
            return "UZDoom";
        case Kind::gzdoom:
            return "GZDoom";
        case Kind::vkdoom:
            return "VKDoom";
        case Kind::zdoom:
            return "ZDoom";
        default:
            return "Zandronum";
    }
}

// The default save root of the ZDoom ports, before save_dir and the folder per game.
std::filesystem::path zdoomRoot(const Kind kind, const std::filesystem::path &program) {
#ifdef _WIN32
    const std::filesystem::path beside = program.parent_path();

    if (sortsByGame(kind)) {
        if (!underProgramFiles(beside) && isFile(beside / (Text::lower(zdoomName(kind)) + "_portable.ini"))) {
            return beside / "Save";
        }

        return savedGames() / zdoomName(kind);
    }

    // A copy that can write beside itself keeps everything there.
    return underProgramFiles(beside) ? savedGames() / zdoomName(kind) : beside;
#else
    switch (kind) {
        case Kind::uzdoom:
            return dataHome(program) / "games" / "uzdoom" / "savegames";
        case Kind::gzdoom:
            return Paths::home_directory() / ".config" / "gzdoom" / "savegames";
        case Kind::vkdoom:
            return Paths::home_directory() / ".config" / "vkdoom" / "savegames";
        case Kind::zdoom:
            return Paths::home_directory() / ".config" / "zdoom";
        default:
            return Paths::home_directory() / ".config" / "zandronum";
    }
#endif
}

std::filesystem::path zdoomIni(const Kind kind, const std::filesystem::path &program) {
    const std::string name = Text::lower(zdoomName(kind));
#ifdef _WIN32
    const std::filesystem::path beside = program.parent_path();

    if (sortsByGame(kind)) {
        if (const std::filesystem::path portable = beside / (name + "_portable.ini");
            !underProgramFiles(beside) && isFile(portable)) {
            return portable;
        }

        return documents() / "My Games" / zdoomName(kind) / (name + ".ini");
    }

    return underProgramFiles(beside) ? std::filesystem::path() : beside / (name + "-" + Env::get("USERNAME") + ".ini");
#else
    if (kind == Kind::uzdoom) {
        return configHome(program) / name / (name + ".ini");
    }

    return Paths::home_directory() / ".config" / name / (name + ".ini");
#endif
}

std::filesystem::path helionFolder(const std::filesystem::path &program) {
    if (const std::filesystem::path beside = program.parent_path(); isFile(beside / "config.ini")) {
        return beside;
    }

#ifdef _WIN32
    return savedGames() / "Helion";
#else
    const std::filesystem::path home = configHome(program);
    std::error_code code;

    return std::filesystem::is_directory(home / "helion", code) ? home / "helion" : home / "Helion";
#endif
}

std::filesystem::path boomPrefs(const Kind kind, const std::filesystem::path &program) {
#ifdef _WIN32
    (void)kind;

    return program.parent_path();
#else
    if (kind == Kind::prboom) {
        const std::filesystem::path legacy = Paths::home_directory() / ".prboom-plus";
        std::error_code code;

        return std::filesystem::is_directory(legacy, code) ? legacy : dataHome(program) / "prboom-plus";
    }

    return dataHome(program) / (kind == Kind::woof ? "woof" : "nugget-doom");
#endif
}

bool holdsDsg(const std::filesystem::path &folder) {
    return remembered(folder,
                      "dsg",
                      [&folder] {
                          std::error_code code;

                          for (std::filesystem::directory_iterator walk(folder, code), end; walk != end && !code;
                               walk.increment(code)) {
                              if (Text::iends_with(walk->path().filename().string(), ".dsg")) {
                                  return std::string("1");
                              }
                          }

                          return std::string();
                      })
           == "1";
}

bool mapFor(const std::string_view name, const bool commercial) {
    const auto digit = [&name](const size_t at, const char low, const char high) {
        return name[at] >= low && name[at] <= high;
    };

    if (commercial) {
        return name.size() == 5 && name.starts_with("MAP") && digit(3, '0', '3') && digit(4, '0', '9')
               && name != "MAP00" && (name[3] != '3' || name[4] <= '4');
    }

    return name.size() == 4 && name[0] == 'E' && digit(1, '1', '4') && name[2] == 'M' && digit(3, '1', '9');
}

std::vector<std::string> lumpsOf(const std::string_view wad) {
    const auto word = [&wad](const size_t at) {
        std::uint32_t out = 0;

        for (size_t byte = 4; byte > 0; byte--) {
            out = (out << 8U) | static_cast<unsigned char>(wad[at + byte - 1]);
        }

        return out;
    };

    if (wad.size() < 12 || (!wad.starts_with("PWAD") && !wad.starts_with("IWAD"))) {
        return {};
    }

    const std::uint32_t count = word(4);
    const std::uint32_t table = word(8);
    std::vector<std::string> out;

    for (std::uint32_t at = 0; at < count && table + ((at + 1) * 16ULL) <= wad.size(); at++) {
        const std::string_view name = wad.substr(table + (at * 16ULL) + 8, 8);

        out.emplace_back(Text::upper(name.substr(0, name.find('\0'))));
    }

    return out;
}

struct Loaded {
    std::string name;
    std::vector<std::string> lumps;
};

// The .wad files Woof loads from the add-ons, in order. A zip or pk3 adds each .wad at its root.
std::vector<Loaded> loadedWads(const Config &config) {
    std::vector<Loaded> out;

    for (const FileEntry &entry : config.activeProfile().files) {
        if (!entry.enabled) {
            continue;
        }

        if (Text::iends_with(entry.file, ".wad")) {
            const MapFile::Maps &maps = MapFile::maps(entry.file);

            out.push_back({.name = std::filesystem::path(entry.file).filename().string(), .lumps = maps.names});

            continue;
        }

        if (!Text::iends_with(entry.file, ".zip") && !Text::iends_with(entry.file, ".pk3")) {
            continue;
        }

        mz_zip_archive zip{};

        if (mz_zip_reader_init_file(&zip, entry.file.c_str(), 0) == 0) {
            continue;
        }

        for (mz_uint at = 0; at < mz_zip_reader_get_num_files(&zip); at++) {
            std::array<char, 512> named{};

            mz_zip_reader_get_filename(&zip, at, named.data(), static_cast<mz_uint>(named.size()));

            const std::string_view inner(named.data());

            if (inner.contains('/') || !Text::iends_with(inner, ".wad")) {
                continue;
            }

            size_t size = 0;
            void *data = mz_zip_reader_extract_to_heap(&zip, at, &size, 0);

            if (data != nullptr) {
                const std::string_view bytes(static_cast<const char *>(data), size);

                out.push_back(Loaded{.name = std::string(inner), .lumps = lumpsOf(bytes)});
            }

            mz_free(data);
        }

        mz_zip_reader_end(&zip);
    }

    return out;
}

// Woof files saves under the first .wad that holds a map that wins, which a later file can take
// over. Without one it uses the game.
std::string mapWad(const Config &config) {
    const std::string iwad = config.activeIwadFile();
    const bool commercial = !iwad.empty() && MapFile::maps(iwad).mapxx;
    const std::vector<Loaded> wads = loadedWads(config);
    std::map<std::string, size_t> winner;

    for (size_t at = 0; at < wads.size(); at++) {
        for (const std::string &lump : wads[at].lumps) {
            if (mapFor(lump, commercial)) {
                winner[lump] = at;
            }
        }
    }

    for (size_t at = 0; at < wads.size(); at++) {
        if (std::ranges::any_of(winner, [at](const auto &each) { return each.second == at; })) {
            return wads[at].name;
        }
    }

    return std::filesystem::path(iwad).filename().string();
}

std::filesystem::path woofFolder(const Config &config, const Kind kind, const std::filesystem::path &program) {
    const std::filesystem::path prefs = boomPrefs(kind, program);
    const std::filesystem::path cfg = prefs / (kind == Kind::woof ? "woof.cfg" : "nugget-doom.cfg");
    std::filesystem::path base = prefs;
    std::filesystem::path sorted = prefs / "savegames";

    if (kind == Kind::nugget) {
        if (const std::string moved = cfgValue(cfg, "savegame_dir"); !moved.empty()) {
            base = moved;
            sorted = moved;
        }
    }

    const std::string organize = cfgValue(cfg, "organize_savefiles");
    const bool flat = organize == "0" || (organize != "1" && holdsDsg(base));

    if (flat) {
        return base;
    }

    const std::string wad = mapWad(config);

    return wad.empty() ? std::filesystem::path() : sorted / wad;
}

std::string_view dsdaName(const Kind kind) {
    return kind == Kind::nyan ? "nyan-doom" : "dsda-doom";
}

std::filesystem::path dsdaBase(const Kind kind, const std::filesystem::path &program) {
#ifdef _WIN32
    (void)kind;

    return program.parent_path();
#else
    const std::filesystem::path legacy = Paths::home_directory() / ("." + std::string(dsdaName(kind)));
    std::error_code code;

    return std::filesystem::exists(legacy, code) ? legacy : dataHome(program) / dsdaName(kind);
#endif
}

// The folder name DSDA-Doom makes from a .wad file: lowercase, without the extension.
std::string dsdaStem(const std::string_view file) {
    const std::string name = Text::lower(std::filesystem::path(file).filename().string());

    return name.size() > 4 && name.ends_with(".wad") ? name.substr(0, name.size() - 4) : std::string();
}

// DSDA-Doom unpacks a zip flat, and loads the .wad files in it sorted by name.
std::vector<std::string> zippedWads(const std::string &archive) {
    // Joined with a newline, which no .wad name in a zip holds.
    const std::string joined = remembered(archive, "wads", [&archive] {
        mz_zip_archive zip{};
        std::vector<std::string> names;

        if (mz_zip_reader_init_file(&zip, archive.c_str(), 0) == 0) {
            return std::string();
        }

        for (mz_uint at = 0; at < mz_zip_reader_get_num_files(&zip); at++) {
            std::array<char, 512> named{};

            mz_zip_reader_get_filename(&zip, at, named.data(), static_cast<mz_uint>(named.size()));

            if (std::string file = Text::lower(std::filesystem::path(named.data()).filename().string());
                file.ends_with(".wad")) {
                names.push_back(std::move(file));
            }
        }

        mz_zip_reader_end(&zip);
        std::ranges::sort(names);

        return Text::join(names, "\n");
    });

    return Text::split(joined, '\n');
}

// The folders DSDA-Doom nests under its data root: the game, then up to eight add-on WADs in load order.
// It stops on a PK3, so only .wad files and the .wad files in a .zip count.
std::filesystem::path dsdaDataFolder(const Config &config) {
    constexpr size_t ADD_ONS = 8;

    std::filesystem::path out = dsdaStem(config.activeIwadFile());
    size_t added = 0;

    const auto add = [&out, &added](const std::string &stem) {
        if (!stem.empty() && added < ADD_ONS) {
            out /= stem;
            added++;
        }
    };

    for (const FileEntry &entry : config.activeProfile().files) {
        if (!entry.enabled) {
            continue;
        }

        if (Text::iends_with(entry.file, ".zip")) {
            for (const std::string &inner : zippedWads(entry.file)) {
                add(dsdaStem(inner));
            }
        } else if (Text::iends_with(entry.file, ".wad")) {
            add(dsdaStem(entry.file));
        }
    }

    return out;
}

std::filesystem::path dsdaFolder(const Config &config, const Kind kind, const std::filesystem::path &program) {
    const std::filesystem::path base = dsdaBase(kind, program);

    if (cfgValue(base / (std::string(dsdaName(kind)) + ".cfg"), "dsda_organized_saves") == "0") {
        const std::string moved = Env::get("DOOMSAVEDIR");

        return moved.empty() ? base : std::filesystem::path(moved);
    }

    if (config.activeIwadFile().empty()) {
        return {};
    }

    const std::string moved = Env::get("DOOMDATADIR");

    return (moved.empty() ? base : std::filesystem::path(moved))
           / (kind == Kind::nyan ? "nyan_doom_data" : "dsda_doom_data") / dsdaDataFolder(config);
}

#ifndef _WIN32
// Chocolate and Crispy name the folder after the game, never after the file that was loaded.
std::string missionWad(const std::filesystem::path &iwad, const Kind kind) {
    using Named = std::pair<std::string_view, std::string_view>;

    static constexpr std::array NAMED = {
            Named{"doom.wad", "doom.wad"},
            Named{"doom1.wad", "doom.wad"},
            Named{"doom2.wad", "doom2.wad"},
            Named{"doom2f.wad", "doom2.wad"},
            Named{"plutonia.wad", "plutonia.wad"},
            Named{"tnt.wad", "tnt.wad"},
            Named{"chex.wad", "chex.wad"},
            Named{"hacx.wad", "hacx.wad"},
            Named{"freedoom1.wad", "freedoom1.wad"},
            Named{"freedoom2.wad", "freedoom2.wad"},
            Named{"freedm.wad", "freedm.wad"},
            Named{"rekkrsa.wad", "doom.wad"},
            Named{"rekkrsl.wad", "doom.wad"},
    };

    const std::string name = Text::lower(iwad.filename().string());
    const bool rekkr = name.starts_with("rekkr");

    for (const auto &[file, mission] : NAMED) {
        if (file == name && (!rekkr || kind == Kind::crispy)) {
            return std::string(mission);
        }
    }

    const MapFile::Maps &maps = MapFile::maps(iwad.string());

    if (maps.names.empty()) {
        return "unknown.wad";
    }

    return maps.mapxx ? "doom2.wad" : "doom.wad";
}
#endif

std::filesystem::path chocolateFolder(const Config &config, const Kind kind, const std::filesystem::path &program) {
#ifdef _WIN32
    (void)kind;
    (void)program;

    // Saves land where the port runs, and a profile runs in its own folder.
    const std::filesystem::path own = Storage::profileDirectory(config.activeProfile());
    const PortEntry *entry = config.findPort(config.activeProfile().port);

    return own.empty() && entry != nullptr ? std::filesystem::path(entry->file).parent_path() : own;
#else
    const std::string iwad = config.activeIwadFile();

    if (iwad.empty()) {
        return {};
    }

    return dataHome(program) / (kind == Kind::chocolate ? "chocolate-doom" : "crispy-doom") / "savegames"
           / missionWad(iwad, kind);
#endif
}

std::string zipEntry(const std::filesystem::path &archive, const char *name) {
    mz_zip_archive zip{};

    if (mz_zip_reader_init_file(&zip, archive.string().c_str(), 0) == 0) {
        return {};
    }

    size_t size = 0;
    void *data = mz_zip_reader_extract_file_to_heap(&zip, name, &size, 0);
    std::string out = data == nullptr ? std::string() : std::string(static_cast<const char *>(data), size);

    mz_free(data);
    mz_zip_reader_end(&zip);

    return out;
}

std::uint32_t bigEndian(const std::array<char, 4> &bytes) {
    std::uint32_t out = 0;

    for (const char each : bytes) {
        out = (out << 8U) | static_cast<unsigned char>(each);
    }

    return out;
}

// A ZDoom 2.8 or Zandronum save is a PNG that names its game in a text chunk.
std::string pngText(const std::filesystem::path &file, const std::string_view key) {
    static constexpr std::string_view SIGNATURE = "\x89PNG\r\n\x1a\n";
    static constexpr std::uint32_t TEXT_LIMIT = 1U << 16U;

    std::ifstream in(file, std::ios::binary);
    std::array<char, 8> head{};

    if (!in.read(head.data(), head.size()) || std::string_view(head.data(), head.size()) != SIGNATURE) {
        return {};
    }

    std::array<char, 4> length{};
    std::array<char, 4> type{};

    while (in.read(length.data(), length.size()) && in.read(type.data(), type.size())) {
        const std::uint32_t size = bigEndian(length);
        const std::string_view kind(type.data(), type.size());

        if (kind == "IEND") {
            break;
        }

        if (kind == "tEXt" && size < TEXT_LIMIT) {
            std::string data(size, '\0');

            if (!in.read(data.data(), size)) {
                break;
            }

            if (const size_t split = data.find('\0'); split != std::string::npos && data.substr(0, split) == key) {
                return data.substr(split + 1);
            }

            in.seekg(4, std::ios::cur);
        } else {
            in.seekg(static_cast<std::streamoff>(size) + 4, std::ios::cur);
        }
    }

    return {};
}

std::string jsonPath(const std::string &text, const std::initializer_list<const char *> keys) {
    const Json::Doc doc = Json::read_data(text);

    if (!doc.valid()) {
        return {};
    }

    const yyjson_val *at = doc.root();

    for (const char *key : keys) {
        at = Json::obj_get(at, key);
    }

    return at == nullptr ? std::string() : Json::as_string(at);
}

// The IWAD a save names as its game, the way the port reads it back.
std::string savedGame(const Kind kind, const std::filesystem::path &save) {
    switch (kind) {
        case Kind::uzdoom:
        case Kind::gzdoom:
        case Kind::vkdoom:
            return jsonPath(zipEntry(save, "info.json"), {"Game WAD"});
        case Kind::zdoom:
        case Kind::zandronum:
            return pngText(save, "Game WAD");
        case Kind::helion:
            return jsonPath(zipEntry(save, "save.json"), {"Files", "IWad", "FileName"});
        default:
            return {};
    }
}

// What -loadgame can reach: the numbered saves of a port that loads by slot.
bool slotted(const std::string &name, const std::string_view prefix) {
    const std::string stem = std::filesystem::path(name).stem().string();

    return Text::iends_with(name, ".dsg") && stem.size() > prefix.size()
           && Text::iequals(std::string_view(stem).substr(0, prefix.size()), prefix)
           && std::ranges::all_of(std::string_view(stem).substr(prefix.size()),
                                  [](const char each) { return std::isdigit(static_cast<unsigned char>(each)) != 0; });
}

std::string_view slotPrefix(const Kind kind) {
    switch (kind) {
        case Kind::prboom:
            return "prboom-plus-savegame";
        case Kind::woof:
            return "woofsav";
        case Kind::nugget:
            return "nuggsav";
        case Kind::dsda:
            return "dsda-doom-savegame";
        case Kind::nyan:
            return "nyan-doom-savegame";
        default:
            return "doomsav";
    }
}

using Found = std::vector<std::pair<std::filesystem::file_time_type, std::string>>;

// Each save in folder, named relative to root.
template <typename Keep>
void collect(Found &found, const std::filesystem::path &root, const std::filesystem::path &folder, const Keep &keep) {
    std::error_code code;

    for (std::filesystem::directory_iterator walk(folder, code), end; walk != end && !code; walk.increment(code)) {
        std::error_code asked;

        if (walk->is_regular_file(asked) && keep(walk->path())) {
            found.emplace_back(walk->last_write_time(asked), walk->path().lexically_relative(root).generic_string());
        }
    }
}

std::vector<std::string> newestFirst(Found found) {
    std::ranges::sort(found, [](const auto &left, const auto &right) {
        return left.first != right.first ? left.first > right.first : Text::natural_less(left.second, right.second);
    });

    std::vector<std::string> names;

    names.reserve(found.size());

    for (auto &name : found | std::views::values) {
        names.push_back(std::move(name));
    }

    return names;
}

}

std::filesystem::path folder(const Config &config) {
    const Kind kind = kindOf(config);
    const PortEntry *entry = config.findPort(config.activeProfile().port);

    if (kind == Kind::none || entry == nullptr) {
        return {};
    }

    const std::filesystem::path program(entry->file);

    switch (kind) {
        case Kind::uzdoom:
        case Kind::gzdoom:
        case Kind::vkdoom:
        case Kind::zdoom:
        case Kind::zandronum: {
            const std::filesystem::path moved = saveDir(zdoomIni(kind, program), program);

            return moved.empty() ? zdoomRoot(kind, program) : moved;
        }
        case Kind::helion:
            return helionFolder(program);
        case Kind::prboom: {
            const std::string moved = Env::get("DOOMSAVEDIR");

            return moved.empty() ? boomPrefs(kind, program) : std::filesystem::path(moved);
        }
        case Kind::woof:
        case Kind::nugget:
            return woofFolder(config, kind, program);
        case Kind::dsda:
        case Kind::nyan:
            return dsdaFolder(config, kind, program);
        case Kind::chocolate:
        case Kind::crispy:
            return chocolateFolder(config, kind, program);
        case Kind::none:
            break;
    }

    return {};
}

std::vector<std::string> saves(const Config &config) {
    const Kind kind = kindOf(config);
    const std::filesystem::path root = folder(config);

    if (root.empty()) {
        return {};
    }

    Found found;

    switch (kind) {
        case Kind::uzdoom:
        case Kind::gzdoom:
        case Kind::vkdoom:
        case Kind::zdoom:
        case Kind::zandronum:
        case Kind::helion: {
            const std::string game = std::filesystem::path(config.activeIwadFile()).filename().string();

            if (game.empty()) {
                return {};
            }

            const std::string_view extension = kind == Kind::helion ? ".hsg" : ".zds";
            const auto keep = [kind, &game, extension](const std::filesystem::path &save) {
                return Text::iends_with(save.filename().string(), extension)
                       && Text::iequals(savedGame(kind, save), game);
            };

            collect(found, root, root, keep);

            // One folder per game below the root. Netgame saves load from elsewhere.
            if (sortsByGame(kind)) {
                std::error_code code;

                for (std::filesystem::directory_iterator walk(root, code), end; walk != end && !code;
                     walk.increment(code)) {
                    std::error_code asked;

                    if (walk->is_directory(asked) && !Text::iequals(walk->path().filename().string(), "netgame")) {
                        collect(found, root, walk->path(), keep);
                    }
                }
            }

            break;
        }
        case Kind::prboom:
        case Kind::woof:
        case Kind::nugget:
        case Kind::dsda:
        case Kind::nyan:
        case Kind::chocolate:
        case Kind::crispy: {
            const std::string_view prefix = slotPrefix(kind);

            collect(found, root, root, [prefix](const std::filesystem::path &save) {
                return slotted(save.filename().string(), prefix);
            });

            break;
        }
        case Kind::none:
            break;
    }

    return newestFirst(std::move(found));
}

std::string loadName(const Config &config, const std::filesystem::path &save) {
    const Kind kind = kindOf(config);

    if (!sortsByGame(kind)) {
        return {};
    }

    const std::filesystem::path relative = save.lexically_relative(folder(config));

    if (relative.empty() || !relative.has_parent_path() || *relative.begin() == "..") {
        return {};
    }

    return "../" + relative.generic_string();
}

std::filesystem::path uzdoomNetgameFolder(const std::filesystem::path &program) {
#ifdef _WIN32
    return zdoomRoot(Kind::uzdoom, program) / "NetGame";
#else
    return zdoomRoot(Kind::uzdoom, program) / "netgame";
#endif
}

}
