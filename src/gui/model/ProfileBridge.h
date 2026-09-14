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
    void pushCommand() const;

    // Anything that changes what would be launched.
    void touch();

    // Called by the hub once the preview debounce is due.
    static void showCommand();

    [[nodiscard]] static State::ProfileCard cardOf(int index);

    // The pills a card shows, from what cardOf put on it.
    [[nodiscard]] static std::vector<State::BadgeSpec> badgesOf(const State::ProfileCard &card);

    [[nodiscard]] static std::string artKey();
    [[nodiscard]] static std::string zdlFileName();

    // Whether the active profile can be launched as it stands.
    [[nodiscard]] static bool launchable();

    // Stats each profile, so built only when the dialog asks.
    static void pushConfigDonors();

    void setProfileIndex(int index) const;
    void setIwad(const std::string &value);
    void setPort(const std::string &value) const;
    void setSkill(int value) const;
    void setMonsters(int value) const;
    void setWarp(const std::string &value) const;
    void setExtra(const std::string &value) const;
    void setSharedConfig(bool value) const;
    void setCommandOverride(bool value) const;
    void setCommand(const std::string &value) const;
    void setDosFullscreen(bool value) const;
    void setDosExit(bool value) const;
    void setCaptureOutput(bool value) const;
    void setLevelstat(bool value) const;

    void moveProfile(int from, int to) const;
    void addProfile(const std::string &name) const;
    void duplicateProfile() const;
    void renameProfile(const std::string &name) const;
    void removeProfile() const;
    void clearProfile() const;

    // What Delete warns of: the folder goes only when it is the profile's own.
    [[nodiscard]] static std::string removalNote();

    void copyEngineConfig(const std::string &id) const;

    void loadZdl(const std::string &path) const;
    void saveZdl(const std::string &path) const;

    void launch() const;
    void launchAt(int index) const;

private:
    [[nodiscard]] bool running(const Profile &profile) const;

    std::vector<std::string> _maps;
    bool _mapsKnown{false};

    // What _maps was worked out from.
    std::string _mapsMark;
};
