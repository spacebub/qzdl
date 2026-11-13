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
#include <string_view>
#include <vector>

#include "core/config/Config.h"

namespace Dialect {

// Which -complevel numbers a port accepts.
enum class Complevels : std::uint8_t {
    none,
    woof,
    prboom,
    dsda,
};

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

    // ZDoom family cvars.
    bool fragLimit{false};
    bool flags{false};
    bool savegame{false};
    bool netgameFolder{false};

    bool extratic{false};
    bool netmode{false};
    bool dup{false};

    // ZDoom family userinfo: name, color and team.
    bool player{false};
    bool teams{false};
    bool teamPick{false};
};

// Defaults are the ZDoom family, which is also what an unrecognised port is taken for.
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

    // Chocolate Doom's second config file.
    bool extraConfig{false};

    // Takes -config. A port without it keeps whatever config it writes on its own.
    bool configFile{true};

    // Names the config in the profile folder, so ports that write other formats keep apart.
    // Empty for the ZDoom family, whose config is named after the profile.
    std::string_view configName{};

    // Empty for a port that cannot take the patch.
    std::string_view deh{"-deh"};
    std::string_view bex{"-bex"};

    // ZDoom's own.
    bool exec{true};
    bool map{true};

    bool respawn{true};

    Netgames netgame{Netgames::zdoom};

    // UZDoom dropped -netmode.
    bool netmode{true};

    // In a netgame UZDoom reads saves only from its own netgame folder and ignores -savedir.
    bool netgameSaves{false};

    // Every port has -record and -playdemo. The rest varies.
    bool demos{true};
    bool timedemo{true};
    bool fastdemo{false};
    Complevels complevel{Complevels::none};
    bool longtics{false};
    bool soloNet{false};

    // Writes levelstat.txt with a time per level.
    bool levelstat{false};

    PistolStart pistolStart{PistolStart::cvar};

    // +sv_cheats. The rest allow cheats in single player anyway.
    bool cheats{true};

    bool shotdir{true};

    // +team, which Zandronum takes only once a client is in the game.
    bool teamPick{true};

    // -recordfromto. Some also insist on -complevel, which the demo header then overrides.
    bool continues{false};
    bool continueComplevel{false};
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

[[nodiscard]] DemoSupport demos(const Port &speaks);
[[nodiscard]] SaveSupport saves(const Port &speaks);
[[nodiscard]] NetSupport net(const Port &speaks);

}
