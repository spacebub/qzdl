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
#include "core/config/GameplayKeys.h"
#include "core/launch/PortLine.h"

namespace Dialect {

// Stored in a port entry, by its word, when the dialect is picked by hand.
enum class Id : std::uint8_t {
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

// How a port is made to start each map with a pistol.
enum class PistolStart : std::uint8_t {
    cvar,
    flag,
};

// How a port is told to run in a window or fullscreen.
enum class Windowing : std::uint8_t {
    none,
    // -window and -fullscreen.
    switches,
    // -window and -nowindow, the pair PrBoom+ does not save.
    noWindow,
    // +vid_fullscreen.
    cvar,
    // +fullscreen, the ZDoom 2.8 name.
    oldCvar,
};

// What a port takes, as far as it is a yes or no. The form of a capability that comes in several
// is a value in Port, and its bit says whether the port has it at all.
enum class Feature : std::uint8_t {
    // Runs inside DOSBox. Only the entry and Config overloads of of() can tell.
    dos,

    // Clear when the name matched nothing and these are guesses.
    recognised,

    // Pre-Boom ports find the game through $DOOMWADDIR instead.
    iwad,

    // Chocolate Doom's second config file.
    extraConfig,

    // Takes -config. A port without it keeps whatever config it writes on its own.
    configFile,

    // ZDoom's own.
    exec,
    map,

    // -warp takes a two digit map after the episode. The ZDoom family and Chocolate read one digit.
    longWarp,

    respawn,

    // Reads -loadgame, in the form Port::loads names.
    loadgame,

    // Without a save folder the saves cannot be listed.
    saveFolder,

    // Every port has -record and -playdemo. The rest varies.
    demos,
    timedemo,
    fastdemo,

    // Stops on -record without -complevel. DSDA-Doom also takes the level a COMPLVL lump names.
    recordsWithComplevel,

    // Stops on -record without -skill. Continuing a demo with -recordfromto needs none.
    recordsWithSkill,

    // Stops on -recordfromto without -skill as well.
    continuesWithSkill,

    longtics,
    soloNet,

    // -record adds .lmp to the name it is given. Helion writes the exact path.
    recordAddsLmp,

    // -recordfromto. Some also insist on -complevel, which the demo header then overrides.
    continues,
    continueComplevel,

    // Writes levelstat.txt with a time per level.
    levelstat,

    // Starts each map with a pistol, in the form Port::pistolStart names.
    pistolStart,

    // +sv_cheats. The rest allow cheats in single player anyway.
    cheats,

    nomusic,

    // The port saves the display choice, so it outlives the launch.
    displayKept,

    shotdir,

    // The netgame features follow from Port::netgame, except the three marked.
    hosts,
    joins,

    // ZDoom family only.
    players,

    listing,

    // ZDoom family cvars. Without the cvar the time limit goes as -timer.
    fragLimit,
    timeLimitCvar,
    savegame,

    extratic,
    dup,

    // UZDoom dropped -netmode.
    netmode,

    // Deathmatch 3.0, with weapons that stay and items that respawn.
    deathmatch3,

    // ZDoom family userinfo: name, color and team.
    player,
    teams,

    // +team, which Zandronum takes only once a client is in the game.
    teamPick,

    // In a netgame UZDoom reads saves only from its own netgame folder and ignores -savedir.
    netgameFolder,

    count,
};

static_assert(static_cast<unsigned>(Feature::count) <= 64, "the features fit one 64 bit mask");

struct Features {
    std::uint64_t bits{0};

    constexpr Features() = default;

    constexpr Features(const Feature one) : bits(std::uint64_t{1} << static_cast<unsigned>(one)) {}

    [[nodiscard]] constexpr bool has(const Feature one) const { return (bits & Features(one).bits) != 0; }

    constexpr Features &operator|=(const Features other) {
        bits |= other.bits;

        return *this;
    }

    constexpr Features &operator&=(const Features other) {
        bits &= other.bits;

        return *this;
    }

    friend constexpr bool operator==(const Features &, const Features &) = default;
};

[[nodiscard]] constexpr Features operator|(Features left, const Features right) {
    return left |= right;
}

[[nodiscard]] constexpr Features operator|(const Feature left, const Feature right) {
    return Features(left) | Features(right);
}

[[nodiscard]] constexpr Features operator&(Features left, const Features right) {
    return left &= right;
}

[[nodiscard]] constexpr Features operator~(const Features all) {
    Features out;
    out.bits = ~all.bits;

    return out;
}

// Defaults are the ZDoom family, which is also what an unrecognised port is taken for.
// Biggest members first, and only ever a few alive.
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct Port {
    Features features;

    // Save folder switch. Empty for a port without one.
    std::string_view save{"-savedir"};

    std::string_view saveExt{".zds"};

    // The save -loadgame AUTOSAVE_SLOT loads. Empty for a port without one.
    std::string_view autosave{};

    // With a number after it, a periodic autosave that only the menu loads.
    std::string_view periodicAutosave{};

    // Names the config in the profile folder, so ports that write other formats keep apart.
    // Empty for the ZDoom family, whose config is named after the profile.
    std::string_view configName{};

    // Chocolate Doom finds the sprites and flats of a PWAD only when it is merged in.
    std::string_view pwads{"-file"};

    // Empty for a port that cannot take the patch.
    std::string_view deh{"-deh"};
    std::string_view bex{"-bex"};

    // Hosts Deathmatch 3.0. Empty for a port without it.
    std::string_view deathmatch3{};

    // Skips the files the port loads on its own. Empty for a port without any.
    std::string_view noAutoload{"-noautoload"};

    // Turns off the sound effects and leaves the music. Empty for a port without one.
    std::string_view nosfx{"-nosfx"};

    // The highest slot -loadgame takes.
    int lastSlot{9999};

    // A complevel recording stops on, -1 for none. Woof plays id24 but does not record it.
    int unrecordable{-1};

    // The rules this release has no switch for, one bit per Rules::Rule.
    std::uint32_t withoutRules{0};

    Id id{Id::gzdoom};
    SaveNames loads{SaveNames::name};
    Netgames netgame{Netgames::zdoom};
    Complevels complevel{Complevels::none};
    PistolStart pistolStart{PistolStart::cvar};
    Windowing display{Windowing::cvar};

    // Which flag words and rule switches it reads.
    PortLine line{PortLine::gzdoom};

    [[nodiscard]] constexpr bool has(const Feature one) const { return features.has(one); }

    [[nodiscard]] constexpr bool loadsBy(const SaveNames form) const {
        return features.has(Feature::loadgame) && loads == form;
    }

    [[nodiscard]] constexpr bool startsBy(const PistolStart form) const {
        return features.has(Feature::pistolStart) && pistolStart == form;
    }
};

// What a port records with. None without demos.
[[nodiscard]] constexpr Complevels replayComplevel(const Port &port) {
    return port.has(Feature::demos) ? port.complevel : Complevels::none;
}

[[nodiscard]] constexpr std::uint32_t ruleBit(const Rules::Rule rule) {
    return 1U << Rules::index(rule);
}

[[nodiscard]] constexpr bool lacksRule(const Port &port, const Rules::Rule rule) {
    return (port.withoutRules & ruleBit(rule)) != 0;
}

struct Choice {
    Id id;

    // How a port entry stores it.
    std::string_view word;

    std::string_view name;

    // Spoken by programs run inside DOSBox.
    bool dos{false};
};

// Every dialect, as a port can be set to one by hand.
[[nodiscard]] std::span<const Choice> choices();

[[nodiscard]] const Choice &choice(Id id);

// Null for a word that names no dialect.
[[nodiscard]] const Choice *choice(std::string_view word);

// Told from the program's name.
[[nodiscard]] Port of(const std::filesystem::path &program);

// A dialect picked by hand wins over the name, which still decides the details when it names a
// port of that dialect, such as Nugget Doom for Woof. An empty or unknown word leaves it to the name.
[[nodiscard]] Port of(const std::filesystem::path &program, std::string_view picked);

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

    // Bits of Rules::Rule this range has no switch for.
    std::uint32_t withoutRules;

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

[[nodiscard]] std::string_view complevelSwitch(Complevels which);

// What a game version is called, such as Ultimate Doom. Empty for any other level.
[[nodiscard]] std::string_view versionName(int level);

[[nodiscard]] bool isAutosave(const Port &port, std::string_view file);

}
