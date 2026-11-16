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
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/config/Config.h"
#include "core/launch/Feature.h"
#include "core/launch/PortLine.h"

namespace Program {
enum class Program : std::uint8_t;
}

namespace Dialect {

// Stored in a port entry, by its word, when the dialect is picked by hand.
enum class Dialect : std::uint8_t {
    gzdoom,
    uzdoom,
    zdoom,
    zandronum,
    dsda,
    prboom,
    woof,
    chocolate,
    eternity,
    helion,
    boom,
    legacy,
    doom,
};

// Which -complevel numbers a port accepts.
enum class Complevels : std::uint8_t {
    none,
    woof,
    prboom,
    dsda,
    // vanilla, boom, mbf and mbf21 through +complevel.
    helion,
    // The Doom release -gameversion emulates, numbered from GAME_VERSIONS on so no complevel is taken for one.
    gameVersion,
};

inline constexpr int GAME_VERSIONS = 100;

// The complevels the switch tables name.
inline constexpr int MBF = 11;
inline constexpr int MBF21 = 21;
inline constexpr int ID24 = 24;

// How -loadgame names a save: slot index, name inside the save folder, or a path.
enum class SaveNames : std::uint8_t {
    slot,
    name,
    path,
};

inline constexpr int AUTOSAVE_SLOT = 255;

// zdoom waits for a player count, chocolate hosts its own lobby, prboom only joins a prboom-server.
enum class Netgames : std::uint8_t {
    none,
    zdoom,
    chocolate,
    prboom,
};

// Where a port stops on a switch, so the launch leaves it out there.
inline constexpr std::uint32_t RECORDING = 1U;
inline constexpr std::uint32_t PLAYBACK = 2U;
inline constexpr std::uint32_t NETGAME = 4U;

struct Range {
    int least{0};
    int most{0};

    friend constexpr bool operator==(const Range &, const Range &) = default;
};

// The complevels a port takes a switch at, -1 for any.
struct Levels {
    int from{-1};
    int to{-1};

    // The level has to be on the line, since the default of the port does not qualify.
    bool onLine{false};

    friend constexpr bool operator==(const Levels &, const Levels &) = default;
};

// How a port spells a feature on its command line.
struct Arg {
    // The switch itself, such as -turbo.
    std::string_view name;

    std::string_view description{};

    // A fixed word after the name, as +sv_cheats 1 needs. Empty for none.
    std::string_view word{};

    Range range{};
    std::uint32_t refused{0};
    Levels levels{};

    friend constexpr bool operator==(const Arg &, const Arg &) = default;
};

struct Spelling {
    Feature feature{};
    Arg arg{};
};

// Defaults are the ZDoom family, which is also what an unrecognised port is taken for.
// Biggest members first, and only ever a few alive.
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct Port {
    Features features;

    std::string_view saveExt{".zds"};

    // The save -loadgame AUTOSAVE_SLOT loads. Empty for a port without one.
    std::string_view autosave{};

    // With a number after it, a periodic autosave that only the menu loads.
    std::string_view periodicAutosave{};

    // Names the config in the profile folder, so ports that write other formats keep apart.
    // Empty for the ZDoom family, whose config is named after the profile.
    std::string_view configName{};

    // What it spells other than the baseline.
    std::span<const Spelling> changes{};

    // The highest slot -loadgame takes.
    int lastSlot{9999};

    // A complevel recording stops on, -1 for none. Woof plays id24 but does not record it.
    int unrecordable{-1};

    Dialect dialect{Dialect::gzdoom};
    SaveNames loads{SaveNames::name};
    Netgames netgame{Netgames::zdoom};
    Complevels complevel{Complevels::none};

    // Which table of flag bits and modes it reads.
    PortLine line{PortLine::gzdoom};

    // The program the file name names. For a dialect picked by hand, the program the dialect is named for.
    Program::Program program{};

    [[nodiscard]] constexpr bool has(const Feature one) const { return features.has(one); }

    [[nodiscard]] constexpr bool loadsBy(const SaveNames form) const {
        return features.has(Feature::loadgame) && loads == form;
    }
};

// What a port records with. None without demos.
[[nodiscard]] constexpr Complevels replayComplevel(const Port &port) {
    return port.has(Feature::demos) ? port.complevel : Complevels::none;
}

// The spelling of a feature on this port: its change, or the baseline.
[[nodiscard]] const Arg &arg(const Port &port, Feature feature);

// What a feature does on this port. A change that does not say keeps the baseline description.
[[nodiscard]] std::string_view describe(const Port &port, Feature feature);

// Writes a feature as the port spells it: the name, its fixed word if any, then the value. False for a port
// without it and for a feature with no spelling, which write nothing.
bool add(std::vector<std::string> &args, const Port &port, Feature feature);
bool add(std::vector<std::string> &args, const Port &port, Feature feature, std::string value);

// A whole decimal number within the range. Empty text is not one.
[[nodiscard]] std::optional<int> parse(const Arg &arg, std::string_view typed);

// Whether the port takes the switch with `level` on the line, -1 for none.
[[nodiscard]] bool worksAt(const Arg &arg, int level);

struct Choice {
    Dialect dialect;

    // How a port entry stores it.
    std::string_view word;

    std::string_view name;

    // Spoken by programs run inside DOSBox.
    bool dos{false};
};

// Every dialect, as a port can be set to one by hand.
[[nodiscard]] std::span<const Choice> choices();

[[nodiscard]] const Choice &choice(Dialect dialect);

// Null for a word that names no dialect.
[[nodiscard]] const Choice *choice(std::string_view word);

// Told from the program's name.
[[nodiscard]] Port of(const std::filesystem::path &program);

// A dialect picked by hand wins over the name, which still decides the details when it names a
// port of that dialect, such as Nugget Doom for Woof. An empty or unknown word leaves it to the name.
[[nodiscard]] Port of(const std::filesystem::path &program, std::string_view picked);

// The word an entry stores for a dialect picked by hand. Empty when the file name names a port of that
// dialect. An unknown file name keeps the word, since GZDoom is only a guess for it.
[[nodiscard]] std::string_view pickWord(const PortEntry &entry, Dialect picked);

// Off Windows a .exe, .com or batch file can only run inside DOSBox, since no port is run
// through Wine. On Windows the name cannot tell.
[[nodiscard]] std::optional<bool> dosFromName(const std::filesystem::path &program);

// A DOS port is answered for as it runs inside DOSBox.
[[nodiscard]] Port of(const PortEntry &entry);
[[nodiscard]] Port of(const Config &config);
[[nodiscard]] Port of(const Config &config, const Profile &profile);

// A span of releases in which ZDL4 speaks to a port the same way. What it lacks and gains is
// against the latest release, which the port definition describes.
struct VersionRange {
    // What a port entry stores.
    std::string_view first;

    std::string_view label;

    // Features the latest release has and this range does not.
    Features lacks;

    // Features this range has and the latest release does not.
    Features gains;

    // Replaces the changes of the port when not empty.
    std::span<const Spelling> changes;

    // Only for what the masks cannot say. Null for none.
    void (*special)(Port &port);
};

// Newest first. Empty for a port with no version rules.
[[nodiscard]] std::span<const VersionRange> versions(const PortEntry &entry);

// A port with ranges whose stored version names none of them. A managed port and a DOSBox program
// are never asked.
[[nodiscard]] bool needsVersion(const PortEntry &entry);

// -1 is the port's own.
[[nodiscard]] std::span<const int> complevels(Complevels which);

// The level as it goes on the command line. Empty for -1.
[[nodiscard]] std::string complevelWord(Complevels which, int level);

// What a game version is called, such as Ultimate Doom. Empty for any other level.
[[nodiscard]] std::string_view versionName(int level);

[[nodiscard]] bool isAutosave(const Port &port, std::string_view file);

}
