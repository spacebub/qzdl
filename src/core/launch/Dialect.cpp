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

#include "core/launch/Dialect.h"
#include "core/util/Text.h"

namespace Dialect {

Port of(const std::filesystem::path &program) {
    static constexpr Port ZDOOM{};
    static constexpr Port BOOM{
        .save = "-save", .loads = SaveNames::slot, .saveExt = ".dsg",
        .bex = "-deh", .exec = false, .map = false, .netgame = Netgames::prboom,
        .fastdemo = true, .complevel = Complevels::prboom,
        .longtics = true, .soloNet = true, .levelstat = true};

    // No -loadgame, and it stops on a switch it does not know.
    static constexpr Port DSDA{
        .save = "-save", .loads = SaveNames::none, .saveExt = ".dsg",
        .bex = "-deh", .exec = false, .map = false, .netgame = Netgames::none,
        .fastdemo = true, .complevel = Complevels::dsda,
        .longtics = true, .soloNet = true, .levelstat = true};

    static constexpr Port WOOF{
        .save = "-save", .loads = SaveNames::slot, .saveExt = ".dsg",
        .bex = "-deh", .exec = false, .map = false, .netgame = Netgames::chocolate,
        .fastdemo = true, .complevel = Complevels::woof,
        .longtics = true, .soloNet = true, .levelstat = true};

    static constexpr Port ETERNITY{
        .save = "-save", .loads = SaveNames::slot, .saveExt = ".dsg", .exec = false,
        .map = false, .netgame = Netgames::none, .fastdemo = true, .soloNet = true};

    static constexpr Port BOOM202{
        .save = "-save", .loads = SaveNames::slot, .saveExt = ".dsg", .bex = "-deh",
        .exec = false, .map = false, .netgame = Netgames::none, .fastdemo = true};
    static constexpr Port VANILLA{
        .loads = SaveNames::slot, .saveExt = ".dsg", .extraConfig = true,
        .bex = "-deh", .exec = false, .map = false, .netgame = Netgames::chocolate,
        .longtics = true, .soloNet = true};
    static constexpr Port HELION{
        .loads = SaveNames::name, .saveExt = ".hsg", .bex = "-deh",
        .exec = false, .respawn = false, .netgame = Netgames::none, .timedemo = false,
        .soloNet = true};
    static constexpr Port LEGACY{
        .iwad = false, .save = "", .loads = SaveNames::slot, .saveExt = ".dsg",
        .deh = "-dehacked", .bex = "-dehacked", .exec = false, .map = false,
        .netgame = Netgames::none};
    static constexpr Port DOOM{
        .iwad = false, .save = "", .loads = SaveNames::slot, .saveExt = ".dsg",
        .deh = "", .bex = "", .exec = false, .map = false, .netgame = Netgames::none};

    const std::string name = Text::lower(program.stem().string());

    // DOS releases, matched by whole name.
    static constexpr std::array PLAIN = {"doom", "doom2", "doomu", "doom95", "dosdoom"};
    static constexpr std::array LEGACIES = {"doom3", "legacy", "doomlegacy"};

    if (std::ranges::find(PLAIN, name) != PLAIN.end()) {
        return DOOM;
    }

    if (std::ranges::find(LEGACIES, name) != LEGACIES.end()) {
        return LEGACY;
    }

    if (name.contains("helion")) {
        return HELION;
    }

    // Whole name: gzdoom contains zdoom.
    static constexpr Port ZDOOM28{.loads = SaveNames::path};

    if (name == "zdoom" || name.contains("zandronum")) {
        return ZDOOM28;
    }

    static constexpr Port UZDOOM{.netmode = false};

    if (name.contains("uzdoom")) {
        return UZDOOM;
    }

    // Chocolate plus -levelstat, which it has written since 5.9.0.
    if (name.contains("crispy")) {
        Port crispy = VANILLA;
        crispy.levelstat = true;

        return crispy;
    }

    if (name.contains("chocolate")) {
        return VANILLA;
    }

    // Whole name: prboom contains boom. MBF and WinMBF are the same line, predating
    // -complevel and everything after it.
    if (name == "boom" || name.contains("mbf")) {
        return BOOM202;
    }

    if (name.contains("eternity")) {
        return ETERNITY;
    }

    static constexpr std::array WOOFS = {"woof", "nugget", "cherry"};

    for (const char *each : WOOFS) {
        if (name.contains(each)) {
            return WOOF;
        }
    }

    if (name.contains("dsda")) {
        return DSDA;
    }

    static constexpr std::array BOOMS = {"prboom", "glboom", "rude"};

    for (const char *each : BOOMS) {
        if (name.contains(each)) {
            return BOOM;
        }
    }

    return ZDOOM;
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
            return {.hosts = true, .joins = true, .players = true, .fragLimit = true,
                    .flags = true, .savegame = true, .extratic = true,
                    .netmode = speaks.netmode, .dup = true};
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
    const NameEntry *entry = config.findPort(profile.port);

    if (entry == nullptr) {
        return {};
    }

    Port speaks = of(std::filesystem::path(entry->file));
    speaks.dos = entry->dosbox;

    if (!speaks.dos) {
        return speaks;
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
