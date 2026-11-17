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

#include <filesystem>
#include <string>
#include <vector>

#include "core/config/Profile.h"
#include "core/launch/Dialect.h"

namespace Netgame {

// #rrggbb in lowercase, from six hex digits with or without the #. Empty for anything else.
[[nodiscard]] std::string playerColor(const std::string &typed);

// The profile hosts or joins, and the port does not take that side.
[[nodiscard]] bool unsupported(const Profile &profile, const Dialect::Traits &traits);

// Whether the launch starts or joins a netgame at all, as the port speaks.
[[nodiscard]] bool inPlay(const Profile &profile, const Dialect::Traits &traits);

// Whether the launch joins a netgame that another player hosts.
[[nodiscard]] bool joins(const Profile &profile, const Dialect::Traits &traits);

// A server loads no save, records and plays back nothing, and has no player of its own.
inline constexpr Dialect::Features SERVER_LEAVES_OUT =
        Dialect::Feature::loadgame | Dialect::Feature::savegame | Dialect::Feature::demos | Dialect::Feature::record
        | Dialect::Feature::playdemo | Dialect::Feature::timedemo | Dialect::Feature::player;

// A client handed a map or a skill starts a game of its own while it connects.
inline constexpr Dialect::Features CLIENT_LEAVES_OUT =
        Dialect::Feature::warp | Dialect::Feature::map | Dialect::Feature::skill;

// The count a host is opened for, inside the player limit of the port. A count kept from another port may be above
// it. 0 and below, which join, stay as they are.
[[nodiscard]] int players(const MultiplayerSettings &mp, const Dialect::Traits &traits);

// Whether the launch hosts on a port that starts a server, which plays no game itself.
[[nodiscard]] bool hostsServer(const Profile &profile, const Dialect::Traits &traits);

// What the launch leaves out on a port that hosts a server, as the server or as a client of one. Empty otherwise.
[[nodiscard]] Dialect::Features leftOut(const Profile &profile, const Dialect::Traits &traits);

// The traits without what the launch leaves out.
[[nodiscard]] Dialect::Traits launched(const Profile &profile, Dialect::Traits traits);

// Whether the launch runs a netgame on a port that then reads saves only from its netgame folder.
[[nodiscard]] bool readsNetgameFolder(const Profile &profile, const Dialect::Traits &traits);

// What the launch adds for the netgame. Anything the port leaves out as a server or client is left off.
void arguments(std::vector<std::string> &args,
               const Profile &profile,
               const Dialect::Traits &asked,
               const std::filesystem::path &save);

}
