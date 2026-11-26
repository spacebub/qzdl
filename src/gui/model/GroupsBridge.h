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

#include <cstdint>
#include <string>
#include <vector>

#include "gui/model/Bridge.h"

// The groups of the profiles, the games and the files of the active profile.
class GroupsBridge : public Bridge {
public:
    enum class List : std::uint8_t {
        Profiles,
        Games,
        Files,
    };

    using Bridge::Bridge;

    void add(List list, const std::string &name) const;

    // A blank name is ignored.
    void rename(List list, std::uint16_t id, const std::string &name) const;

    // Up for -1, down for 1.
    void move(List list, std::uint16_t id, int by) const;

    // The members become ungrouped.
    void remove(List list, std::uint16_t id) const;
    void fold(List list, std::uint16_t id, bool folded) const;

private:
    [[nodiscard]] static std::vector<Group> &groups(List list);

    void changed(List list) const;
};
