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
#include <tuple>
#include <vector>

#include "core/config/Config.h"
#include "core/launch/Feature.h"
#include "core/launch/Port.h"
#include "core/launch/PortLine.h"

namespace Dialect {

// How ZDL4 speaks to a port. Each port speaks one.
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

    // The Doom releases -gameversion emulates, numbered from GAME_VERSIONS on so no complevel is taken for one. Each
    // list adds to the one before it and is named for the first release it adds. gameVersion holds them all.
    gameVersion19,
    gameVersion1666,
    gameVersion12,
    gameVersion,

    woof,
    woofToMbf21,
    prboom,
    dsda,
    // vanilla, boom, mbf and mbf21 through +complevel.
    helion,
};

inline constexpr int GAME_VERSIONS = 100;

// What the level list of a port holds.
enum class Compatibility : std::uint8_t {
    none,
    complevels,

    // The Doom releases Chocolate and Crispy emulate.
    versions,

    // The compatibility modes of a ZDoom family port, which fill compatflags and compatflags2.
    modes,
};

// What a setting left to the port is called.
inline constexpr std::string_view PORTS_OWN = "The port's own";

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

// What a slot holds on disk. The name is the slot prefix of the port, the slot and the save extension.
enum class SaveLayout : std::uint8_t {
    file,

    // Hexen: the main file of a slot takes one or two digits. Longer numbers name the map files of a hub. Slots 6
    // and 7 hold the game in play and the one a death restarts.
    hub,

    // Strife: a folder per slot, holding the description in a file called name. Slot 6 holds the game in play.
    folder,
};

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
struct Traits {
    Features features;

    std::string_view saveExt{".zds"};

    // The save -loadgame AUTOSAVE_SLOT loads. Empty for a port without one.
    std::string_view autosave{};

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
    SaveLayout saveLayout{SaveLayout::file};
    Netgames netgame{Netgames::zdoom};
    Complevels complevel{Complevels::none};

    // Which table of flag bits and modes it reads.
    PortLine line{PortLine::gzdoom};

    // The port picked for the engine, or else the one the file name gives.
    Port::Port port{};

    [[nodiscard]] constexpr bool has(const Feature one) const { return features.has(one); }

    [[nodiscard]] constexpr bool loadsBy(const SaveNames form) const {
        return features.has(Feature::loadgame) && loads == form;
    }
};

// What a port records with. None without replays.
[[nodiscard]] constexpr Complevels replayComplevel(const Traits &traits) {
    return traits.has(Feature::replays) ? traits.complevel : Complevels::none;
}

// The spelling of a feature on this port: its change, or the baseline.
[[nodiscard]] const Arg &arg(const Traits &traits, Feature feature);

// What a feature does on this port. A change that does not say keeps the baseline description.
[[nodiscard]] std::string_view describe(const Traits &traits, Feature feature);

// Writes a feature as the port spells it: the name, its fixed word if any, then the value. False for a port
// without it and for a feature with no spelling, which write nothing.
bool add(std::vector<std::string> &args, const Traits &traits, Feature feature);
bool add(std::vector<std::string> &args, const Traits &traits, Feature feature, std::string value);

// A whole decimal number within the range. Empty text is not one.
[[nodiscard]] std::optional<int> parse(const Arg &arg, std::string_view typed);

// Whether the port takes the switch with `level` on the line, -1 for none.
[[nodiscard]] bool worksAt(const Arg &arg, int level);

// The ports an engine can be set to, in the order a list shows them. With dos, the ports that run inside
// DOSBox.
[[nodiscard]] std::span<const Port::Port> picks(bool dos);

// The port the dialect is named for. It leads the dialect in a pick list.
[[nodiscard]] Port::Port lead(Dialect dialect);

// Told from the file name.
[[nodiscard]] Traits of(const std::filesystem::path &file);

// The port picked wins over the name. Unknown leaves it to the name.
[[nodiscard]] Traits of(const std::filesystem::path &file, Port::Port picked);

// The port the file name gives the engine, whatever it picked. A port without a name shows as the lead of
// its dialect.
[[nodiscard]] Port::Port fromName(const Engine &engine);

// Off Windows a .exe, .com or batch file can only run inside DOSBox, since no port is run
// through Wine. On Windows the name cannot tell.
[[nodiscard]] std::optional<bool> dosFromName(const std::filesystem::path &file);

// A DOS port is answered for as it runs inside DOSBox.
[[nodiscard]] Traits of(const Engine &engine);
[[nodiscard]] Traits of(const Config &config);
[[nodiscard]] Traits of(const Config &config, const Profile &profile);

// What the answer for an engine depends on. Engines with equal keys are answered alike.
[[nodiscard]] inline auto answerKey(const Engine &engine) {
    return std::tie(engine.file, engine.port, engine.version, engine.managed, engine.dosbox);
}

// An answer key kept past the engine it was taken from.
using AnswerKey = std::tuple<std::string, Port::Port, std::string, bool, bool>;

// A span of releases in which ZDL4 speaks to a port the same way. What it lacks and gains is
// against the latest release, which the traits of the port describe.
struct VersionRange {
    // What an engine stores when the range is picked by hand.
    std::string_view first;

    std::string_view label;

    // Features the latest release has and this range does not.
    Features lacks;

    // Features this range has and the latest release does not.
    Features gains;

    // Replaces the changes in the traits when not empty.
    std::span<const Spelling> changes;

    // Only for what the masks cannot say. Null for none.
    void (*special)(Traits &traits);
};

// Newest first. Empty for a port with no version rules.
[[nodiscard]] std::span<const VersionRange> versions(const Engine &engine);

// The release at the start of text, such as 4.14.2 in g4.14.2-0-g1234abc: digits, dots and letters from the first
// digit on. Empty for text without one.
[[nodiscard]] std::string_view leadingRelease(std::string_view text);

// The range a release falls in. That is the newest range that starts at or before it. A release older than every
// range falls in the oldest. Null when there are no ranges or the version holds no release.
[[nodiscard]] const VersionRange *rangeOf(std::span<const VersionRange> ranges, std::string_view version);

// The port has ranges and the stored version holds no release. A managed engine and a DOSBox engine are never asked.
[[nodiscard]] bool needsVersion(const Engine &engine);

// Reads the version text of a file, such as the Windows FileVersion string.
using VersionReader = std::string (*)(const std::filesystem::path &file);

// Suggests a version from the file: the release its text holds. Empty when the engine needs no version or
// the text holds no release with a dot. The file is read only when a version is needed.
[[nodiscard]] std::string suggestedVersion(const Engine &engine, VersionReader read);

// -1 is the port's own.
[[nodiscard]] std::span<const int> complevels(Complevels which);

// The level as it goes on the command line. Empty for -1.
[[nodiscard]] std::string complevelWord(Complevels which, int level);

// Which list the port offers: complevels, game versions or compatibility modes.
[[nodiscard]] Compatibility compatibility(const Traits &traits);

// What the level is called on that list, such as Complevel or Game version.
[[nodiscard]] std::string_view levelTitle(Compatibility kind);

// What the level list of the port offers and fills.
[[nodiscard]] std::string_view levelHint(const Traits &traits);

// What a level is called, such as Boom v2.02 or Ultimate Doom. -1 is the port's own. Empty for a number that
// names no level.
[[nodiscard]] std::string_view levelName(int level);

// A game version older than Final Doom, which plays plutonia.wad and tnt.wad as Doom II.
[[nodiscard]] bool beforeFinalDoom(int level);

// The level, or -1 when the port does not read it.
[[nodiscard]] int pickedLevel(Complevels which, int level);

// The picked level by name and by the word on the command line, such as Boom v2.02 · 9. Empty for none.
[[nodiscard]] std::string levelLabel(Complevels which, int level);

[[nodiscard]] bool isAutosave(const Traits &traits, std::string_view file);

}
