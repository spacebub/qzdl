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

#include <cctype>
#include <string>

#include "core/launch/Netgame.h"

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

void addHost(std::vector<std::string> &args, const MultiplayerSettings &mp,
             const Dialect::NetSupport &net, const std::filesystem::path &save) {
    if (mp.gameType == 2) {
        args.emplace_back("-deathmatch");
    } else if (mp.gameType == 3) {
        args.emplace_back("-altdeath");
    }

    if (net.flags && !mp.dmflags.empty()) {
        args.emplace_back("+set");
        args.emplace_back("dmflags");
        args.push_back(mp.dmflags);
    }

    if (net.flags && !mp.dmflags2.empty()) {
        args.emplace_back("+set");
        args.emplace_back("dmflags2");
        args.push_back(mp.dmflags2);
    }

    // -privateserver is -server without the master server listing.
    if (net.players) {
        args.emplace_back("-host");
        args.push_back(std::to_string(mp.players));
    } else {
        args.emplace_back(mp.listed ? "-server" : "-privateserver");
    }

    if (!mp.port.empty()) {
        args.emplace_back("-port");
        args.push_back(mp.port);
    }

    if (net.fragLimit && !mp.fragLimit.empty()) {
        args.emplace_back("+set");
        args.emplace_back("fraglimit");
        args.push_back(mp.fragLimit);
    }

    if (!mp.timeLimit.empty()) {
        if (net.flags) {
            args.emplace_back("+set");
            args.emplace_back("timelimit");
        } else {
            args.emplace_back("-timer");
        }

        args.push_back(mp.timeLimit);
    }

    // Ports read only the first -loadgame, and the profile's own save may already be on the line.
    if (net.savegame && !mp.savegame.empty() && save.empty()) {
        args.emplace_back("-loadgame");
        args.push_back(mp.savegame);
    }
}

void addJoin(std::vector<std::string> &args, const MultiplayerSettings &mp,
             const Dialect::Port &speaks) {
    std::string_view how = "-join";

    if (speaks.netgame == Dialect::Netgames::chocolate) {
        how = "-connect";
    } else if (speaks.netgame == Dialect::Netgames::prboom) {
        how = "-net";
    }

    args.emplace_back(how);

    if (!mp.port.empty()) {
        args.push_back(withoutPort(mp.host) + ":" + mp.port);
    } else {
        args.push_back(mp.host);
    }
}

void addTuning(std::vector<std::string> &args, const MultiplayerSettings &mp,
               const Dialect::NetSupport &net) {
    if (net.extratic && mp.extratic == 1) {
        args.emplace_back("-extratic");
    }

    if (net.netmode && mp.netmode != -1) {
        args.emplace_back("-netmode");
        args.push_back(std::to_string(mp.netmode));
    }

    if (net.dup && mp.dup != 0) {
        args.emplace_back("-dup");
        args.push_back(std::to_string(mp.dup));
    }
}

}

void arguments(std::vector<std::string> &args, const Profile &profile,
               const Dialect::Port &speaks, const std::filesystem::path &save) {
    const MultiplayerSettings &mp = profile.multiplayer;
    const Dialect::NetSupport net = Dialect::net(speaks);

    // A player count is what makes this side the host.
    const bool hosting = mp.players > 0;

    if (mp.gameType == 0 || !(hosting ? net.hosts : net.joins)) {
        return;
    }

    if (hosting) {
        addHost(args, mp, net, save);
    } else if (!mp.host.empty()) {
        addJoin(args, mp, speaks);
    }

    addTuning(args, mp, net);
}

}
