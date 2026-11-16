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

#include <array>

#include "core/launch/dialects/Dialects.h"

namespace Dialect::dialects {

namespace {

constexpr Features ZDOOM_FAMILY =
        Feature::recognised | Feature::iwad | Feature::configFile | Feature::exec | Feature::map | Feature::respawn
        | Feature::loadgame | Feature::demos | Feature::timedemo | Feature::recordAddsLmp | Feature::cheats
        | Feature::nomusic | Feature::displayKept | Feature::shotdir | Feature::alwaysApplyDmflags | Feature::turbo;

// The GZDoom line takes pistol start as a cvar, which no demo or netgame refuses.
constexpr std::array GZDOOM_CHANGES = std::to_array<Change>({
        {Feature::pistolStart, {"+sv_pistolstart", "Starts every map with only the pistol.", "1"}},
});

constexpr Port GZDOOM_PORT = defined({
        .features = ZDOOM_FAMILY | Feature::netmode | Feature::teamPick | Feature::pistolStart,
        .changes = GZDOOM_CHANGES,
        .id = Id::gzdoom,
});

constexpr Port UZDOOM_PORT = defined({
        .features =
                ZDOOM_FAMILY | Feature::teamPick | Feature::pistolStart | Feature::netgameFolder | Feature::coopPreset,
        .changes = GZDOOM_CHANGES,
        .id = Id::uzdoom,
        .line = PortLine::uzdoom,
});

constexpr Port ZDOOM_PORT = defined({
        .features = ZDOOM_FAMILY | Feature::netmode | Feature::teamPick,
        .id = Id::zdoom,
        .loads = SaveNames::path,
        .display = Windowing::oldCvar,
        .line = PortLine::zdoom,
});

// ZDoom 2.8 underneath, with flag words of its own and +team only once a client is in.
constexpr Port ZANDRONUM_PORT = defined({
        .features = ZDOOM_FAMILY | Feature::netmode,
        .id = Id::zandronum,
        .loads = SaveNames::path,
        .display = Windowing::oldCvar,
        .line = PortLine::zandronum,
});

std::optional<Port> gzdoomNamed(const Name &name) {
    if (name.text.contains("zdoom") || name.text.contains("vkdoom")) {
        return GZDOOM_PORT;
    }

    return std::nullopt;
}

std::optional<Port> uzdoomNamed(const Name &name) {
    if (name.text.contains("uzdoom")) {
        return UZDOOM_PORT;
    }

    return std::nullopt;
}

// Whole name: gzdoom contains zdoom.
std::optional<Port> zdoomNamed(const Name &name) {
    if (name.text == "zdoom") {
        return ZDOOM_PORT;
    }

    return std::nullopt;
}

std::optional<Port> zandronumNamed(const Name &name) {
    if (name.text.contains("zandronum") || name.text.contains("skulltag")) {
        return ZANDRONUM_PORT;
    }

    return std::nullopt;
}

}

constexpr Definition GZDOOM{
        .id = Id::gzdoom,
        .bare = GZDOOM_PORT,
        .named = gzdoomNamed,
        .releases = {},
        .levels = {},
        .levelSwitch = "-complevel",
        .levelWord = nullptr,
};

constexpr Definition UZDOOM{
        .id = Id::uzdoom,
        .bare = UZDOOM_PORT,
        .named = uzdoomNamed,
        .releases = {},
        .levels = {},
        .levelSwitch = "-complevel",
        .levelWord = nullptr,
};

constexpr Definition ZDOOM{
        .id = Id::zdoom,
        .bare = ZDOOM_PORT,
        .named = zdoomNamed,
        .releases = {},
        .levels = {},
        .levelSwitch = "-complevel",
        .levelWord = nullptr,
};

constexpr Definition ZANDRONUM{
        .id = Id::zandronum,
        .bare = ZANDRONUM_PORT,
        .named = zandronumNamed,
        .releases = {},
        .levels = {},
        .levelSwitch = "-complevel",
        .levelWord = nullptr,
};

}
