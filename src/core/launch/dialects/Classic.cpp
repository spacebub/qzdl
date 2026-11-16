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
#include <span>
#include <string_view>

#include "core/launch/dialects/Dialects.h"

namespace Dialect::dialects {

namespace {

constexpr std::array BOOM_CHANGES = std::to_array<Spelling>({
        {saveFolder, {"-save"}},
        // MBF skips its preloaded files. Boom has none, and passes over a switch it does not know.
        {noAutoload, {"-noload"}},
});

constexpr Port BOOM_PORT = defined({
        .features = COMMON | recognised | iwad | saveFolder | deh | bex | noAutoload | nosfx | respawn | loadgame
                    | demos | timedemo | fastdemo | recordAddsLmp | nomusic | displayKept,
        .saveExt = ".dsg",
        .configName = "mbf",
        .changes = BOOM_CHANGES,
        .id = Id::boom,
        .loads = SaveNames::slot,
        .netgame = Netgames::none,
        .line = PortLine::none,
});

// MBF takes -config. Boom 2.02 keeps boom.cfg beside the exe.
constexpr Port MBF_PORT = [] {
    Port mbf = BOOM_PORT;
    mbf.features |= configFile;

    return defined(mbf);
}();

constexpr std::array LEGACY_CHANGES = std::to_array<Spelling>({
        {deh, {"-dehacked"}},
        {bex, {"-dehacked"}},
        // Its -nosound leaves the music on.
        {nosfx, {"-nosound"}},
});

// Takes -config but writes to it without ever reading it back, so it keeps
// $DOOMWADDIR/config.cfg instead.
constexpr Port LEGACY_PORT = defined({
        .features = COMMON | recognised | deh | bex | nosfx | respawn | loadgame | demos | timedemo | recordAddsLmp
                    | nomusic | displayKept,
        .saveExt = ".dsg",
        .changes = LEGACY_CHANGES,
        .id = Id::legacy,
        .loads = SaveNames::slot,
        .netgame = Netgames::none,
        .line = PortLine::none,
});

constexpr Port DOOM_PORT = defined({
        .features = COMMON | recognised | nosfx | respawn | loadgame | demos | timedemo | recordAddsLmp | nomusic
                    | displayKept,
        .saveExt = ".dsg",
        .id = Id::doom,
        .loads = SaveNames::slot,
        .netgame = Netgames::none,
        .line = PortLine::none,
});

// DOS releases, matched by whole name.
constexpr std::array PLAIN = {"doom", "doom2", "doomu", "doom95", "dosdoom"};
constexpr std::array LEGACIES = {"doom3", "legacy", "doomlegacy"};

bool isAny(const std::string_view name, const std::span<const char *const> words) {
    return std::ranges::find(words, name) != words.end();
}

// Whole name: prboom contains boom. MBF and WinMBF are the same line, predating
// -complevel and everything after it.
std::optional<Port> boomNamed(const Name &name) {
    if (name.text == "boom") {
        return BOOM_PORT;
    }

    if (name.text.contains("mbf")) {
        return MBF_PORT;
    }

    return std::nullopt;
}

std::optional<Port> legacyNamed(const Name &name) {
    if (!name.exported && isAny(name.text, LEGACIES)) {
        return LEGACY_PORT;
    }

    return std::nullopt;
}

std::optional<Port> doomNamed(const Name &name) {
    if (!name.exported && isAny(name.text, PLAIN)) {
        return DOOM_PORT;
    }

    return std::nullopt;
}

}

constexpr Definition BOOM{
        .id = Id::boom,
        .bare = BOOM_PORT,
        .named = boomNamed,
        .releases = {},
        .levels = {},
        .levelWord = nullptr,
};

constexpr Definition LEGACY{
        .id = Id::legacy,
        .bare = LEGACY_PORT,
        .named = legacyNamed,
        .releases = {},
        .levels = {},
        .levelWord = nullptr,
};

constexpr Definition DOOM{
        .id = Id::doom,
        .bare = DOOM_PORT,
        .named = doomNamed,
        .releases = {},
        .levels = {},
        .levelWord = nullptr,
};

}
