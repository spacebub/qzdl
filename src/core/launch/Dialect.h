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

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/config/Config.h"
#include "core/launch/PortLine.h"

namespace Dialect {

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

// How -loadgame names a save: slot index, name inside the save folder, or a path.
enum class SaveNames : std::uint8_t {
    none,
    slot,
    name,
    path,
};

// zdoom waits for a player count, chocolate hosts its own lobby, prboom only joins a prboom-server.
enum class Netgames : std::uint8_t {
    none,
    zdoom,
    chocolate,
    prboom,
};

// How a port is made to start each map with a pistol.
enum class PistolStart : std::uint8_t {
    none,
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

struct DemoSupport {
    bool records{false};
    bool continues{false};
    bool timed{false};
    bool fast{false};
    Complevels complevel{Complevels::none};
    bool longtics{false};
    bool soloNet{false};
};

struct SaveSupport {
    SaveNames names{SaveNames::none};

    // Without a save folder the saves cannot be listed.
    bool folder{false};
};

struct NetSupport {
    bool hosts{false};
    bool joins{false};

    // ZDoom family only.
    bool players{false};

    bool listing{false};

    // ZDoom family cvars. Without the cvar the time limit goes as -timer.
    bool fragLimit{false};
    bool timeLimitCvar{false};
    bool savegame{false};
    bool netgameFolder{false};

    bool extratic{false};
    bool netmode{false};
    bool dup{false};

    // Deathmatch 3.0, with weapons that stay and items that respawn.
    bool deathmatch3{false};

    // ZDoom family userinfo: name, color and team.
    bool player{false};
    bool teams{false};
    bool teamPick{false};
};

// Defaults are the ZDoom family, which is also what an unrecognised port is taken for.
// Grouped as the initializers in Dialect.cpp are, and only ever a few alive.
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct Port {
    // Runs inside DOSBox. Only the entry and Config overloads of of() can tell.
    bool dos{false};

    // False when the name matched nothing and these are guesses.
    bool recognised{true};

    // Stored in a port entry when the dialect is picked by hand.
    std::string_view id{"gzdoom"};

    // Pre-Boom ports find the game through $DOOMWADDIR instead.
    bool iwad{true};

    // Save folder switch. Empty for a port without one.
    std::string_view save{"-savedir"};

    SaveNames loads{SaveNames::name};
    std::string_view saveExt{".zds"};

    // The highest slot -loadgame takes.
    int lastSlot{9999};

    // Chocolate Doom's second config file.
    bool extraConfig{false};

    // Takes -config. A port without it keeps whatever config it writes on its own.
    bool configFile{true};

    // Names the config in the profile folder, so ports that write other formats keep apart.
    // Empty for the ZDoom family, whose config is named after the profile.
    std::string_view configName{};

    // Chocolate Doom finds the sprites and flats of a PWAD only when it is merged in.
    std::string_view pwads{"-file"};

    // Empty for a port that cannot take the patch.
    std::string_view deh{"-deh"};
    std::string_view bex{"-bex"};

    // ZDoom's own.
    bool exec{true};
    bool map{true};

    // -warp takes a two digit map after the episode. The ZDoom family and Chocolate read one digit.
    bool longWarp{false};

    bool respawn{true};

    Netgames netgame{Netgames::zdoom};

    // UZDoom dropped -netmode.
    bool netmode{true};

    // Hosts Deathmatch 3.0. Empty for a port without it.
    std::string_view deathmatch3{};

    // In a netgame UZDoom reads saves only from its own netgame folder and ignores -savedir.
    bool netgameSaves{false};

    // Every port has -record and -playdemo. The rest varies.
    bool demos{true};
    bool timedemo{true};
    bool fastdemo{false};
    Complevels complevel{Complevels::none};

    // Stops on -record without -complevel. DSDA-Doom also takes the level a COMPLVL lump names.
    bool recordsWithComplevel{false};

    // A complevel recording stops on, -1 for none. Woof plays id24 but does not record it.
    int unrecordable{-1};

    bool longtics{false};
    bool soloNet{false};

    // -record adds .lmp to the name it is given. Helion writes the exact path.
    bool recordAddsLmp{true};

    // Writes levelstat.txt with a time per level.
    bool levelstat{false};

    PistolStart pistolStart{PistolStart::cvar};

    // +sv_cheats. The rest allow cheats in single player anyway.
    bool cheats{true};

    // Skips the files the port loads on its own. Empty for a port without any.
    std::string_view noAutoload{"-noautoload"};

    bool nomusic{true};

    // Turns off the sound effects and leaves the music. Empty for a port without one.
    std::string_view nosfx{"-nosfx"};

    Windowing display{Windowing::cvar};

    // The port saves the choice, so it outlives the launch.
    bool displayKept{true};

    bool shotdir{true};

    // +team, which Zandronum takes only once a client is in the game.
    bool teamPick{true};

    // -recordfromto. Some also insist on -complevel, which the demo header then overrides.
    bool continues{false};
    bool continueComplevel{false};

    // Which flag words and rule switches it reads.
    PortLine line{PortLine::gzdoom};
};

struct Choice {
    std::string_view id;
    std::string_view name;

    // Spoken by programs run inside DOSBox.
    bool dos{false};
};

// Every dialect, as a port can be set to one by hand.
[[nodiscard]] std::span<const Choice> choices();

[[nodiscard]] const Choice *choice(std::string_view id);

// Told from the program's name.
[[nodiscard]] Port of(const std::filesystem::path &program);

// A dialect picked by hand wins over the name, which still decides the details when it names a
// port of that dialect, such as Nugget Doom for Woof. An empty or unknown id leaves it to the name.
[[nodiscard]] Port of(const std::filesystem::path &program, std::string_view picked);

// Off Windows a .exe, .com or batch file can only run inside DOSBox, since no port is run
// through Wine. On Windows the name cannot tell.
[[nodiscard]] std::optional<bool> dosFromName(const std::filesystem::path &program);

// A program in flatpak/exports/bin, named for its app ID.
[[nodiscard]] bool flatpakExport(const std::filesystem::path &program);

// A DOS port is answered for as it runs inside DOSBox.
[[nodiscard]] Port of(const PortEntry &entry);
[[nodiscard]] Port of(const Config &config);
[[nodiscard]] Port of(const Config &config, const Profile &profile);

// -1 is the port's own.
[[nodiscard]] std::vector<int> complevels(Complevels which);

// The level as it goes on the command line. Empty for -1.
[[nodiscard]] std::string complevelWord(Complevels which, int level);

// The switch that sets the level.
[[nodiscard]] std::string_view complevelSwitch(Complevels which);

// What a game version is called, such as Ultimate Doom. Empty for any other level.
[[nodiscard]] std::string_view versionName(int level);

// What the Gameplay and Replay panels call the level.
[[nodiscard]] std::string_view complevelTitle(Complevels which);

[[nodiscard]] DemoSupport demos(const Port &speaks);
[[nodiscard]] SaveSupport saves(const Port &speaks);
[[nodiscard]] NetSupport net(const Port &speaks);

}
