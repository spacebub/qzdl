/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
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

#include "core/config/Config.h"

namespace pgo {

// The sandbox furnished as a player's machine: a dozen games, eight source ports
// and a shelf of add-ons that are really there, so every reader opens real bytes
// and every launch runs a real process.
class World {
public:
    World();

    // A config over the world's files in the shape a real one has. Zero profiles is
    // a first run: no games and no ports either.
    [[nodiscard]] Config config(int profiles, int filesPerProfile) const;

    [[nodiscard]] const std::vector<NameEntry> &games() const { return _games; }
    [[nodiscard]] const std::vector<NameEntry> &ports() const { return _ports; }
    [[nodiscard]] const std::vector<std::string> &addons() const { return _addons; }
    [[nodiscard]] const std::filesystem::path &gamesDirectory() const { return _gamesDirectory; }

    // Rows of ports(). The chatty one prints like a game and leaves, the waiting one
    // holds until it is stopped, the DOS one runs under the fake DOSBox on PATH.
    static constexpr int CHATTY = 0;
    static constexpr int WAITING = 5;
    static constexpr int DOS = 7;

private:
    std::filesystem::path _gamesDirectory;
    std::vector<NameEntry> _games;
    std::vector<NameEntry> _ports;
    std::vector<std::string> _addons;
};

}
