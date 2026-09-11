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

// The profile page, and the profile list it picks from.
class ProfileBridge : public Bridge {
public:
    using Bridge::Bridge;

    // The active profile's fields.
    void push() const;

    void pushCards() const;
    void pushMaps();
    void pushCommand();

    // Anything that changes what would be launched.
    void touch();

    // Called by the hub once the preview debounce is due.
    static void showCommand();

    [[nodiscard]] static State::ProfileCard cardOf(int index);

    [[nodiscard]] static std::vector<State::BadgeSpec> badgesOf(int index);

    [[nodiscard]] static std::string artKey();
    [[nodiscard]] static std::string zdlFileName();

    // Stats each profile, so built only when the sheet asks.
    static void pushConfigDonors();

    void setProfileIndex(int index) const;
    void setIwad(const std::string &value);
    void setPort(const std::string &value);
    void setSkill(int value);
    void setMonsters(int value);
    void setWarp(const std::string &value);
    void setExtra(const std::string &value);
    void setSharedConfig(bool value);
    void setCommandOverride(bool value);
    void setCommand(const std::string &value);
    void setDosFullscreen(bool value);
    void setCaptureOutput(bool value) const;
    void setLevelstat(bool value);

    void moveProfile(int from, int to) const;
    void addProfile(const std::string &name) const;
    void duplicateProfile() const;
    void renameProfile(const std::string &name) const;
    void removeProfile() const;
    void clearProfile() const;

    void copyEngineConfig(const std::string &id) const;

    void loadZdl(const std::string &path) const;
    void saveZdl(const std::string &path) const;

    void launch() const;
    void launchAt(int index) const;

private:
    std::vector<std::string> _maps;
    bool _mapsKnown{false};

    // What _maps was worked out from.
    std::string _mapsMark;
};
