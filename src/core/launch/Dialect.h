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
#include <string_view>
#include <vector>

#include "core/config/Config.h"

namespace Dialect {

// Which -complevel numbers a port accepts.
enum class Complevels : std::uint8_t { none, woof, prboom, dsda };

// How -loadgame names a save: slot index, name inside the save folder, or a path.
enum class SaveNames : std::uint8_t { none, slot, name, path };

// zdoom waits for a player count, chocolate hosts its own lobby, prboom only joins a prboom-server.
enum class Netgames : std::uint8_t { none, zdoom, chocolate, prboom };

struct DemoSupport {
    bool records{false};
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

    bool extratic{false};
    bool netmode{false};
    bool dup{false};
};

// Defaults are the ZDoom family, which is also what an unrecognised port is taken for.
struct Port {
    // Runs inside DOSBox. Only the Config overloads of of() can tell.
    bool dos{false};

    // Pre-Boom ports find the game through $DOOMWADDIR instead.
    bool iwad{true};

    // Save folder switch; empty for a port without one.
    std::string_view save{"-savedir"};

    SaveNames loads{SaveNames::name};
    std::string_view saveExt{".zds"};

    // Chocolate Doom's second config file.
    bool extraConfig{false};

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

    // Every port has -record and -playdemo; the rest varies.
    bool demos{true};
    bool timedemo{true};
    bool fastdemo{false};
    Complevels complevel{Complevels::none};
    bool longtics{false};
    bool soloNet{false};
};

// Told from the program's name.
[[nodiscard]] Port of(const std::filesystem::path &program);

// A DOS port is answered for as it runs inside DOSBox.
[[nodiscard]] Port of(const Config &config);
[[nodiscard]] Port of(const Config &config, const Profile &profile);

// -1 is the port's own.
[[nodiscard]] std::vector<int> complevels(Complevels which);

[[nodiscard]] DemoSupport demos(const Port &speaks);
[[nodiscard]] SaveSupport saves(const Port &speaks);
[[nodiscard]] NetSupport net(const Port &speaks);

}
