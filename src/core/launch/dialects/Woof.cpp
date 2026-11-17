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
#include <span>
#include <string_view>

#include "core/launch/dialects/Dialects.h"

namespace Dialect::dialects {

namespace {

// -1 first, the port's own.
constexpr std::array WOOF_LEVELS = {-1, 2, 3, 4, 9, MBF, MBF21, ID24};

// NOLINTBEGIN(readability-trailing-comma): a change is one row, with its description inside it
constexpr std::array WOOF_OWN = std::to_array<Spelling>({
        {saveFolder, {"-save"}},
        {gameSpeed,
         {"-speed",
          "Game speed in percent, from 10 to 1000. Strict mode and netgames keep it at 100.",
          {},
          Range{.least = 10, .most = 1000}}},
        {dogs,
         {"-dogs",
          "How many helper dogs join a single player game, from 0 to 3. Left out below the MBF complevel, since the "
          "port stops there.",
          {},
          Range{.least = 0, .most = 3},
          PLAYBACK,
          Levels{.from = MBF, .to = AND_UP, .onLine = false}}},
});
// NOLINTEND(readability-trailing-comma)

constexpr std::array WOOF_CHANGES = join(CHOCOLATE_CHANGES, WOOF_OWN);

// The port stops on a recording without a complevel or a skill.
constexpr Features RECORDING_RULES = recordsWithComplevel | recordsWithSkill | continueComplevel;

// -loadgame takes a page and a slot as two digits.
constexpr int LAST_PAGED_SLOT = 77;

constexpr void beforeId24(Traits &traits) {
    traits.complevel = Complevels::woofToMbf21;
}

constexpr void withoutAutosave(Traits &traits) {
    traits.autosave = {};
    traits.lastSlot = Traits{}.lastSlot;
}

constexpr void beforeAutosave(Traits &traits) {
    beforeId24(traits);
    withoutAutosave(traits);
}

constexpr void beforeChocolateNetgame(Traits &traits) {
    beforeAutosave(traits);
    traits.netgame = Netgames::none;
    traits.features &= ~NETGAME_CHOCOLATE;
}

// What a span of Woof releases lacks against the latest. Nugget Doom releases share the era of the Woof they are
// built on.
struct Era {
    Features lacks;
    void (*special)(Traits &traits){nullptr};
};

constexpr Era NEWEST{.lacks = {}, .special = nullptr};

// id24 and the recording rules are read from 16.0.0.
constexpr Era WOOF_15{.lacks = RECORDING_RULES, .special = beforeId24};

// From 15.0.0 -loadgame takes the autosave as slot 255 and no slot above 77.
constexpr Era WOOF_10_2{.lacks = WOOF_15.lacks, .special = beforeAutosave};

// -recordfromto is read from 10.2.0.
constexpr Era WOOF_10{.lacks = WOOF_10_2.lacks | continues, .special = beforeAutosave};

// -dm3 and -shotdir are read from 10.0.0.
constexpr Era WOOF_9{.lacks = WOOF_10.lacks | deathmatch3 | shotdir, .special = beforeAutosave};

// The Chocolate netgame, -coop_spawns and -solo-net are read from 9.0.0.
constexpr Era WOOF_8_1{.lacks = WOOF_9.lacks | coopSpawns | soloNet, .special = beforeChocolateNetgame};

// -noautoload is read from 8.1.0.
constexpr Era WOOF_6_1{.lacks = WOOF_8_1.lacks | noAutoload, .special = beforeChocolateNetgame};

// -loadgame takes the autosave as slot 255 and no slot above 77 from Cherry Doom 2.1.0.
constexpr Era CHERRY_2{.lacks = {}, .special = withoutAutosave};

constexpr VersionRange row(const std::string_view first, const std::string_view label, const Era &era) {
    return {.first = first, .label = label, .lacks = era.lacks, .gains = {}, .changes = {}, .special = era.special};
}

}

constexpr Traits WOOF_TRAITS = defined({
        .features = COMMON | recognised | iwad | configFile | saveFolder | deh | bex | noAutoload | nosfx | fullscreen
                    | windowed | deathmatch3 | respawn | longWarp | loadgame | demos | timedemo | fastdemo
                    | RECORDING_RULES | longtics | soloNet | recordAddsLmp | continues | levelstat | pistolStart
                    | nomusic | displayKept | shotdir | coopSpawns | noCheats | noOptions | mbfBeta | turbo | gameSpeed
                    | dogs,
        .saveExt = ".dsg",
        .autosave = "autosave.dsg",
        .configName = "woof",
        .changes = WOOF_CHANGES,
        .lastSlot = LAST_PAGED_SLOT,
        .unrecordable = ID24,
        .dialect = Dialect::woof,
        .loads = SaveNames::slot,
        .netgame = Netgames::chocolate,
        .complevel = Complevels::woof,
        .line = PortLine::woof,
});

// Nugget and Cherry write settings that Woof would drop.
constexpr Traits NUGGET_TRAITS = [] {
    Traits nugget = withConfig(WOOF_TRAITS, "nugget");
    nugget.features |= savegameDir;

    return nugget;
}();

// Cherry Doom offers no id24 and sets no rules for recording.
constexpr Traits CHERRY_TRAITS = [] {
    Traits cherry = withConfig(NUGGET_TRAITS, "cherry");
    cherry.features &= ~RECORDING_RULES;
    beforeId24(cherry);

    return cherry;
}();

// Each row starts at the first release it describes. An older release runs as the oldest row.
constexpr std::array WOOF_RANGES = std::to_array<VersionRange>({
        row("16.0.0", "16.0.0 and later", NEWEST),
        row("15.0.0", "15.0.0 to 15.3.0", WOOF_15),
        row("10.2.0", "10.2.0 to 14.5.0", WOOF_10_2),
        row("10.0.0", "10.0.0 to 10.1.0", WOOF_10),
        row("9.0.0", "9.0.0", WOOF_9),
        row("8.1.0", "8.1.0", WOOF_8_1),
        row("6.1.0", "6.1.0 to 8.0.0", WOOF_6_1),
});

constexpr Releases WOOF_RELEASES(WOOF_TRAITS, WOOF_RANGES);

constexpr std::array NUGGET_RANGES = std::to_array<VersionRange>({
        row("6.0.0", "6.0.0 and later", NEWEST),
        row("4.0.0", "4.0.0 to 5.1.0", WOOF_15),
        row("1.10.0", "1.10.0 to 3.3.0", WOOF_10_2),
        row("1.9.0", "1.9.0 to 1.9.1", WOOF_10),
        row("1.8.0", "1.8.0", WOOF_9),
        row("1.6.1", "1.6.1 to 1.7.0", WOOF_8_1),
        row("1.0.0", "1.0.0 to 1.6.0", WOOF_6_1),
});

constexpr Releases NUGGET_RELEASES(NUGGET_TRAITS, NUGGET_RANGES);

constexpr std::array CHERRY_RANGES = std::to_array<VersionRange>({
        row("2.1.0", "2.1.0 and later", NEWEST),
        row("1.0.0", "1.0.0 to 2.0.0", CHERRY_2),
});

constexpr Releases CHERRY_RELEASES(CHERRY_TRAITS, CHERRY_RANGES);

static_assert(WOOF_LEVELS.back() == ID24, "the levels up to MBF21 leave out the last");

constexpr Numbering WOOF_NUMBERING{.levels = WOOF_LEVELS, .levelWord = nullptr};

constexpr Numbering WOOF_TO_MBF21_NUMBERING{
        .levels = std::span(WOOF_LEVELS).first(WOOF_LEVELS.size() - 1),
        .levelWord = nullptr,
};

}
