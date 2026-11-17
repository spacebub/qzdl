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
#include <cctype>
#include <string>
#include <string_view>
#include <utility>

#include "ttk/system/Text.h"

#include "core/launch/Netgame.h"
#include "core/launch/Storage.h"

namespace Netgame {

using Dialect::Feature;

namespace {

std::string withoutPort(const std::string &host) {
    size_t end = host.size();

    while (end > 0 && std::isspace(static_cast<unsigned char>(host[end - 1])) != 0) {
        --end;
    }

    size_t at = end;

    while (at > 0 && std::isdigit(static_cast<unsigned char>(host[at - 1])) != 0) {
        --at;
    }

    return host.substr(0, at > 0 && host[at - 1] == ':' ? at - 1 : end);
}

std::filesystem::path addHost(std::vector<std::string> &args,
                              const MultiplayerSettings &mp,
                              const Dialect::Traits &traits,
                              const std::filesystem::path &save) {
    if (mp.gameType == GameType::Deathmatch || mp.gameType == GameType::TeamDeathmatch) {
        Dialect::add(args, traits, Feature::deathmatch);
    } else if (mp.gameType == GameType::AltDeathmatch) {
        Dialect::add(args, traits, Feature::altDeathmatch);
    } else if (mp.gameType == GameType::Deathmatch3) {
        Dialect::add(args, traits, traits.has(Feature::deathmatch3) ? Feature::deathmatch3 : Feature::deathmatch);
    }

    if (mp.gameType == GameType::TeamDeathmatch) {
        Dialect::add(args, traits, Feature::teams);
    }

    // -privateserver is -server without the master server listing.
    if (!Dialect::add(args, traits, Feature::hostPlayers, std::to_string(players(mp, traits)))) {
        Dialect::add(args, traits, mp.listed ? Feature::hostListed : Feature::hostUnlisted);
    }

    if (!mp.port.empty()) {
        Dialect::add(args, traits, Feature::netPort, mp.port);
    }

    if (!mp.fragLimit.empty()) {
        Dialect::add(args, traits, Feature::fragLimit, mp.fragLimit);
    }

    if (!mp.timeLimit.empty()) {
        Dialect::add(args, traits, Feature::timeLimit, mp.timeLimit);
    }

    // Ports read only the first -loadgame, and the profile's own save may already be on the line.
    if (!traits.has(Feature::savegame) || mp.savegame.empty() || !save.empty()) {
        return {};
    }

    std::string named = Storage::loadName(mp.savegame, traits.loads);

    return !named.empty() && Dialect::add(args, traits, Feature::loadgame, std::move(named))
                   ? std::filesystem::path(mp.savegame)
                   : std::filesystem::path();
}

// A value that starts with a dash or a plus would end the + command, so it goes in one argument with it.
void addCvar(std::vector<std::string> &args, const std::string &name, const std::string &value) {
    if (value.starts_with('-') || value.starts_with('+')) {
        std::string quoted;

        for (const char each : value) {
            quoted += each == '"' ? std::string("\\\"") : std::string(1, each);
        }

        args.push_back("+" + name + " \"" + quoted + "\"");

        return;
    }

    args.push_back("+" + name);
    args.push_back(value);
}

void addPlayer(std::vector<std::string> &args,
               const MultiplayerSettings &mp,
               const Dialect::Traits &traits,
               const bool hosting) {
    if (!traits.has(Feature::player)) {
        return;
    }

    if (!mp.playerName.empty()) {
        addCvar(args, "name", mp.playerName);
    }

    if (const std::string color = playerColor(mp.playerColor); !color.empty()) {
        addCvar(args, "color", color);

        if (traits.has(Feature::colorSet)) {
            addCvar(args, "colorset", "-1");
        }
    }

    // A joining player cannot know whether the host plays teams, so the pick goes along anyway.
    if (traits.has(Feature::teamPick) && mp.team >= 0 && (!hosting || mp.gameType == GameType::TeamDeathmatch)) {
        addCvar(args, "team", std::to_string(mp.team));
    }
}

void addTuning(std::vector<std::string> &args, const MultiplayerSettings &mp, const Dialect::Traits &traits) {
    if (mp.extratic) {
        Dialect::add(args, traits, Feature::extratic);
    }

    if (mp.netmode != -1) {
        Dialect::add(args, traits, Feature::netmode, std::to_string(mp.netmode));
    }

    if (mp.dup != 0) {
        Dialect::add(args, traits, Feature::dup, std::to_string(mp.dup));
    }
}

}

std::string playerColor(const std::string &typed) {
    const std::string trimmed = ttk::Text::trim(typed);
    const std::string_view digits = std::string_view(trimmed).substr(trimmed.starts_with('#') ? 1 : 0);

    if (digits.size() != 6 || !std::ranges::all_of(digits, [](const char each) {
            return std::isxdigit(static_cast<unsigned char>(each)) != 0;
        })) {
        return {};
    }

    return "#" + ttk::Text::lower(digits);
}

bool unsupported(const Profile &profile, const Dialect::Traits &traits) {
    const MultiplayerSettings &mp = profile.multiplayer;

    return mp.gameType != GameType::None && !traits.has(mp.players > 0 ? Feature::hosts : Feature::joins);
}

bool inPlay(const Profile &profile, const Dialect::Traits &traits) {
    const MultiplayerSettings &mp = profile.multiplayer;

    return mp.gameType != GameType::None && !unsupported(profile, traits) && (mp.players > 0 || !mp.host.empty());
}

bool joins(const Profile &profile, const Dialect::Traits &traits) {
    return inPlay(profile, traits) && profile.multiplayer.players <= 0;
}

int players(const MultiplayerSettings &mp, const Dialect::Traits &traits) {
    const Dialect::Range limit = Dialect::arg(traits, Feature::hostPlayers).range;

    return mp.players > 0 && limit.least < limit.most ? std::clamp(mp.players, limit.least, limit.most) : mp.players;
}

bool hostsServer(const Profile &profile, const Dialect::Traits &traits) {
    return traits.has(Feature::hostsServer) && inPlay(profile, traits) && profile.multiplayer.players > 0;
}

Dialect::Features leftOut(const Profile &profile, const Dialect::Traits &traits) {
    if (!traits.has(Feature::hostsServer) || !inPlay(profile, traits)) {
        return {};
    }

    return profile.multiplayer.players > 0 ? SERVER_LEAVES_OUT : CLIENT_LEAVES_OUT;
}

Dialect::Traits launched(const Profile &profile, Dialect::Traits traits) {
    traits.features &= ~leftOut(profile, traits);

    return traits;
}

bool readsNetgameFolder(const Profile &profile, const Dialect::Traits &traits) {
    // A host of one starts no network, so the port reads the ordinary save folder.
    return traits.has(Feature::netgameFolder) && inPlay(profile, traits) && profile.multiplayer.players != 1;
}

std::filesystem::path arguments(std::vector<std::string> &args,
                                const Profile &profile,
                                const Dialect::Traits &asked,
                                const std::filesystem::path &save) {
    const MultiplayerSettings &mp = profile.multiplayer;
    const Dialect::Traits traits = launched(profile, asked);

    // A player count is what makes this side the host.
    const bool hosting = mp.players > 0;

    if (!inPlay(profile, traits)) {
        return {};
    }

    std::filesystem::path loaded;

    if (hosting) {
        loaded = addHost(args, mp, traits, save);
    } else {
        Dialect::add(args, traits, Feature::joins, mp.port.empty() ? mp.host : withoutPort(mp.host) + ":" + mp.port);
    }

    addPlayer(args, mp, traits, hosting);
    addTuning(args, mp, traits);

    return loaded;
}

}
