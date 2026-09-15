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
#include <string>

#include "core/config/Session.h"
#include "support/Corpus.h"
#include "support/Fixtures.h"

namespace {

constexpr const char *VOCABULARY[] = {
    "doom", "wad", "sector", "linedef", "sprite", "brutal", "eternal", "hell",
    "knee", "deep", "shores", "inferno", "thy", "flesh", "consumed", "plutonia",
    "evilution", "sigil", "ancient", "aliens", "scythe", "valiant", "going",
    "down", "sunlust", "eviternity", "rekkr", "hedon", "ashes", "afterglow",
};

constexpr size_t VOCABULARY_SIZE = sizeof(VOCABULARY) / sizeof(VOCABULARY[0]);

unsigned next(unsigned &state) {
    state = (state * 1103515245U) + 12345U;

    return (state >> 16U) & 0x7FFFU;
}

}

namespace bench::Fixtures {

std::string words(const int count, const unsigned seed) {
    unsigned state = seed;
    std::string out;

    for (int at = 0; at < count; ++at) {
        if (at > 0) {
            out.push_back(' ');
        }

        out += VOCABULARY[next(state) % VOCABULARY_SIZE];
    }

    return out;
}

std::string paragraph(const int sentences) {
    std::string out;

    for (int at = 0; at < sentences; ++at) {
        out += words(6 + (at % 7), static_cast<unsigned>(at) + 3U);
        out += ". ";
    }

    return out;
}

std::vector<std::string> lines(const int count) {
    std::vector<std::string> out;

    out.reserve(static_cast<size_t>(count));

    for (int at = 0; at < count; ++at) {
        out.push_back(std::to_string(at) + ": " + words(9, static_cast<unsigned>(at) + 1U));
    }

    return out;
}

Config config(const int profiles, const int filesPerProfile) {
    Config made;

    made.general.alwaysAdd = "-nomusic";
    made.general.showPaths = true;
    made.general.profileConfigs = true;
    made.general.startView = StartView::Profiles;

    for (int at = 0; at < 12; ++at) {
        made.iwads.push_back({.name = "Game " + std::to_string(at),
                              .file = "/games/game" + std::to_string(at) + ".wad"});

        made.ports.push_back({.name = "Port " + std::to_string(at),
                              .file = "/ports/gzdoom" + std::to_string(at),
                              .dosbox = at % 5 == 0});
    }

    for (int at = 0; at < profiles; ++at) {
        Profile profile;

        profile.id = "profile-" + std::to_string(at);
        profile.name = words(3, static_cast<unsigned>(at) + 11U);
        profile.iwad = made.iwads[static_cast<size_t>(at) % made.iwads.size()].name;
        profile.port = made.ports[static_cast<size_t>(at) % made.ports.size()].name;
        profile.skill = 1 + (at % 5);
        profile.monsters = at % 3;
        profile.warp = "MAP" + std::to_string(1 + (at % 32));
        profile.extra = "-fast -respawn";
        profile.config = "profile" + std::to_string(at) + ".ini";

        for (int file = 0; file < filesPerProfile; ++file) {
            profile.files.push_back({.file = "/addons/" + words(2, static_cast<unsigned>(file) + 1U)
                                         + std::to_string(file) + ".wad",
                                     .enabled = file % 4 != 0});
        }

        profile.multiplayer.gameType = gameTypeOf(at % 3);
        profile.multiplayer.players = 2 + (at % 6);
        profile.multiplayer.host = "10.0.0." + std::to_string(at % 255);
        profile.multiplayer.port = "5029";

        profile.replay.mode = replayModeOf(at % 3);
        profile.replay.file = "run" + std::to_string(at) + ".lmp";

        made.profiles.push_back(std::move(profile));
    }

    if (!made.profiles.empty()) {
        made.activeProfileId = made.profiles.front().id;
    }

    return made;
}

Config shaped(const int ports, const int profiles, const int addons) {
    Config made;

    made.general.alwaysAdd = "-nomusic +vid_vsync 0";
    made.general.dosbox = "/usr/bin/dosbox-staging";
    made.general.profileConfigs = true;
    made.general.theme = "dark";
    made.general.gamePort = "5029";
    made.general.lastDirs = {"/home/user/Games/doom",
                             "/home/user/Games/doom/wads",
                             "/home/user/.local/share/qzdl/ports",
                             "/home/user/.config/gzdoom/saves",
                             "/home/user/Games/doom/zdl",
                             "/home/user/.config/gzdoom",
                             "/home/user/Games/doom/demos"};
    made.general.window = {1600, 1000, 120, 80, true, true};

    for (int at = 0; at < 12; ++at) {
        const unsigned seed = static_cast<unsigned>(at);

        made.iwads.push_back({.name = "Game " + words(2, 500U + seed),
                              .file = "/home/user/.local/share/games/doom/" + words(1, 900U + seed)
                                  + std::to_string(at) + ".wad"});
    }

    for (int at = 0; at < ports; ++at) {
        const std::string version = "4." + std::to_string(at % 20) + "." + std::to_string(at % 3);
        const std::string id = "gzdoom-" + version + "-" + std::to_string(at);

        made.ports.push_back({.name = "GZDoom " + version + " (" + std::to_string(at) + ")",
                              .file = "/home/user/.local/share/qzdl/ports/" + id + "/gzdoom",
                              .dosbox = at % 5 == 0,
                              .portId = id});
        made.general.detected.push_back(made.ports.back().file);
    }

    for (int at = 0; at < profiles; ++at) {
        const std::string index = std::to_string(at);
        Profile profile;

        profile.id = "9b3f1c2e-4d5a-4f6b-8c7d-" + std::string(12 - index.size(), '0') + index;
        profile.name = words(3, 11U + static_cast<unsigned>(at));

        if (at % 7 == 3) {
            profile.name += " \"final\" \u00e9dition";
        }

        profile.iwad = made.iwads[static_cast<size_t>(at) % made.iwads.size()].name;
        profile.port = made.ports[static_cast<size_t>(at) % made.ports.size()].name;
        profile.skill = 1 + (at % 5);
        profile.monsters = at % 3;
        profile.warp = "MAP" + std::to_string(1 + (at % 32));
        profile.extra = at % 2 == 0 ? "-fast -respawn" : "";
        profile.config = "profile" + index + ".ini";
        profile.dosFullscreen = at % 3 != 0;
        profile.captureOutput = at % 4 == 0;

        for (int file = 0; file < addons; ++file) {
            const unsigned seed = 1U + static_cast<unsigned>(file) + static_cast<unsigned>(at) * 7U;
            std::string name = words(2, seed);

            std::replace(name.begin(), name.end(), ' ', '-');

            profile.files.push_back({.file = "/home/user/Games/doom/mods/" + words(1, seed + 3U) + "/"
                                         + name + std::to_string(file) + (file % 3 == 0 ? ".pk3" : ".wad"),
                                     .enabled = file % 4 != 0});
        }

        profile.multiplayer.gameType = gameTypeOf(at % 3);
        profile.multiplayer.players = 2 + (at % 6);
        profile.multiplayer.host = "10.0.0." + std::to_string(at % 255);
        profile.multiplayer.port = "5029";
        profile.multiplayer.dmflags = std::to_string(at * 1024);

        profile.replay.mode = replayModeOf(at % 3);
        profile.replay.file = "/home/user/Games/doom/demos/run" + index + ".lmp";

        profile.save.file = "/home/user/.config/gzdoom/saves/save" + std::to_string(at % 10) + ".zds";

        made.profiles.push_back(std::move(profile));
    }

    if (!made.profiles.empty()) {
        made.activeProfileId = made.profiles.front().id;
    }

    return made;
}

void shapes(benchmark::Benchmark *bench) {
    bench->Args({2, 3, 4})
        ->Args({12, 8, 16})
        ->Args({32, 64, 64})
        ->Args({64, 256, 256})
        ->ArgNames({"ports", "profiles", "addons"});
}

Config grounded(const int profiles, const int filesPerProfile) {
    Config made = config(profiles, filesPerProfile);

    const std::vector<std::string> &files = Corpus::addons(filesPerProfile);

    made.iwads.front().file = Corpus::iwad().string();
    made.iwads.front().name = "Benchmark Doom";
    made.iwads[1].file = Corpus::pk3().string();
    made.iwads[1].name = "Benchmark PK3";

    for (Profile &profile : made.profiles) {
        profile.iwad = "Benchmark Doom";

        for (size_t at = 0; at < profile.files.size() && at < files.size(); ++at) {
            profile.files[at].file = files[at];
        }
    }

    return made;
}

void install(Config what) {
    Session::get().config() = std::move(what);
}

}
