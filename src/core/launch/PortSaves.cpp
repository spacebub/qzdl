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
#include <initializer_list>
#include <map>
#include <ranges>
#include <span>
#include <spanstream>
#include <string_view>
#include <system_error>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <lmcons.h>
#include <shlobj.h>
#endif

#include "ttk/system/Env.h"
#include "ttk/system/Json.h"
#include "ttk/system/Paths.h"
#include "ttk/system/Text.h"

#include "core/launch/Arguments.h"
#include "core/launch/Dialect.h"
#include "core/launch/DosFiles.h"
#include "core/launch/KnownPorts.h"
#include "core/launch/PortSaves.h"
#include "core/launch/Storage.h"
#include "core/ports/Flatpak.h"
#include "core/util/FileCache.h"
#include "core/util/Ini.h"
#include "core/wad/MapFile.h"
#include "external/miniz/miniz.h"

using namespace ttk;

namespace PortSaves {

namespace {

const Engine *launched(const Config &config) {
    return config.findEngine(config.activeProfile().engine);
}

// The port whose saves the launch lists. Null for none.
const Port::Known *listed(const Engine *engine, const Dialect::Traits &traits) {
    const Port::Known &known = Port::known(traits.port);

    if (engine == nullptr) {
        return nullptr;
    }

    return (engine->dosbox ? !known.dos : known.dataName.empty()) ? nullptr : &known;
}

#ifndef _WIN32
// An XDG base directory as the port sees it. A Flatpak points them into its own folder.
std::filesystem::path xdg(const std::filesystem::path &file, const char *variable, const char *fallback) {
    if (const std::filesystem::path inside = Flatpak::home(file); !inside.empty()) {
        return inside / (std::string_view(variable) == "XDG_DATA_HOME" ? "data" : "config");
    }

    if (std::string set = Env::get(variable); !set.empty()) {
        return set;
    }

    return Paths::home_directory() / fallback;
}

// A Flatpak keeps the folders it persists in its sandbox.
std::filesystem::path homeOf(const std::filesystem::path &file) {
    const std::filesystem::path inside = Flatpak::home(file);

    return inside.empty() ? Paths::home_directory() : inside;
}

std::filesystem::path dataHome(const std::filesystem::path &file) {
    return xdg(file, "XDG_DATA_HOME", ".local/share");
}

std::filesystem::path configHome(const std::filesystem::path &file) {
    return xdg(file, "XDG_CONFIG_HOME", ".config");
}
#else
bool underProgramFiles(const std::filesystem::path &directory) {
    return std::ranges::any_of(std::array{"ProgramFiles", "ProgramFiles(x86)"},
                               [&directory](const char *variable) { return underRoot(directory, Env::get(variable)); });
}

// As GetUserNameW gives it, with a slash either way made an underscore. Empty when it gives none.
std::wstring userName() {
    std::array<wchar_t, UNLEN + 1> name{};
    auto length = static_cast<DWORD>(name.size());

    if (GetUserNameW(name.data(), &length) == 0) {
        return {};
    }

    std::wstring out(name.data());

    std::ranges::replace(out, L'\\', L'_');
    std::ranges::replace(out, L'/', L'_');

    return out;
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

std::filesystem::path appData() {
    static const std::filesystem::path folder = knownFolder(FOLDERID_RoamingAppData, "AppData/Roaming");

    return folder;
}
#endif

bool isFile(const std::filesystem::path &path) {
    std::error_code code;

    return std::filesystem::is_regular_file(path, code);
}

// What GZDoom and its forks do to a save_dir: a leading ~, $PROGDIR and other variables are expanded.
std::filesystem::path expanded(const std::string &value, const std::filesystem::path &file) {
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

            out += Text::iequals(name, "PROGDIR") ? file.parent_path().string() : Env::get(name.c_str());
        } else {
            out += value.substr(at, end - at);
        }

        at = end;
    }

    return out;
}

// save_dir from the [GlobalSettings] of a ZDoom ini. Empty when it is not set.
std::filesystem::path saveDir(const std::filesystem::path &ini, const std::filesystem::path &file) {
    static FileCache<std::string, std::filesystem::path> seen;

    if (ini.empty()) {
        return {};
    }

    const std::string &value = seen.get(ini, [&ini] {
        Ini read;

        if (!read.read(ini)) {
            return std::string();
        }

        const Ini::Section *global = read.section("GlobalSettings");

        return global == nullptr ? std::string() : Text::trim(global->get("save_dir"));
    });

    return value.empty() ? std::filesystem::path() : expanded(value, file);
}

// The value of a key in a Boom style cfg, where each line is a name and a value. Quotes are dropped.
std::string cfgValue(const std::filesystem::path &cfg, const std::string_view key) {
    // One cache for each key asked, so a whole cfg is never kept.
    static std::map<std::string, FileCache<std::string, std::filesystem::path>, std::less<>> byKey;

    return byKey.try_emplace(std::string(key)).first->second.get(cfg, [&cfg, key] {
        std::ifstream in(cfg);

        for (std::string line; std::getline(in, line);) {
            const std::string trimmed = Text::trim(line);
            const size_t gap = trimmed.find_first_of(" \t");

            if (gap == std::string::npos || !Text::iequals(std::string_view(trimmed).substr(0, gap), key)) {
                continue;
            }

            std::string value = Text::trim(trimmed.substr(gap));

            return value.size() >= 2 && value.front() == '"' && value.back() == '"' ? value.substr(1, value.size() - 2)
                                                                                    : value;
        }

        return std::string();
    });
}

#ifdef _WIN32
std::filesystem::path portableFile(const Port::Known &known, const std::filesystem::path &file) {
    return file.parent_path() / (Text::lower(known.dataName) + "_portable.ini");
}
#else
// UZDoom keeps to the XDG folders, the others to ~/.config.
bool onXdg(const Port::Known &known) {
    return known.port == Port::Port::uzdoom;
}
#endif

// The default save root of the ZDoom ports, before save_dir and the folder per game.
std::filesystem::path
zdoomRoot(const Dialect::Traits &traits, const Port::Known &known, const std::filesystem::path &file) {
#ifdef _WIN32
    const std::filesystem::path beside = file.parent_path();

    switch (windowsRoot(traits.features, underProgramFiles(beside), isFile(portableFile(known, file)))) {
        case WindowsRoot::savedGames:
            return savedGames() / known.dataName;
        case WindowsRoot::saveBeside:
            return beside / "Save";
        case WindowsRoot::beside:
            break;
    }

    return beside;
#else
    const std::string name = Text::lower(known.dataName);

    if (traits.has(Dialect::Feature::homeDotFolder)) {
        return homeOf(file) / ("." + name);
    }

    const std::filesystem::path own = onXdg(known) ? dataHome(file) / "games" / name : homeOf(file) / ".config" / name;

    return traits.has(Dialect::Feature::savegamesSubfolder) ? own / "savegames" : own;
#endif
}

// Where a port that saves where it runs keeps them, before save_dir.
std::filesystem::path runFolder(const Config &config, const Port::Known &known, const std::filesystem::path &file) {
    const std::filesystem::path run = Storage::plannedRunDirectory(config, file.parent_path());
#ifdef _WIN32
    (void)known;

    return run;
#else
    return run / ("~." + Text::lower(known.dataName));
#endif
}

std::filesystem::path
zdoomIni(const Dialect::Traits &traits, const Port::Known &known, const std::filesystem::path &file) {
    const std::string name = Text::lower(known.dataName);
#ifdef _WIN32
    const std::filesystem::path beside = file.parent_path();
    const auto pathOf = [&](const WindowsIni ini) -> std::filesystem::path {
        switch (ini) {
            case WindowsIni::portable:
                return portableFile(known, file);
            case WindowsIni::myGames:
                return documents() / "My Games" / known.dataName / (name + ".ini");
            case WindowsIni::appData:
                return appData() / known.dataName / (name + ".ini");
            case WindowsIni::besideShared:
                return beside / (name + ".ini");
            case WindowsIni::besideForUser:
                break;
        }

        // Without a user name the port skips this ini.
        const std::wstring user = userName();

        return user.empty() ? std::filesystem::path()
                            : beside / (std::filesystem::path(name).wstring() + L"-" + user + L".ini");
    };

    return pathOf(windowsIni(traits.features, underProgramFiles(beside), [&pathOf](const WindowsIni ini) {
        return isFile(pathOf(ini));
    }));
#else
    const std::filesystem::path own = traits.has(Dialect::Feature::homeDotFolder)
                                              ? homeOf(file) / ("." + name)
                                              : (onXdg(known) ? configHome(file) : homeOf(file) / ".config") / name;

    return own / (name + ".ini");
#endif
}

std::filesystem::path
helionFolder(const Dialect::Traits &traits, const Port::Known &known, const std::filesystem::path &file) {
    if (const std::filesystem::path beside = file.parent_path();
        traits.has(Dialect::Feature::savesBesideProgram) || isFile(beside / "config.ini")) {
        return beside;
    }

#ifdef _WIN32
    return savedGames() / known.dataName;
#else
    const std::filesystem::path home = configHome(file);
    const std::filesystem::path lower = home / Text::lower(known.dataName);
    std::error_code code;

    return std::filesystem::is_directory(lower, code) ? lower : home / known.dataName;
#endif
}

std::filesystem::path boomPrefs(const Port::Known &known, const std::filesystem::path &file) {
#ifdef _WIN32
    (void)known;

    return file.parent_path();
#else
    return dataHome(file) / known.dataName;
#endif
}

// PrBoom+ and DSDA-Doom keep to a dot folder in the home while one is there. With homeDotFolder they make it.
std::filesystem::path
dotFolderOrPrefs(const Dialect::Traits &traits, const Port::Known &known, const std::filesystem::path &file) {
#ifdef _WIN32
    (void)traits;
#else
    std::filesystem::path dot = homeOf(file) / ("." + std::string(known.dataName));

    if (std::error_code code; traits.has(Dialect::Feature::homeDotFolder) || std::filesystem::is_directory(dot, code)) {
        return dot;
    }
#endif

    return boomPrefs(known, file);
}

bool holdsDsg(const std::filesystem::path &folder) {
    static FileCache<bool, std::filesystem::path> seen;

    return seen.get(folder, [&folder] {
        std::error_code code;

        for (std::filesystem::directory_iterator walk(folder, code), end; walk != end && !code; walk.increment(code)) {
            if (Text::iends_with(walk->path().filename().string(), ".dsg")) {
                return true;
            }
        }

        return false;
    });
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

// Calls each with every lump name of a WAD in exact case. The read stops at the end of the file, whatever count the
// header gives. False for a file that is no WAD.
template <typename Each>
bool eachLump(std::istream &in, const Each &each) {
    std::array<char, 12> head{};

    if (!in.read(head.data(), head.size())) {
        return false;
    }

    if (const std::string_view kind(head.data(), 4); kind != "IWAD" && kind != "PWAD") {
        return false;
    }

    const auto word = [&head](const size_t at) {
        std::uint32_t out = 0;

        for (size_t byte = 4; byte > 0; byte--) {
            out = (out << 8U) | static_cast<unsigned char>(head[at + byte - 1]);
        }

        return out;
    };

    std::array<char, 16> entry{};

    in.seekg(static_cast<std::streamoff>(word(8)));

    for (std::uint32_t left = word(4); left > 0 && in.read(entry.data(), entry.size()); --left) {
        const std::string_view name(entry.data() + 8, 8);

        each(name.substr(0, name.find('\0')));
    }

    return true;
}

struct Loaded {
    std::string name;
    std::vector<std::string> lumps;
};

// The .wad files Woof loads from the add-ons, in order. A zip or pk3 adds each .wad at its root.
std::vector<Loaded> loadedWads(const Config &config) {
    std::vector<Loaded> out;

    for (const FileEntry &entry : config.activeProfile().files) {
        if (!entry.enabled || entry.gone) {
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
                std::ispanstream bytes(std::span(static_cast<char *>(data), size));
                Loaded wad{.name = std::string(inner), .lumps = {}};

                eachLump(bytes, [&wad](const std::string_view name) { wad.lumps.push_back(Text::upper(name)); });
                out.push_back(std::move(wad));
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

std::filesystem::path woofFolder(const Config &config,
                                 const Dialect::Traits &traits,
                                 const Port::Known &known,
                                 const std::filesystem::path &file) {
    const std::filesystem::path prefs = boomPrefs(known, file);
    const std::filesystem::path cfg = prefs / (std::string(known.dataName) + ".cfg");
    std::filesystem::path base = prefs;
    std::filesystem::path sorted = prefs / "savegames";

    if (traits.has(Dialect::Feature::savegameDir)) {
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

// The folder name DSDA-Doom makes from a .wad file: lowercase, without the extension.
std::string dsdaStem(const std::string_view file) {
    const std::string name = Text::lower(std::filesystem::path(file).filename().string());

    return name.size() > 4 && name.ends_with(".wad") ? name.substr(0, name.size() - 4) : std::string();
}

// DSDA-Doom unpacks a zip flat, and loads the .wad files in it sorted by name. Valid until the next call.
const std::vector<std::string> &zippedWads(const std::string &archive) {
    static FileCache<std::vector<std::string>> seen;

    return seen.get(archive, [&archive] {
        mz_zip_archive zip{};
        std::vector<std::string> names;

        if (mz_zip_reader_init_file(&zip, archive.c_str(), 0) == 0) {
            return names;
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

        return names;
    });
}

// The folders DSDA-Doom nests under its data root: the game, then add-on WADs in load order, eight at most or seven
// with dataRootTakesSlot. It stops on a PK3, so only .wad files and the .wad files in a .zip count.
std::filesystem::path dsdaDataFolder(const Config &config, const Dialect::Traits &traits) {
    const size_t addOns = traits.has(Dialect::Feature::dataRootTakesSlot) ? 7 : 8;

    std::filesystem::path out = dsdaStem(config.activeIwadFile());
    size_t added = 0;

    const auto add = [&out, &added, addOns](const std::string &stem) {
        if (!stem.empty() && added < addOns) {
            out /= stem;
            added++;
        }
    };

    for (const FileEntry &entry : config.activeProfile().files) {
        if (!entry.enabled || entry.gone) {
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

std::filesystem::path dsdaFolder(const Config &config,
                                 const Dialect::Traits &traits,
                                 const Port::Known &known,
                                 const std::filesystem::path &file) {
    const std::filesystem::path base = dotFolderOrPrefs(traits, known, file);

    // The port reads the config the launch hands it in place of its own. The launch first adopts an older one.
    const bool own = traits.has(Dialect::Feature::configFile) && !Storage::configFile(config).empty();
    const std::filesystem::path handed =
            own ? Storage::writtenPortConfig(config, config.activeProfile()) : std::filesystem::path();
    const std::filesystem::path cfg = handed.empty() ? base / (std::string(known.dataName) + ".cfg") : handed;

    if (!organizesSaves(cfg)) {
        const std::string moved = Env::get("DOOMSAVEDIR");

        return moved.empty() ? base : std::filesystem::path(moved);
    }

    if (config.activeIwadFile().empty()) {
        return {};
    }

    const std::string moved = Env::get("DOOMDATADIR");

    // Named for the port, such as dsda_doom_data.
    std::string data = std::string(known.dataName) + "_data";
    std::ranges::replace(data, '-', '_');

    return (moved.empty() ? base : std::filesystem::path(moved)) / data / dsdaDataFolder(config, traits);
}

#ifndef _WIN32
// What Chocolate and Crispy read from the lumps of the IWAD. FREEDOOM and FREEDM mark the Freedoom games. The first
// MAP01 or E1M1 picks the mission when the port does not know the file name.
struct IwadMarks {
    bool freedoom{false};
    bool freedm{false};
    std::string_view firstMission;

    static IwadMarks of(const std::filesystem::path &iwad) {
        static FileCache<IwadMarks, std::filesystem::path> seen;

        return seen.get(iwad, [&iwad] {
            const std::unique_ptr<MapFile> opened = MapFile::open(iwad);
            const std::vector<std::string> names = opened == nullptr ? std::vector<std::string>() : opened->lumpNames();
            const auto first = std::ranges::find_if(
                    names, [](const std::string &name) { return name == "MAP01" || name == "E1M1"; });

            return IwadMarks{
                    .freedoom = std::ranges::contains(names, "FREEDOOM"),
                    .freedm = std::ranges::contains(names, "FREEDM"),
                    .firstMission =
                            first == names.end() ? std::string_view() : (*first == "MAP01" ? "doom2.wad" : "doom.wad"),
            };
        });
    }
};

// The mission of each IWAD name the ports know. The variant and the game version can still change the folder.
std::string_view namedMission(const std::filesystem::path &iwad) {
    struct Named {
        std::string_view file;
        std::string_view mission;
    };

    static constexpr std::array NAMED = {
            Named{.file = "doom.wad", .mission = "doom.wad"},
            Named{.file = "doom1.wad", .mission = "doom.wad"},
            Named{.file = "doom2.wad", .mission = "doom2.wad"},
            Named{.file = "doom2f.wad", .mission = "doom2.wad"},
            Named{.file = "plutonia.wad", .mission = "plutonia.wad"},
            Named{.file = "tnt.wad", .mission = "tnt.wad"},
            Named{.file = "chex.wad", .mission = "chex.wad"},
            Named{.file = "hacx.wad", .mission = "hacx.wad"},
            Named{.file = "freedoom1.wad", .mission = "doom.wad"},
            Named{.file = "freedoom2.wad", .mission = "doom2.wad"},
            Named{.file = "freedm.wad", .mission = "doom2.wad"},
            Named{.file = "rekkrsa.wad", .mission = "doom.wad"},
            Named{.file = "rekkrsl.wad", .mission = "doom.wad"},
    };

    const std::string name = Text::lower(iwad.filename().string());
    // NOLINTNEXTLINE(readability-qualified-auto): the MSVC array iterator is a class, not a pointer.
    const auto found = std::ranges::find(NAMED, std::string_view(name), &Named::file);

    return found == NAMED.end() ? std::string_view() : found->mission;
}

bool onLine(const Config &config, const std::string_view word) {
    for (const std::string *line : {&config.general.alwaysAdd, &config.activeProfile().extra}) {
        if (std::ranges::any_of(Text::parse_arguments(*line),
                                [word](const std::string &each) { return Text::iequals(each, word); })) {
            return true;
        }
    }

    return false;
}

// Playing a demo back stops the PWAD expansions. Recording one stops them only where the release says so.
bool demoStopsExpansions(const Config &config, const Dialect::Traits &traits) {
    // The profile demo takes the place of any demo among the add-ons.
    if (Arguments::replayOf(config, traits).leftOut == Arguments::ReplayLeftOut::none) {
        return config.activeProfile().replay.mode != ReplayMode::Record
               || traits.has(Dialect::Feature::recordingStopsExpansions);
    }

    return traits.has(Dialect::Feature::playdemo)
           && std::ranges::any_of(config.activeProfile().files, [](const FileEntry &entry) {
                  return entry.enabled && !entry.gone && Text::iends_with(entry.file, ".lmp");
              });
}

// Whether the path ends in a file of this name, in any case.
bool fileNamed(const std::string_view path, const std::string_view name) {
    if (!Text::iends_with(path, name)) {
        return false;
    }

    const std::size_t at = path.size() - name.size();

    return at == 0 || path[at - 1] == '/';
}

// Crispy names the WAD that gives the last lump of a name. Only a PWAD called NERVE.WAD or MASTERLEVELS.WAD can win,
// so the maps are read only when one is enabled.
bool playsPwadExpansion(const Config &config, const Dialect::Traits &traits) {
    static constexpr std::string_view NERVE = "nerve.wad";
    static constexpr std::string_view MASTER_LEVELS = "masterlevels.wad";

    const std::vector<FileEntry> &files = config.activeProfile().files;
    const auto loaded = [](const FileEntry &entry) {
        return entry.enabled && !entry.gone && Text::iends_with(entry.file, ".wad");
    };

    if (std::ranges::none_of(files, [&loaded](const FileEntry &entry) {
            return loaded(entry) && (fileNamed(entry.file, NERVE) || fileNamed(entry.file, MASTER_LEVELS));
        })) {
        return false;
    }

    if (onLine(config, "-nosideload") || demoStopsExpansions(config, traits)) {
        return false;
    }

    const FileEntry *map01 = nullptr;
    const FileEntry *map09 = nullptr;
    const FileEntry *map21 = nullptr;

    for (const FileEntry &entry : files | std::views::filter(loaded)) {
        const std::vector<std::string> &maps = MapFile::maps(entry.file).names;

        for (const auto &[map, last] :
             {std::pair{"MAP01", &map01}, std::pair{"MAP09", &map09}, std::pair{"MAP21", &map21}}) {
            if (std::ranges::any_of(maps, [map](const std::string &each) { return Text::iequals(each, map); })) {
                *last = &entry;
            }
        }
    }

    const auto from = [](const FileEntry *entry, const std::string_view name) {
        return entry != nullptr && fileNamed(entry->file, name);
    };

    return (from(map01, NERVE) && from(map09, NERVE)) || (from(map01, MASTER_LEVELS) && from(map21, MASTER_LEVELS));
}

// Chocolate and Crispy name the folder after the mission, never after the file that was loaded.
std::string missionWad(const Config &config, const Dialect::Traits &traits, const std::filesystem::path &iwad) {
    const std::string_view named = namedMission(iwad);
    const bool freedoomApart = traits.has(Dialect::Feature::freedoomFolders);
    const IwadMarks marks = named.empty() || freedoomApart ? IwadMarks::of(iwad) : IwadMarks{};
    std::string_view mission = named.empty() ? marks.firstMission : named;

    if (mission.empty()) {
        return "unknown.wad";
    }

    // A game version older than Final Doom plays the Final Doom IWADs as Doom II.
    if ((mission == "plutonia.wad" || mission == "tnt.wad")
        && Dialect::beforeFinalDoom(Dialect::pickedLevel(traits.complevel, config.activeProfile().complevel))) {
        mission = "doom2.wad";
    }

    if (mission == "doom2.wad" && traits.has(Dialect::Feature::pwadExpansions) && playsPwadExpansion(config, traits)) {
        return "unknown.wad";
    }

    if (freedoomApart && marks.freedoom) {
        if (marks.freedm) {
            return mission == "doom2.wad" ? "freedm.wad" : std::string(mission);
        }

        if (mission == "doom.wad" || mission == "doom2.wad") {
            return mission == "doom.wad" ? "freedoom1.wad" : "freedoom2.wad";
        }
    }

    return std::string(mission);
}
#endif

std::filesystem::path chocolateFolder(const Config &config,
                                      const Dialect::Traits &traits,
                                      const Port::Known &known,
                                      const std::filesystem::path &file) {
#ifdef _WIN32
    (void)known;
    (void)file;

    // Saves land where the port runs, and a profile runs in its own folder.
    const std::filesystem::path own = Storage::profileDirectory(config.activeProfile());
    const Engine *engine = config.findEngine(config.activeProfile().engine);
    const std::filesystem::path run =
            own.empty() && engine != nullptr ? std::filesystem::path(engine->file).parent_path() : own;

    // Hexen keeps hexndata there, as the original does. The savedir key of hexen.cfg can name another folder.
    return run.empty() || traits.saveLayout != Dialect::SaveLayout::hub ? run : run / "hexndata";
#else
    const std::filesystem::path root = traits.has(Dialect::Feature::homeDotFolder)
                                               ? homeOf(file) / ("." + std::string(known.dataName))
                                               : dataHome(file) / known.dataName;

    if (!known.savesFolder.empty()) {
        return root / "savegames" / known.savesFolder;
    }

    const std::string iwad = config.activeIwadFile();

    return iwad.empty() ? std::filesystem::path() : root / "savegames" / missionWad(config, traits, iwad);
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

enum class Holds : std::uint8_t {
    noFolder,
    fewer,
    asMany,
};

// Eternity counts the entries in the folder that match one of the names in any case. The folder passes when the
// count reaches the number of names.
Holds holdsAsMany(FileCache<Holds, std::filesystem::path> &seen,
                  const std::filesystem::path &folder,
                  const std::span<const std::string_view> names) {
    return seen.get(folder, [&folder, names] {
        std::error_code code;
        std::filesystem::directory_iterator walk(folder, code);

        if (code == std::errc::no_such_file_or_directory) {
            return Holds::noFolder;
        }

        std::size_t found = 0;

        for (const std::filesystem::directory_iterator end; walk != end && !code; walk.increment(code)) {
            const std::string name = Text::lower(walk->path().filename().string());

            found += std::ranges::count(names, std::string_view(name));
        }

        return found >= names.size() ? Holds::asMany : Holds::fewer;
    });
}

constexpr std::array<std::string_view, 3> ETERNITY_BASE_ENTRIES = {"startup.wad", "root.edf", "doom"};
constexpr std::array<std::string_view, 2> ETERNITY_USER_ENTRIES = {"doom", "shots"};

bool isEternityBase(const std::filesystem::path &folder) {
    static FileCache<Holds, std::filesystem::path> seen;

    return holdsAsMany(seen, folder, ETERNITY_BASE_ENTRIES) == Holds::asMany;
}

Holds eternityUserHolds(const std::filesystem::path &folder) {
    static FileCache<Holds, std::filesystem::path> seen;

    return holdsAsMany(seen, folder, ETERNITY_USER_ENTRIES);
}

bool isEternityUser(const std::filesystem::path &folder) {
    return eternityUserHolds(folder) == Holds::asMany;
}

// The base folder an install of Eternity reads. Empty where the release looks in none.
std::filesystem::path eternityInstallBase(const Dialect::Traits &traits, const std::filesystem::path &file) {
#ifdef __linux__
    if (!traits.has(Dialect::Feature::installedBaseFolder)) {
        return {};
    }

    if (Flatpak::exported(file)) {
        const std::filesystem::path commit = Flatpak::deploy(file).folder;

        return commit.empty() ? commit : commit / "files" / "share" / "eternity" / "base";
    }

    // The port takes the /usr/local folder whenever it is there, even when it is no base folder.
    const std::filesystem::path local = "/usr/local/share/eternity/base";
    std::error_code code;

    return std::filesystem::is_directory(local, code) ? local : std::filesystem::path("/usr/share/eternity/base");
#else
    (void)traits;
    (void)file;

    return {};
#endif
}

// The folder Eternity reads the data it comes with from. Empty where the port would stop.
std::filesystem::path
eternityBaseFolder(const Config &config, const Dialect::Traits &traits, const std::filesystem::path &file) {
    if (std::string set = Env::get("ETERNITYBASE"); !set.empty() && isEternityBase(set)) {
        return set;
    }

    if (std::filesystem::path installed = eternityInstallBase(traits, file);
        !installed.empty() && isEternityBase(installed)) {
        return installed;
    }

    const std::filesystem::path beside = file.parent_path();

    for (const std::filesystem::path &each : {beside, Storage::plannedRunDirectory(config, beside)}) {
        if (isEternityBase(each / "base")) {
            return each / "base";
        }
    }

    return {};
}

// The first user folder Eternity accepts, or else its base folder.
std::filesystem::path
eternityUserFolder(const Config &config, const Dialect::Traits &traits, const std::filesystem::path &file) {
    if (std::string set = Env::get("ETERNITYUSER"); !set.empty() && isEternityUser(set)) {
        return set;
    }

#ifdef __linux__
    // The port makes this folder with the ones it needs, unless a folder is already there.
    if (traits.has(Dialect::Feature::userFolderInConfigHome)) {
        const std::filesystem::path own = configHome(file) / "eternity" / "user";

        if (eternityUserHolds(own) != Holds::fewer) {
            return own;
        }
    }
#endif

    const std::filesystem::path beside = file.parent_path();

    for (const std::filesystem::path &each : {beside, Storage::plannedRunDirectory(config, beside)}) {
        if (isEternityUser(each / "user")) {
            return each / "user";
        }
    }

    std::filesystem::path base = eternityBaseFolder(config, traits, file);

    // Through a symbolic link the parent of the base folder is the parent of the target.
    if (const std::filesystem::path above = base / ".." / "user"; !base.empty() && isEternityUser(above)) {
        return above;
    }

    return base;
}

// What a game archive names in its gameversion.txt, and the folder Eternity saves that game in.
struct ZipGame {
    std::string_view named;
    std::string_view folder;
};

constexpr std::array ZIP_GAMES = {
        ZipGame{.named = "doom retail", .folder = "doom"},
        ZipGame{.named = "doom registered", .folder = "doom"},
        ZipGame{.named = "doom shareware", .folder = "doom"},
        ZipGame{.named = "doom2 commercial", .folder = "doom2"},
        ZipGame{.named = "doom2 tnt", .folder = "tnt"},
        ZipGame{.named = "doom2 plutonia", .folder = "plutonia"},
        ZipGame{.named = "doom2 hacx", .folder = "hacx"},
        ZipGame{.named = "doom2 bfg", .folder = "doom2"},
        ZipGame{.named = "doom2 psx", .folder = "doom2"},
        ZipGame{.named = "heretic sosr", .folder = "heretic"},
        ZipGame{.named = "heretic registered", .folder = "heretic"},
        ZipGame{.named = "heretic shareware", .folder = "heretic"},
        ZipGame{.named = "heretic beta", .folder = "heretic"},
};

// What Eternity counts in the lump names of an IWAD to tell its game. It tests the names in this order and in exact
// case. From ADVISOR on, a name matches by its start.
struct EternityLumps {
    int secondOrThird{0};
    int numbered{0};
    int caverns{0};
    int mcLumps{0};
    int raven{0};
    int bfg{0};
    bool freedoom{false};
    bool hacx{false};

    void add(const std::string_view name) {
        const auto startsWithAny = [name](const std::initializer_list<std::string_view> starts) {
            return std::ranges::any_of(starts,
                                       [name](const std::string_view start) { return name.starts_with(start); });
        };

        if (name.size() >= 3 && name.size() <= 4 && name[0] == 'E' && name[2] == 'M') {
            secondOrThird += name[1] == '2' || name[1] == '3' ? 1 : 0;
        } else if (name.starts_with("MAP") && name.size() <= 5) {
            ++numbered;
        } else if (name.starts_with("CAV") && name.size() <= 7) {
            ++caverns;
        } else if (name.starts_with("MC") && name.size() <= 3) {
            ++mcLumps;
        } else if (startsWithAny({"ADVISOR", "TINTTAB", "SNDCURVE"})) {
            ++raven;
        } else if (name.starts_with("FREEDOOM")) {
            freedoom = true;
        } else if (name.starts_with("HACX-R")) {
            hacx = true;
        } else if (startsWithAny({"M_ACPT", "M_CAN", "M_EXITO", "M_CHG", "DMENUPIC"})) {
            ++bfg;
        }
    }

    [[nodiscard]] std::string_view game() const {
        static constexpr int RAVEN_LUMPS = 3;
        static constexpr int FULL_SET = 30;
        static constexpr int BFG_LUMPS = 5;
        static constexpr int TNT_CAVERNS = 4;
        static constexpr int PLUTONIA_MC_LUMPS = 8;

        if (raven == RAVEN_LUMPS) {
            return "heretic";
        }

        if (numbered < FULL_SET && (numbered == 0 || secondOrThird > 0)) {
            return "doom";
        }

        if (freedoom || bfg >= BFG_LUMPS) {
            return "doom2";
        }

        if (caverns >= TNT_CAVERNS) {
            return "tnt";
        }

        if (mcLumps >= PLUTONIA_MC_LUMPS) {
            return "plutonia";
        }

        return hacx ? "hacx" : "doom2";
    }
};

std::string_view zipGame(const std::filesystem::path &archive) {
    const std::string named = zipEntry(archive, "gameversion.txt");
    // NOLINTNEXTLINE(readability-qualified-auto): the MSVC array iterator is a class, not a pointer.
    const auto game =
            std::ranges::find_if(ZIP_GAMES, [&named](const ZipGame &each) { return Text::iequals(each.named, named); });

    return game == ZIP_GAMES.end() ? "doom" : game->folder;
}

// The game folder Eternity picks from what the IWAD holds, never from its file name. Empty for a file that is neither
// a WAD nor a zip.
std::string_view eternityGame(const std::filesystem::path &iwad) {
    static FileCache<std::string_view, std::filesystem::path> seen;

    return seen.get(iwad, [&iwad] {
        std::ifstream in(iwad, std::ios::binary);

        // A whole WAD header, so a file too short for one gives no folder, zip magic or not.
        std::array<char, 12> head{};

        if (!in.read(head.data(), head.size())) {
            return std::string_view();
        }

        if (std::string_view(head.data(), 4) == std::string_view("PK\x03\x04", 4)) {
            return zipGame(iwad);
        }

        EternityLumps lumps;

        in.seekg(0);

        return eachLump(in, [&lumps](const std::string_view name) { lumps.add(name); }) ? lumps.game()
                                                                                        : std::string_view();
    });
}

std::filesystem::path
eternityFolder(const Config &config, const Dialect::Traits &traits, const std::filesystem::path &file) {
    const std::string iwad = Arguments::beside(config.activeIwadFile(), file.parent_path());
    const std::string_view game = iwad.empty() ? std::string_view() : eternityGame(iwad);

    if (game.empty()) {
        return {};
    }

    const std::filesystem::path root = traits.has(Dialect::Feature::userFolder)
                                               ? eternityUserFolder(config, traits, file)
                                               : eternityBaseFolder(config, traits, file);

    return root.empty() ? root : root / game;
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
std::string savedGame(const Dialect::Dialect dialect, const std::filesystem::path &save) {
    switch (dialect) {
        case Dialect::Dialect::gzdoom:
        case Dialect::Dialect::uzdoom:
            return jsonPath(zipEntry(save, "info.json"), {"Game WAD"});
        case Dialect::Dialect::zdoom:
        case Dialect::Dialect::zandronum:
            return pngText(save, "Game WAD");
        case Dialect::Dialect::helion:
            return jsonPath(zipEntry(save, "save.json"), {"Files", "IWad", "FileName"});
        default:
            return {};
    }
}

// The slot that -loadgame takes to load this save, or -1. The name is the prefix, the slot and the extension.
int slotOf(const std::string &name, const std::string_view prefix, const Dialect::Traits &traits) {
    static constexpr std::size_t HUB_SLOT_DIGITS = 2;
    static constexpr int HUB_GAME_SLOT = 6;
    static constexpr int HUB_REBORN_SLOT = 7;
    static constexpr int FOLDER_GAME_SLOT = 6;

    const std::string stem = std::filesystem::path(name).stem().string();

    if (!Text::iends_with(name, traits.saveExt) || stem.size() <= prefix.size()
        || !Text::iequals(std::string_view(stem).substr(0, prefix.size()), prefix)) {
        return -1;
    }

    const std::string_view digits = std::string_view(stem).substr(prefix.size());

    if (!std::ranges::all_of(digits,
                             [](const char each) { return std::isdigit(static_cast<unsigned char>(each)) != 0; })) {
        return -1;
    }

    const int slot = Storage::saveSlot(name);

    switch (traits.saveLayout) {
        case Dialect::SaveLayout::file:
            break;
        case Dialect::SaveLayout::hub:
            return digits.size() <= HUB_SLOT_DIGITS && slot != HUB_GAME_SLOT && slot != HUB_REBORN_SLOT ? slot : -1;
        case Dialect::SaveLayout::folder:
            return slot != FOLDER_GAME_SLOT ? slot : -1;
    }

    return slot;
}

using Found = std::vector<std::pair<std::filesystem::file_time_type, std::string>>;

// Each save in folder, named relative to root.
template <typename Keep>
void collect(Found &found, const std::filesystem::path &root, const std::filesystem::path &folder, const Keep &keep) {
    std::error_code code;

    for (std::filesystem::directory_iterator walk(folder, code), end; walk != end && !code; walk.increment(code)) {
        std::error_code asked;

        if (keep(*walk)) {
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

bool organizesSaves(const std::filesystem::path &cfg) {
    return cfgValue(cfg, "dsda_organized_saves") != "0";
}

std::filesystem::path folder(const Config &config) {
    const Dialect::Traits traits = Dialect::of(config);
    const Engine *engine = launched(config);
    const Port::Known *known = listed(engine, traits);

    if (known == nullptr) {
        return {};
    }

    const std::filesystem::path file(engine->file);

    switch (traits.dialect) {
        case Dialect::Dialect::gzdoom:
        case Dialect::Dialect::uzdoom:
        case Dialect::Dialect::zdoom:
        case Dialect::Dialect::zandronum: {
            const std::filesystem::path moved = traits.has(Dialect::Feature::saveDirSetting)
                                                        ? saveDir(zdoomIni(traits, *known, file), file)
                                                        : std::filesystem::path();

            const std::filesystem::path root = traits.has(Dialect::Feature::savesWhereRun)
                                                       ? runFolder(config, *known, file)
                                                       : zdoomRoot(traits, *known, file);

            return moved.empty() ? root : moved;
        }
        case Dialect::Dialect::helion:
            return helionFolder(traits, *known, file);
        case Dialect::Dialect::prboom: {
            const std::string moved = Env::get("DOOMSAVEDIR");

            return moved.empty() ? dotFolderOrPrefs(traits, *known, file) : std::filesystem::path(moved);
        }
        case Dialect::Dialect::woof:
            return woofFolder(config, traits, *known, file);
        case Dialect::Dialect::dsda:
            return dsdaFolder(config, traits, *known, file);
        case Dialect::Dialect::chocolate:
            return chocolateFolder(config, traits, *known, file);
        case Dialect::Dialect::eternity:
            return eternityFolder(config, traits, file);
        case Dialect::Dialect::boom:
        case Dialect::Dialect::legacy:
        case Dialect::Dialect::doom: {
            if (!traits.has(Dialect::Feature::recognised)) {
                return {};
            }

            // A DOS port saves where it runs.
            const std::filesystem::path run = DosFiles::runFolder(config, config.activeProfile());

            return run.empty() ? file.parent_path() : run;
        }
    }

    return {};
}

std::vector<std::string> saves(const Config &config) {
    const Dialect::Traits traits = Dialect::of(config);
    const Port::Known *known = listed(launched(config), traits);
    const std::filesystem::path root = folder(config);

    if (known == nullptr || root.empty()) {
        return {};
    }

    Found found;

    switch (traits.dialect) {
        case Dialect::Dialect::gzdoom:
        case Dialect::Dialect::uzdoom:
        case Dialect::Dialect::zdoom:
        case Dialect::Dialect::zandronum:
        case Dialect::Dialect::helion: {
            const std::string game = std::filesystem::path(config.activeIwadFile()).filename().string();

            if (game.empty()) {
                return {};
            }

            const auto keep = [&game, &traits](const std::filesystem::directory_entry &save) {
                std::error_code asked;

                return save.is_regular_file(asked) && Text::iends_with(save.path().filename().string(), traits.saveExt)
                       && Text::iequals(savedGame(traits.dialect, save.path()), game);
            };

            collect(found, root, root, keep);

            if (traits.has(Dialect::Feature::folderPerGame)) {
                std::error_code code;

                for (std::filesystem::directory_iterator walk(root, code), end; walk != end && !code;
                     walk.increment(code)) {
                    std::error_code asked;

                    // Netgame saves load from elsewhere.
                    if (walk->is_directory(asked) && !Text::iequals(walk->path().filename().string(), "netgame")) {
                        collect(found, root, walk->path(), keep);
                    }
                }
            }

            break;
        }
        case Dialect::Dialect::prboom:
        case Dialect::Dialect::woof:
        case Dialect::Dialect::dsda:
        case Dialect::Dialect::chocolate:
        case Dialect::Dialect::eternity:
        case Dialect::Dialect::boom:
        case Dialect::Dialect::legacy:
        case Dialect::Dialect::doom:
            collect(found, root, root, [&config, &traits](const std::filesystem::directory_entry &save) {
                return slotSave(config, traits, save);
            });

            break;
    }

    return newestFirst(std::move(found));
}

int loadSlot(const Config &config, const Dialect::Traits &traits, const std::string &name) {
    if (!traits.loadsBy(Dialect::SaveNames::slot)) {
        return -1;
    }

    if (Dialect::isAutosave(traits, name)) {
        return Dialect::AUTOSAVE_SLOT;
    }

    if (const Port::Known *known = listed(launched(config), traits); known != nullptr && !known->slotPrefix.empty()) {
        return slotOf(name, known->slotPrefix, traits);
    }

    return Storage::saveSlot(name);
}

bool slotSave(const Config &config, const Dialect::Traits &traits, const std::filesystem::directory_entry &entry) {
    const std::string name = entry.path().filename().string();
    std::error_code asked;

    const bool there = traits.saveLayout == Dialect::SaveLayout::folder ? Storage::saveThere(traits, entry.path())
                                                                        : entry.is_regular_file(asked);

    return there && Text::iends_with(name, traits.saveExt) && loadSlot(config, traits, name) >= 0
           && !Storage::pastLastSlot(traits, name);
}

std::string slotLabel(const Config &config, const Dialect::Traits &traits, const std::string &name) {
    if (Dialect::isAutosave(traits, name)) {
        return "Auto Save";
    }

    const int slot = loadSlot(config, traits, name);

    return slot < 0 ? std::string() : "Slot " + std::to_string(slot);
}

std::string loadName(const Config &config, const std::filesystem::path &save) {
    if (const Dialect::Traits traits = Dialect::of(config);
        listed(launched(config), traits) == nullptr || !traits.has(Dialect::Feature::folderPerGame)) {
        return {};
    }

    const std::filesystem::path relative = save.lexically_relative(folder(config));

    if (relative.empty() || !relative.has_parent_path() || *relative.begin() == "..") {
        return {};
    }

    return "../" + relative.generic_string();
}

bool underRoot(const std::filesystem::path &folder, const std::filesystem::path &root) {
    // The port compares its folder with the trailing slash it keeps.
    const std::string at = Text::lower(folder.generic_string()) + "/";
    const std::string start = Text::lower(root.generic_string());

    return !start.empty() && at.starts_with(start);
}

WindowsRoot windowsRoot(const Dialect::Features &features, const bool underProgramFiles, const bool portableIni) {
    if (features.has(Dialect::Feature::programFolderOnly)) {
        return WindowsRoot::beside;
    }

    // A port under Program Files never saves beside itself. Older releases cannot write there, and newer
    // ones do not count a portable ini there.
    if (underProgramFiles || (features.has(Dialect::Feature::portableNeedsIni) && !portableIni)) {
        return WindowsRoot::savedGames;
    }

    return features.has(Dialect::Feature::windowsSaveSubfolder) ? WindowsRoot::saveBeside : WindowsRoot::beside;
}

std::filesystem::path uzdoomNetgameFolder(const Dialect::Traits &traits, const std::filesystem::path &file) {
    const Port::Known &uzdoom = Port::known(Port::Port::uzdoom);
#ifdef _WIN32
    return zdoomRoot(traits, uzdoom, file) / "NetGame";
#else
    return zdoomRoot(traits, uzdoom, file) / "netgame";
#endif
}

}
