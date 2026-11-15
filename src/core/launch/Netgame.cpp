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
        args.emplace_back("-deathmatch");
    } else if (mp.gameType == GameType::AltDeathmatch) {
        args.emplace_back("-altdeath");
    } else if (mp.gameType == GameType::Deathmatch3) {
        args.emplace_back(port.deathmatch3.empty() ? std::string_view("-deathmatch") : port.deathmatch3);
    }

    if (mp.gameType == GameType::TeamDeathmatch && port.has(Dialect::Feature::teams)) {
        args.emplace_back("+teamplay");
        args.emplace_back("1");
    }

    // -privateserver is -server without the master server listing.
    if (port.has(Dialect::Feature::players)) {
        args.emplace_back("-host");
        args.push_back(std::to_string(mp.players));
    } else {
        args.emplace_back(mp.listed ? "-server" : "-privateserver");
    }

    if (!mp.port.empty()) {
        args.emplace_back("-port");
        args.push_back(mp.port);
    }

    if (port.has(Dialect::Feature::fragLimit) && !mp.fragLimit.empty()) {
        args.emplace_back("+set");
        args.emplace_back("fraglimit");
        args.push_back(mp.fragLimit);
    }

    if (!mp.timeLimit.empty()) {
        if (port.has(Dialect::Feature::timeLimitCvar)) {
            args.emplace_back("+set");
            args.emplace_back("timelimit");
        } else {
            args.emplace_back("-timer");
        }

        args.push_back(mp.timeLimit);
    }

    // Ports read only the first -loadgame, and the profile's own save may already be on the line.
    if (!port.has(Dialect::Feature::savegame) || mp.savegame.empty() || !save.empty()) {
        return;
    }

    if (std::string named = Storage::loadName(mp.savegame, port.loads); !named.empty()) {
        args.emplace_back("-loadgame");
        args.push_back(std::move(named));
    }
}

void addJoin(std::vector<std::string> &args, const MultiplayerSettings &mp, const Dialect::Port &port) {
    std::string_view how = "-join";

    if (port.netgame == Dialect::Netgames::chocolate) {
        how = "-connect";
    } else if (port.netgame == Dialect::Netgames::prboom) {
        how = "-net";
    }

    args.emplace_back(how);

    if (!mp.port.empty()) {
        args.push_back(withoutPort(mp.host) + ":" + mp.port);
    } else {
        args.push_back(mp.host);
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
    if (!port.has(Dialect::Feature::player)) {
        return;
    }

    if (!mp.playerName.empty()) {
        addCvar(args, "name", mp.playerName);
    }

    // The default color set would win over the color.
    if (const std::string color = playerColor(mp.playerColor); !color.empty()) {
        addCvar(args, "color", color);
        addCvar(args, "colorset", "-1");
    }

    // A joining player cannot know whether the host plays teams, so the pick goes along anyway.
    if (port.has(Dialect::Feature::teamPick) && mp.team >= 0 && (!hosting || mp.gameType == GameType::TeamDeathmatch)) {
        addCvar(args, "team", std::to_string(mp.team));
    }
}

void addTuning(std::vector<std::string> &args, const MultiplayerSettings &mp, const Dialect::Port &port) {
    if (port.has(Dialect::Feature::extratic) && mp.extratic) {
        args.emplace_back("-extratic");
    }

    if (port.has(Dialect::Feature::netmode) && mp.netmode != -1) {
        args.emplace_back("-netmode");
        args.push_back(std::to_string(mp.netmode));
    }

    if (port.has(Dialect::Feature::dup) && mp.dup != 0) {
        args.emplace_back("-dup");
        args.push_back(std::to_string(mp.dup));
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

bool inPlay(const Profile &profile, const Dialect::Port &port) {
    const MultiplayerSettings &mp = profile.multiplayer;

    if (mp.gameType == GameType::None) {
        return false;
    }

    return mp.players > 0 ? port.has(Dialect::Feature::hosts) : port.has(Dialect::Feature::joins) && !mp.host.empty();
}

bool joins(const Profile &profile, const Dialect::Port &port) {
    return inPlay(profile, port) && profile.multiplayer.players <= 0;
}

bool readsNetgameFolder(const Profile &profile, const Dialect::Port &port) {
    const MultiplayerSettings &mp = profile.multiplayer;

    if (!port.has(Dialect::Feature::netgameFolder) || mp.gameType == GameType::None) {
        return false;
    }

    // A host of one starts no network, so the port reads the ordinary save folder.
    return mp.players > 0 ? mp.players > 1 && port.has(Dialect::Feature::hosts)
                          : port.has(Dialect::Feature::joins) && !mp.host.empty();
}

void arguments(std::vector<std::string> &args,
               const Profile &profile,
               const Dialect::Port &port,
               const std::filesystem::path &save) {
    const MultiplayerSettings &mp = profile.multiplayer;

    // A player count is what makes this side the host.
    const bool hosting = mp.players > 0;

    if (!inPlay(profile, port)) {
        return;
    }

    if (hosting) {
        addHost(args, mp, port, save);
    } else {
        addJoin(args, mp, port);
    }

    addPlayer(args, mp, port, hosting);
    addTuning(args, mp, port);
}

}
