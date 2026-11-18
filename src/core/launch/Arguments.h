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
#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "core/config/Config.h"
#include "core/launch/Dialect.h"

namespace Arguments {

// A relative file found in directory, as the launch names it. Any other name is left for the port to find.
[[nodiscard]] std::string beside(const std::string &file, const std::filesystem::path &directory);

// Relative files resolve against portDirectory, not the directory the port runs in.
[[nodiscard]] std::vector<std::string> of(const Config &config, const std::filesystem::path &portDirectory = {});

// As of(), without -stdout, which a typed command must not keep once capture is off.
[[nodiscard]] std::vector<std::string> uncaptured(const Config &config,
                                                  const std::filesystem::path &portDirectory = {});

enum class Access : std::uint8_t {
    // The port reads the path, a file or a folder.
    reads,

    // The port writes in the folder.
    writes,
};

// A path the arguments send the port to.
struct Reached {
    std::filesystem::path path;
    Access access;
};

struct Built {
    std::vector<std::string> arguments;

    // Absolute paths only. The port looks for a relative name on its own. The IWAD stands for its folder.
    std::vector<Reached> reached;
};

// As of(), with the paths the arguments reach.
[[nodiscard]] Built withReached(const Config &config, const std::filesystem::path &portDirectory = {});

[[nodiscard]] std::vector<std::string> maps(const Config &config);

// Why the launch leaves the replay out. None puts it on the line.
enum class ReplayLeftOut : std::uint8_t {
    // Off, or a port without replays.
    off,

    none,

    // The launch hosts a server, which records and plays back nothing.
    server,

    cannotContinue,
    noSource,
    noName,
};

static_assert(ReplayLeftOut{} == ReplayLeftOut::off, "a replay nothing has decided is off");

// The replay as the launch resolves it.
struct Replay {
    std::filesystem::path file;
    std::filesystem::path source;
    ReplayLeftOut leftOut;
};

[[nodiscard]] Replay replayOf(const Config &config, const Dialect::Traits &traits);

// The Monsters values the port offers, in the order of the list: no monsters, fast, respawn, then fast and respawn.
// A value is left out where the port lacks one of the switches it sends.
[[nodiscard]] std::span<const int> monsterChoices(const Dialect::Traits &traits);

// The Monsters value that plays outside a demo or a netgame. A pick loses each switch the port lacks.
[[nodiscard]] int playedMonsters(int monsters, const Dialect::Traits &traits);

// The descriptions of the monster switches the port stops on in a demo or a netgame. Empty without one.
[[nodiscard]] std::string monstersHint(const Dialect::Traits &traits);

// The port starts on a picked map, by -warp or by name.
[[nodiscard]] bool takesMap(const Dialect::Traits &traits);

// The profile records a demo and the port stops at launch: without a complevel, or at the level it cannot record.
[[nodiscard]] bool needsComplevel(const Profile &profile, const Dialect::Traits &traits);

// The profile records or continues a demo and the port stops at launch for want of a skill.
[[nodiscard]] bool needsSkill(const Profile &profile, const Dialect::Traits &traits);

// What -record does with a demo already there by the name it records into.
enum class TakenDemo : std::uint8_t {
    writesOver,

    // Records into a numbered name beside it.
    numbers,

    stops,

    // Goes on from the last save stored in the old demo, or else writes over it or stops.
    continuesOrWritesOver,
    continuesOrStops,
};

// For the profile as it records or continues, at the complevel picked.
[[nodiscard]] TakenDemo takenDemo(const Profile &profile, const Dialect::Traits &traits);

// On Windows GZDoom up to 4.13.0 and the ZDoom 2.8 line print to a pipe only when given -stdout.
// Without a pipe -stdout opens a console, so it goes only with a captured launch.
[[nodiscard]] bool asksForStdout(const Config &config, bool windows);
[[nodiscard]] bool asksForStdout(const Config &config, const Dialect::Traits &traits, bool windows);

}
