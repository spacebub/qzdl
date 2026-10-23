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
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

#include <sys/stat.h>

#include "ttk/util/Format.h"

#include "World.h"
#include "core/config/Schema.h"
#include "support/Corpus.h"
#include "support/Fixtures.h"
#include "support/Sandbox.h"

namespace fs = std::filesystem;

namespace {

struct Game {
    std::string_view name;
    std::string_view file;
};

// Index 1 is the one game kept as a PK3.
constexpr std::array GAMES = std::to_array<Game>({
        {"Doom", "doom.wad"},
        {"Sigil", "sigil.pk3"},
        {"Doom II", "doom2.wad"},
        {"TNT: Evilution", "tnt.wad"},
        {"The Plutonia Experiment", "plutonia.wad"},
        {"Heretic", "heretic.wad"},
        {"Hexen", "hexen.wad"},
        {"Strife", "strife1.wad"},
        {"Freedoom: Phase 1", "freedoom1.wad"},
        {"Freedoom: Phase 2", "freedoom2.wad"},
        {"Chex Quest", "chex.wad"},
        {"HacX", "hacx.wad"},
});

struct Port {
    std::string_view name;
    std::string_view directory;
    std::string_view program;
    bool dos;
};

constexpr std::array PORTS = std::to_array<Port>({
        {"GZDoom", "gzdoom", "gzdoom", false},
        {"PrBoom+", "prboom", "prboom-plus", false},
        {"DSDA-Doom", "dsda", "dsda-doom", false},
        {"Chocolate Doom", "chocolate", "chocolate-doom", false},
        {"Woof", "woof", "woof", false},
        {"Crispy Doom", "crispy", "crispy-doom", false},
        {"Zandronum", "zandronum", "zandronum", false},
        {"Doom II (DOS)", "dos", "DOOM2.EXE", true},
});

void write(const fs::path &path, const std::string &bytes) {
    fs::create_directories(path.parent_path());

    std::ofstream out(path, std::ios::binary | std::ios::trunc);

    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

std::string slurp(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);

    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void script(const fs::path &path, const std::string_view body) {
    write(path, "#!/bin/sh\n" + std::string(body));
    fs::permissions(path, fs::perms::owner_all);
}

// What a port prints while it starts: coloured lines, a status bar redrawn at the
// bottom row and a progress line rewritten in place, as the ANSI filter sees them.
constexpr std::string_view chatter() {
    return R"(printf 'Startup: %s\n' "$0"
for each in "$@"; do printf 'arg %s\n' "$each"; done
i=0
while [ $i -lt 48 ]; do
    printf '\0337\033[24;0H\033[2K[====....]\0338\033[38;2;223;223;223mInit: subsystem %d ready\033[0m\r\n' $i
    i=$((i+1))
done
printf 'loading 10%%\rloading 100%%\n'
exit 0
)";
}

}

namespace pgo {

World::World() : _addons(tests::Corpus::addons(16)) {
    const fs::path &root = tests::Sandbox::root();

    _gamesDirectory = root / "games";

    // Copies rather than links, so each game is a file of its own to the readers and
    // the art cache.
    const std::string wad = slurp(tests::Corpus::iwad());
    const std::string pk3 = slurp(tests::Corpus::pk3());

    for (const Game &game : GAMES) {
        const fs::path file = _gamesDirectory / game.file;

        write(file, game.file.ends_with(".pk3") ? pk3 : wad);

        _games.push_back({.name = std::string(game.name), .file = ttk::Format::from_path(file)});
    }

    for (const Port &port : PORTS) {
        const fs::path file = root / "ports" / port.directory / port.program;

        if (port.dos) {
            write(file, "MZ");
        } else if (port.name == PORTS[WAITING].name) {
            const fs::path hold = file.parent_path() / "hold";

            fs::create_directories(hold.parent_path());
            ::mkfifo(hold.c_str(), 0600);

            script(file, "read x < '" + hold.string() + "'\n");
        } else {
            script(file, chatter());
        }

        _ports.push_back({
                .name = std::string(port.name),
                .file = ttk::Format::from_path(file),
                .dosbox = port.dos,
                .portId = {},
        });
    }

    // On the sandbox PATH: what a DOS launch runs in, and a port for detection to find.
    script(root / "bin" / "dosbox", chatter());
    script(root / "bin" / "woof", chatter());
}

Config World::config(const int profiles, const int filesPerProfile) const {
    Config made;

    made.general.showPaths = true;
    made.general.profileConfigs = true;
    made.general.theme = ThemeMode::DARK;
    made.general.startView = StartView::Profiles;

    if (profiles == 0) {
        return made;
    }

    made.iwads = _games;
    made.ports = _ports;
    made.general.gamePort = _ports.front().name;

    for (int at = 0; at < profiles; ++at) {
        const auto seed = static_cast<unsigned>(at);
        Profile profile;

        profile.id = "pgo-" + std::to_string(at);
        profile.name = tests::Fixtures::words(2 + (at % 2), 11U + seed);
        profile.iwad = _games[static_cast<size_t>(at) % _games.size()].name;
        profile.port = _ports[static_cast<size_t>(at) % _ports.size()].name;
        profile.skill = 1 + (at % 5);
        profile.monsters = at % 3;
        profile.warp = at % 4 == 0 ? "MAP" + std::string(at % 32 < 9 ? "0" : "") + std::to_string(1 + (at % 32)) : "";
        profile.extra = at % 5 == 0 ? "-nomonsters" : "";
        profile.config = "profile" + std::to_string(at) + ".ini";
        profile.captureOutput = at % 3 == 0;

        for (int file = 0; file < filesPerProfile && std::cmp_less(file, _addons.size()); ++file) {
            profile.files.push_back({.file = _addons[static_cast<size_t>(file)], .enabled = file % 12 != 0});
        }

        if (at == 3) {
            profile.replay.mode = ReplayMode::Record;
            profile.replay.file = "first-run";
        }

        if (at == 4) {
            profile.multiplayer.gameType = GameType::Coop;
            profile.multiplayer.players = 2;
            profile.multiplayer.host = "10.0.0.2";
            profile.multiplayer.port = "5029";
        }

        made.profiles.push_back(std::move(profile));
    }

    made.activeProfileId = made.profiles.front().id;

    return made;
}

}
