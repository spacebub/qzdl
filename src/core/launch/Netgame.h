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
[[nodiscard]] bool unsupported(const Profile &profile, const Dialect::Port &port);

// Whether the launch starts or joins a netgame at all, as the port speaks.
[[nodiscard]] bool inPlay(const Profile &profile, const Dialect::Port &port);

// Whether the launch joins a netgame that another player hosts.
[[nodiscard]] bool joins(const Profile &profile, const Dialect::Port &port);

// Whether the launch runs a netgame on a port that then reads saves only from its netgame folder.
[[nodiscard]] bool readsNetgameFolder(const Profile &profile, const Dialect::Port &port);

void arguments(std::vector<std::string> &args,
               const Profile &profile,
               const Dialect::Port &port,
               const std::filesystem::path &save);

}
