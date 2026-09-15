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
#include <utility>

#include "core/config/Schema.h"
#include "core/launch/DosFiles.h"
#include "core/launch/Storage.h"
#include "core/util/Text.h"
#include "core/wad/MapFile.h"

namespace DosFiles {

bool spellable(const std::string &name) {
    static constexpr std::string_view EXTRA = "!#$%&'()-@^_`{}~";
    const size_t dot = name.find('.');
    const std::string stem = name.substr(0, dot);

    if (stem.empty() || stem.size() > 8) {
        return false;
    }

    if (dot != std::string::npos) {
        const std::string extension = name.substr(dot + 1);

        if (extension.size() > 3 || extension.contains('.')) {
            return false;
        }
    }

    return std::ranges::all_of(name, [](const char letter) {
        const bool plain = (letter >= 'a' && letter <= 'z')
            || (letter >= 'A' && letter <= 'Z')
            || (letter >= '0' && letter <= '9');

        return plain || letter == '.' || EXTRA.contains(letter);
    });
}

namespace {

// Names a port searches the wad directory for, so nothing else may answer to one.
constexpr std::array IWAD_NAMES = {
    "doom.wad", "doom1.wad", "doom2.wad", "doom2f.wad", "doomu.wad",
    "plutonia.wad", "tnt.wad",
};

// Told from the map names, whatever the file is called.
std::string dosGameName(const std::string &iwad) {
    const MapFile::Maps &read = MapFile::maps(iwad);

    if (!read.opened) {
        return "doom.wad";
    }

    if (read.mapxx) {
        return "doom2.wad";
    }

    bool second = false;

    for (const std::string &map : read.names) {
        if (Text::iequals(map, "E4M1")) {
            return "doomu.wad";
        }

        second = second || Text::iequals(map, "E2M1");
    }

    return second ? "doom.wad" : "doom1.wad";
}

// Vanilla takes the first name on its list it finds, whichever the profile meant.
bool crowded(const std::filesystem::path &iwad) {
    std::error_code code;

    for (std::filesystem::directory_iterator walk(iwad.parent_path(), code), end;
         walk != end && !code; walk.increment(code)) {
        const std::string name = Text::lower(walk->path().filename().string());

        if (std::ranges::find(IWAD_NAMES, name) != IWAD_NAMES.end()
            && !Text::iequals(name, iwad.filename().string())) {
            return true;
        }
    }

    return false;
}

}

Reach reach(const std::filesystem::path &iwad, const std::filesystem::path &portDirectory,
            const bool recognised) {
    std::error_code code;
    const bool beside = std::filesystem::equivalent(iwad.parent_path(), portDirectory, code);

    // An unknown port looks for whatever it looks for. All that can be done is point it.
    if (!recognised) {
        return beside ? Reach::beside : Reach::pointed;
    }

    const bool crowd = crowded(iwad);

    // An install keeps its game under whatever name its port wants.
    if (beside && !crowd) {
        return Reach::beside;
    }

    const std::string name = Text::lower(iwad.filename().string());
    const std::string wanted = dosGameName(iwad.string());

    // Final Doom's two go by their own names.
    const bool fits = name == wanted
        || (wanted == "doom2.wad" && (name == "tnt.wad" || name == "plutonia.wad"));

    return fits && !crowd ? Reach::pointed : Reach::staged;
}

Staging::Staging(Directories directories) : _where(std::move(directories)) {}

std::filesystem::path Staging::spellableName(const std::filesystem::path &file) {
    return spellable(file.filename().string())
        ? file
        : keep(file, _where.files, shorten(file));
}

void Staging::game(const std::filesystem::path &iwad) {
    keep(iwad, _where.instance, dosGameName(iwad.string()));
}

const std::vector<Copy> &Staging::planned() const {
    return _planned;
}

std::filesystem::path Staging::keep(const std::filesystem::path &file,
                                    const std::filesystem::path &directory,
                                    const std::string &name) {
    std::filesystem::path to = directory / name;

    _planned.push_back({.from = file, .to = to});

    return to;
}

std::string Staging::shorten(const std::filesystem::path &file) {
    static constexpr std::string_view PLAIN =
        "abcdefghijklmnopqrstuvwxyz0123456789!#$%&'()-@^_`{}~";

    const auto squeeze = [](const std::string &from, const size_t room) {
        std::string kept;

        for (const char letter : from) {
            if (kept.size() == room) {
                break;
            }

            if (PLAIN.contains(letter)) {
                kept.push_back(letter);
            }
        }

        return kept;
    };

    const std::string tail = squeeze(Text::lower(file.extension().string()), 3);
    const std::string extension = tail.empty() ? std::string() : "." + tail;
    const std::string stem = squeeze(Text::lower(file.stem().string()), 8);
    const std::string base = stem.empty() ? "zdl" : stem;

    std::string wanted = base;

    for (size_t number = 1; std::ranges::any_of(_planned, [&](const Copy &each) {
             return each.to.parent_path() == _where.files
                 && Text::iequals(each.to.filename().string(), wanted + extension);
         }); number++) {
        const std::string counted = std::to_string(number);

        wanted = base.substr(0, std::min(base.size(), 8 - counted.size())) + counted;
    }

    return wanted + extension;
}

Directories directories(const Config &config, const std::filesystem::path &port) {
    const std::filesystem::path own = Storage::runDirectory(config, port.parent_path());

    return {.instance = own, .files = own / ConfigFile::DOS_FILES_DIR,
            .profileOwned = own != port.parent_path()};
}

namespace {

void sweep(const std::filesystem::path &directory, const std::vector<Copy> &planned,
           const bool wadsOnly) {
    std::error_code code;

    for (std::filesystem::directory_iterator walk(directory, code), end;
         walk != end && !code; walk.increment(code)) {
        const std::string name = walk->path().filename().string();

        if (wadsOnly && !Text::iendsWith(name, ".wad")) {
            continue;
        }

        const bool wanted = std::ranges::any_of(planned, [&](const Copy &each) {
            return each.to.parent_path() == directory
                && Text::iequals(each.to.filename().string(), name);
        });

        if (!wanted) {
            std::error_code asked;
            std::filesystem::remove_all(walk->path(), asked);
        }
    }
}

}

void prune(const Directories &directories, const std::vector<Copy> &planned) {
    // A port pointed at the profile's folder takes the first game it finds there, so a wad
    // this launch did not stage is in the way. Nothing but ZDL writes to the file directory.
    if (directories.profileOwned) {
        sweep(directories.instance, planned, true);
    }

    sweep(directories.files, planned, false);
}

}
