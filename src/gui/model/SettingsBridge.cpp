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

#include "core/config/Schema.h"
#include "core/ports/Detect.h"
#include "core/system/Paths.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/SettingsBridge.h"
#include "gui/util/Format.h"

void SettingsBridge::push() const {
    State::Cfg &state = cfg();
    const GeneralSettings &general = config().general;

    state.gamePort = general.gamePort;
    state.alwaysAdd = general.alwaysAdd;
    state.dosbox = general.dosbox;
    state.systemDosbox = Format::fromPath(Detect::dosbox());
    state.autoClose = general.autoClose;
    state.launchZdlImmediately = general.launchZdlImmediately;
    state.showPaths = general.showPaths;
    state.startView = general.startView == StartView::GAMES ? StartView::GAMES
                                                            : StartView::PROFILES;
    state.profileConfigs = general.profileConfigs;
    state.ignoreUserConfig = Session::get().userConfigIgnored();

    _hub->library().pushGameRev();
    _hub->scheduleSave();

    State::get().touch();
}

void SettingsBridge::pushPath() {
    State::Cfg &state = cfg();

    state.path = Format::fromPath(Session::get().path());
    state.userConfig = Session::get().path() == Paths::get().configPath(Paths::USER);

    State::get().touch();
}

void SettingsBridge::setGamePort(const std::string &value) const {
    config().general.gamePort = value;

    push();
    _hub->library().pushShelf();
}

void SettingsBridge::setAlwaysAdd(const std::string &value) const {
    config().general.alwaysAdd = value;

    push();
    _hub->profile().pushCommand();
}

void SettingsBridge::setDosbox(const std::string &value) const {
    config().general.dosbox = value;

    push();
    _hub->profile().pushCommand();
}

void SettingsBridge::setAutoClose(const bool value) const {
    config().general.autoClose = value;

    push();
}

void SettingsBridge::setLaunchZdlImmediately(const bool value) const {
    config().general.launchZdlImmediately = value;

    push();
}

void SettingsBridge::setShowPaths(const bool value) const {
    config().general.showPaths = value;

    push();
}

void SettingsBridge::setStartView(const std::string &value) const {
    config().general.startView = value == StartView::GAMES ? StartView::GAMES
                                                           : StartView::PROFILES;

    push();
}

void SettingsBridge::setProfileConfigs(const bool value) const {
    config().general.profileConfigs = value;

    push();
    _hub->profile().push();
    _hub->profile().pushCommand();
}

void SettingsBridge::setIgnoreUserConfig(const bool value) const {
    std::string error;

    if (!Session::get().setUserConfigIgnored(value, &error)) {
        _notifier->error("Could not write the user config: " + error);

        return;
    }

    push();
}

void SettingsBridge::clearEverything() const {
    config().reset();
    _hub->reload();

    if (_hub->replaced) {
        _hub->replaced(false);
    }
}

void SettingsBridge::saveAs(const std::string &path) const {
    std::string error;

    if (!Session::get().saveAs(std::filesystem::path(path), &error)) {
        _notifier->error("Could not save to " + path + ": " + error);

        return;
    }

    pushPath();
    _notifier->success("Saved to " + path + ".");
}

void SettingsBridge::load(const std::string &path) const {
    std::string error;

    // Pending changes belong to the config being left.
    _hub->flush();

    if (!Session::get().load(std::filesystem::path(path), &error)) {
        _notifier->error("Could not read " + path + ": " + error);

        return;
    }

    _hub->reload();

    if (_hub->replaced) {
        _hub->replaced(true);
    }

    _notifier->success("Loaded " + path + ".");
}

void SettingsBridge::adoptAsUserConfig() const {
    std::string error;

    if (!Session::get().adoptAsUserConfig(&error)) {
        _notifier->error("Could not write the user config: " + error);

        return;
    }

    pushPath();
    _notifier->success("This config is now the one ZDL4 opens by default.");
}
