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

#include "gui/model/Bridge.h"

// The settings page, and which config file is open.
class SettingsBridge : public Bridge {
public:
    using Bridge::Bridge;

    void push() const;
    static void pushPath();

    void setGamePort(const std::string &value) const;
    void setAlwaysAdd(const std::string &value) const;
    void setDosbox(const std::string &value) const;
    void setAutoClose(bool value) const;
    void setLaunchZdlImmediately(bool value) const;
    void setShowPaths(bool value) const;
    void setStartView(const std::string &value) const;
    void setProfileConfigs(bool value) const;
    void setIgnoreUserConfig(bool value) const;

    void clearEverything() const;

    void saveAs(const std::string &path) const;
    void load(const std::string &path) const;
    void adoptAsUserConfig() const;
};
