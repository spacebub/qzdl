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

#include <algorithm>
#include <array>
#include <string>

#include "ttk/system/Text.h"

#include "core/launch/Dialect.h"

using namespace ttk;

namespace Dialect {

namespace {
constexpr Port ZDOOM{};

constexpr Port BOOM{
        .save = "-save",
        .loads = SaveNames::slot,
        .saveExt = ".dsg",
        .bex = "-deh",
        .exec = false,
        .map = false,
        .netgame = Netgames::prboom,
        .fastdemo = true,
        .complevel = Complevels::prboom,
        .longtics = true,
        .soloNet = true,
        .levelstat = true,
};

// No -loadgame, and it stops on a switch it does not know.
constexpr Port DSDA{
        .save = "-save",
        .loads = SaveNames::none,
        .saveExt = ".dsg",
        .configName = "dsda",
        .bex = "-deh",
        .exec = false,
        .map = false,
        .netgame = Netgames::none,
        .fastdemo = true,
        .complevel = Complevels::dsda,
        .longtics = true,
        .soloNet = true,
        .levelstat = true,
};

constexpr Port WOOF{
        .save = "-save",
        .loads = SaveNames::slot,
        .saveExt = ".dsg",
        .bex = "-deh",
        .exec = false,
        .map = false,
        .netgame = Netgames::chocolate,
        .fastdemo = true,
        .complevel = Complevels::woof,
        .longtics = true,
        .soloNet = true,
        .levelstat = true,
};

constexpr Port ETERNITY{
        .save = "-save",
        .loads = SaveNames::slot,
        .saveExt = ".dsg",
        .configName = "eternity",
        .exec = false,
        .map = false,
        .netgame = Netgames::none,
        .fastdemo = true,
        .soloNet = true,
};

constexpr Port BOOM202{
        .save = "-save",
        .loads = SaveNames::slot,
        .saveExt = ".dsg",
        .configFile = false,
        .configName = "mbf",
        .bex = "-deh",
        .exec = false,
        .map = false,
        .netgame = Netgames::none,
        .fastdemo = true,
};

constexpr Port VANILLA{
        .loads = SaveNames::slot,
        .saveExt = ".dsg",
        .extraConfig = true,
        .configName = "chocolate",
        .bex = "-deh",
        .exec = false,
        .map = false,
        .netgame = Netgames::chocolate,
        .longtics = true,
        .soloNet = true,
};

constexpr Port HELION{
        .loads = SaveNames::name,
        .saveExt = ".hsg",
        .configName = "helion",
        .bex = "-deh",
        .exec = false,
        .respawn = false,
        .netgame = Netgames::none,
        .timedemo = false,
        .soloNet = true,
};

// Takes -config but writes to it without ever reading it back, so it keeps
// $DOOMWADDIR/config.cfg instead.
constexpr Port LEGACY{
        .iwad = false,
        .save = "",
        .loads = SaveNames::slot,
        .saveExt = ".dsg",
        .configFile = false,
        .deh = "-dehacked",
        .bex = "-dehacked",
        .exec = false,
        .map = false,
        .netgame = Netgames::none,
};

constexpr Port DOOM{
        .iwad = false,
        .save = "",
        .loads = SaveNames::slot,
        .saveExt = ".dsg",
        .configFile = false,
        .deh = "",
        .bex = "",
        .exec = false,
        .map = false,
        .netgame = Netgames::none,
};

constexpr Port ZDOOM28{.loads = SaveNames::path};

constexpr Port UZDOOM{.netmode = false, .netgameSaves = true};

constexpr std::array WOOFS = {"woof", "nugget", "cherry"};
constexpr std::array BOOMS = {"prboom", "glboom", "rude"};

// DOS releases, matched by whole name.
constexpr std::array PLAIN = {"doom", "doom2", "doomu", "doom95", "dosdoom"};
constexpr std::array LEGACIES = {"doom3", "legacy", "doomlegacy"};

}

bool flatpakExport(const std::filesystem::path &program) {
    const std::filesystem::path folder = program.parent_path();

    return folder.filename() == "bin" && folder.parent_path().filename() == "exports"
           && folder.parent_path().parent_path().filename() == "flatpak";
}

Port of(const std::filesystem::path &program) {
    const bool exported = flatpakExport(program);

    // An export is named for its app ID, such as io.github.fabiangreffrath.woof, where only
    // the last part names the port. No DOS release comes as one.
    const std::string name = Text::lower(exported && program.has_extension() ? program.extension().string().substr(1)
                                                                             : program.stem().string());

    if (!exported && std::ranges::find(PLAIN, name) != PLAIN.end()) {
        return DOOM;
    }

    if (!exported && std::ranges::find(LEGACIES, name) != LEGACIES.end()) {
        return LEGACY;
    }

    if (name.contains("helion")) {
        return HELION;
    }

    // Whole name: gzdoom contains zdoom.
    if (name == "zdoom" || name.contains("zandronum")) {
        return ZDOOM28;
    }

    if (name.contains("uzdoom")) {
        return UZDOOM;
    }

    // Chocolate plus -levelstat, which it has written since 5.9.0.
    if (name.contains("crispy")) {
        Port crispy = VANILLA;
        crispy.configName = "crispy";
        crispy.levelstat = true;

        return crispy;
    }

    if (name.contains("chocolate")) {
        return VANILLA;
    }

    // Whole name: prboom contains boom. MBF and WinMBF are the same line, predating
    // -complevel and everything after it.
    if (name == "boom" || name.contains("mbf")) {
        Port boom = BOOM202;

        // MBF takes -config. Boom 2.02 keeps boom.cfg beside the exe.
        boom.configFile = name.contains("mbf");

        return boom;
    }

    if (name.contains("eternity")) {
        return ETERNITY;
    }

    // Nugget and Cherry write settings that Woof would drop.
    for (const char *each : WOOFS) {
        if (name.contains(each)) {
            Port woof = WOOF;
            woof.configName = each;

            return woof;
        }
    }

    if (name.contains("dsda")) {
        return DSDA;
    }

    for (const char *each : BOOMS) {
        if (name.contains(each)) {
            Port boom = BOOM;
            boom.configName = each;

            return boom;
        }
    }

    Port guessed = ZDOOM;
    guessed.recognised = false;

    return guessed;
}

std::vector<int> complevels(const Complevels which) {
    switch (which) {
        case Complevels::woof:
            return {-1, 2, 3, 4, 9, 11, 21, 24};
        case Complevels::prboom:
            return {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
        case Complevels::dsda:
            return {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 21};
        case Complevels::none:
            break;
    }

    return {};
}

DemoSupport demos(const Port &speaks) {
    return {
            .records = speaks.demos,
            .timed = speaks.demos && speaks.timedemo,
            .fast = speaks.demos && speaks.fastdemo,
            .complevel = speaks.demos ? speaks.complevel : Complevels::none,
            .longtics = speaks.demos && speaks.longtics,
            .soloNet = speaks.demos && speaks.soloNet,
    };
}

SaveSupport saves(const Port &speaks) {
    return {.names = speaks.loads, .folder = !speaks.save.empty()};
}

NetSupport net(const Port &speaks) {
    switch (speaks.netgame) {
        case Netgames::zdoom:
            return {
                    .hosts = true,
                    .joins = true,
                    .players = true,
                    .fragLimit = true,
                    .flags = true,
                    .savegame = true,
                    .netgameFolder = speaks.netgameSaves,
                    .extratic = true,
                    .netmode = speaks.netmode,
                    .dup = true,
            };
        case Netgames::chocolate:
            return {.hosts = true, .joins = true, .listing = true, .dup = true};

        case Netgames::prboom:
            return {.joins = true};
        case Netgames::none:
            break;
    }

    return {};
}

Port of(const Config &config) {
    return of(config, config.activeProfile());
}

Port of(const Config &config, const Profile &profile) {
    const PortEntry *entry = config.findPort(profile.port);

    if (entry == nullptr) {
        return {};
    }

    Port speaks = of(std::filesystem::path(entry->file));
    speaks.dos = entry->dosbox;

    if (!speaks.dos) {
        return speaks;
    }

    // Nothing running under DOSBox is ZDoom, so an unknown name is answered for as vanilla.
    if (!speaks.recognised) {
        speaks = DOOM;
        speaks.dos = true;
        speaks.recognised = false;
    }

    // Nothing after the last DOS release applies inside DOSBox.
    speaks.save = "";
    speaks.extraConfig = false;
    speaks.exec = false;
    speaks.map = false;
    speaks.netgame = Netgames::none;
    speaks.complevel = Complevels::none;
    speaks.longtics = false;
    speaks.soloNet = false;
    speaks.levelstat = false;

    return speaks;
}

}
