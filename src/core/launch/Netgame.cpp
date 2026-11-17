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

void addHost(std::vector<std::string> &args,
             const MultiplayerSettings &mp,
             const Dialect::Port &port,
             const std::filesystem::path &save) {
    if (mp.gameType == GameType::Deathmatch || mp.gameType == GameType::TeamDeathmatch) {
        Dialect::add(args, port, Feature::deathmatch);
    } else if (mp.gameType == GameType::AltDeathmatch) {
        Dialect::add(args, port, Feature::altDeathmatch);
    } else if (mp.gameType == GameType::Deathmatch3) {
        Dialect::add(args, port, port.has(Feature::deathmatch3) ? Feature::deathmatch3 : Feature::deathmatch);
    }

    if (mp.gameType == GameType::TeamDeathmatch) {
        Dialect::add(args, port, Feature::teams);
    }

    // -privateserver is -server without the master server listing.
    if (!Dialect::add(args, port, Feature::players, std::to_string(players(mp, port)))) {
        Dialect::add(args, port, mp.listed ? Feature::server : Feature::privateServer);
    }

    if (!mp.port.empty()) {
        Dialect::add(args, port, Feature::netPort, mp.port);
    }

    if (!mp.fragLimit.empty()) {
        Dialect::add(args, port, Feature::fragLimit, mp.fragLimit);
    }

    if (!mp.timeLimit.empty()) {
        Dialect::add(args, port, Feature::timeLimit, mp.timeLimit);
    }

    // Ports read only the first -loadgame, and the profile's own save may already be on the line.
    if (!port.has(Feature::savegame) || mp.savegame.empty() || !save.empty()) {
        return;
    }

    if (std::string named = Storage::loadName(mp.savegame, port.loads); !named.empty()) {
        Dialect::add(args, port, Feature::loadgame, std::move(named));
    }
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
               const Dialect::Port &port,
               const bool hosting) {
    if (!port.has(Feature::player)) {
        return;
    }

    if (!mp.playerName.empty()) {
        addCvar(args, "name", mp.playerName);
    }

    if (const std::string color = playerColor(mp.playerColor); !color.empty()) {
        addCvar(args, "color", color);

        if (port.has(Feature::colorSet)) {
            addCvar(args, "colorset", "-1");
        }
    }

    // A joining player cannot know whether the host plays teams, so the pick goes along anyway.
    if (port.has(Feature::teamPick) && mp.team >= 0 && (!hosting || mp.gameType == GameType::TeamDeathmatch)) {
        addCvar(args, "team", std::to_string(mp.team));
    }
}

void addTuning(std::vector<std::string> &args, const MultiplayerSettings &mp, const Dialect::Port &port) {
    if (mp.extratic) {
        Dialect::add(args, port, Feature::extratic);
    }

    if (mp.netmode != -1) {
        Dialect::add(args, port, Feature::netmode, std::to_string(mp.netmode));
    }

    if (mp.dup != 0) {
        Dialect::add(args, port, Feature::dup, std::to_string(mp.dup));
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

bool unsupported(const Profile &profile, const Dialect::Port &port) {
    const MultiplayerSettings &mp = profile.multiplayer;

    return mp.gameType != GameType::None && !port.has(mp.players > 0 ? Feature::hosts : Feature::joins);
}

bool inPlay(const Profile &profile, const Dialect::Port &port) {
    const MultiplayerSettings &mp = profile.multiplayer;

    return mp.gameType != GameType::None && !unsupported(profile, port) && (mp.players > 0 || !mp.host.empty());
}

bool joins(const Profile &profile, const Dialect::Port &port) {
    return inPlay(profile, port) && profile.multiplayer.players <= 0;
}

int players(const MultiplayerSettings &mp, const Dialect::Port &port) {
    const Dialect::Range limit = Dialect::arg(port, Feature::players).range;

    return mp.players > 0 && limit.least < limit.most ? std::clamp(mp.players, limit.least, limit.most) : mp.players;
}

bool hostsServer(const Profile &profile, const Dialect::Port &port) {
    return port.has(Feature::hostsServer) && inPlay(profile, port) && profile.multiplayer.players > 0;
}

Dialect::Features leftOut(const Profile &profile, const Dialect::Port &port) {
    if (!port.has(Feature::hostsServer) || !inPlay(profile, port)) {
        return {};
    }

    return profile.multiplayer.players > 0 ? SERVER_LEAVES_OUT : CLIENT_LEAVES_OUT;
}

Dialect::Port launched(const Profile &profile, Dialect::Port port) {
    port.features &= ~leftOut(profile, port);

    return port;
}

bool readsNetgameFolder(const Profile &profile, const Dialect::Port &port) {
    // A host of one starts no network, so the port reads the ordinary save folder.
    return port.has(Feature::netgameFolder) && inPlay(profile, port) && profile.multiplayer.players != 1;
}

void arguments(std::vector<std::string> &args,
               const Profile &profile,
               const Dialect::Port &asked,
               const std::filesystem::path &save) {
    const MultiplayerSettings &mp = profile.multiplayer;
    const Dialect::Port port = launched(profile, asked);

    // A player count is what makes this side the host.
    const bool hosting = mp.players > 0;

    if (!inPlay(profile, port)) {
        return;
    }

    if (hosting) {
        addHost(args, mp, port, save);
    } else {
        Dialect::add(args, port, Feature::joins, mp.port.empty() ? mp.host : withoutPort(mp.host) + ":" + mp.port);
    }

    addPlayer(args, mp, port, hosting);
    addTuning(args, mp, port);
}

}
