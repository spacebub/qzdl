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

#include "gui/model/Bridge.h"

// The multiplayer, replay and save panels of the profile page.
class ProfilePanels : public Bridge {
public:
    using Bridge::Bridge;

    void pushMultiplayer() const;
    void pushReplay();
    void pushSave();

    // 0 alone, 1 hosts, 2 joins.
    [[nodiscard]] static int netRoleOf(const MultiplayerSettings &mp);

    void setMultiplayerOpen(bool value) const;
    void setNetRole(int value);
    void setGameType(int value);
    void setPlayers(int value);
    void setHost(const std::string &value);
    void setNetPort(const std::string &value);
    void setFragLimit(const std::string &value);
    void setTimeLimit(const std::string &value);
    void setDmflags(const std::string &value);
    void setDmflags2(const std::string &value);
    void setExtratic(int value);
    void setNetmode(int value);
    void setDup(int value);
    void setListed(bool value);
    void setSavegame(const std::string &value);
    void clearMultiplayer();

    void setReplayOpen(bool value) const;
    void setReplayMode(int value);
    void setReplayFile(const std::string &value);
    void setReplayIndex(int index);
    void setReplayPlayback(int value);
    void setReplayComplevel(int index);
    void setReplayLongtics(bool value);
    void setReplaySoloNet(bool value);
    void refreshReplays();
    void clearReplay();

    void setSaveOpen(bool value) const;
    void setSaveEnabled(bool value);
    void setSaveIndex(int index);
    void refreshSaves();

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
