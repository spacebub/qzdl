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

#include <string>
#include <vector>

#include "gui/bridge/Bridge.h"

// The multiplayer, replay and save panels of the profile page.
class ProfilePanels : public Bridge {
public:
    using Bridge::Bridge;

    void bind();

    void pushMultiplayer() const;
    void pushReplay();
    void pushSave();

    // 0 alone, 1 hosts, 2 joins.
    [[nodiscard]] static int netRoleOf(const MultiplayerSettings &mp);

private:
    static MultiplayerSettings &multiplayer() {
        return active().multiplayer;
    }

    static ReplaySettings &replay() {
        return active().replay;
    }

    static SaveSettings &save() {
        return active().save;
    }

    // Read when the folder changes or the panel asks, never per keystroke.
    std::vector<std::string> _replays;
    std::string _replaysFrom;
    bool _replaysRead{false};

    std::vector<std::string> _saves;
    std::string _savesFrom;
    bool _savesRead{false};
};
