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
