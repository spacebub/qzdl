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

#include "ttk/util/Format.h"

#include "core/config/ConfigPaths.h"
#include "core/config/Schema.h"
#include "core/ports/Detect.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/SettingsBridge.h"

#ifdef _WIN32
#include "core/launch/Association.h"
#endif

using namespace ttk;

void SettingsBridge::push() const {
    State::Cfg &state = cfg();
    const GeneralSettings &general = config().general;

    state.gameEngine = general.gameEngine;
    state.alwaysAdd = general.alwaysAdd;
    state.dosbox = general.dosbox;
    state.systemDosbox = Format::from_path(Detect::dosbox());
    state.autoClose = general.autoClose;
    state.launchZdlImmediately = general.launchZdlImmediately;
    state.showPaths = general.showPaths;
    state.startView = general.startView;
    state.profileConfigs = general.profileConfigs;

#ifdef _WIN32
    state.zdlAssociated = Association::held();
#endif

    _hub->library().pushGameRev();

    State::get().touch();
}

void SettingsBridge::pushPath() {
    State::Cfg &state = cfg();
    const Session &session = Session::get();

    state.path = Format::from_path(session.path());
    state.dataDirectory = Format::from_path(ConfigPaths::dataDirectory());
    state.openedByHand = session.openedByHand();
    state.portableRunning = session.source() == Session::Source::Portable;
    state.portableWanted = session.portableWanted();

    State::get().touch();
}

void SettingsBridge::setGameEngine(const std::string &value) const {
    config().general.gameEngine = value;

    _hub->scheduleSave();
    push();
    _hub->library().pushShelf();
}

void SettingsBridge::setAlwaysAdd(const std::string &value) const {
    config().general.alwaysAdd = value;

    _hub->scheduleSave();
    push();
    _hub->profile().pushMaps();
    _hub->profile().pushCommand();
}

void SettingsBridge::setDosbox(const std::string &value) const {
    config().general.dosbox = value;

    _hub->scheduleSave();
    push();
    _hub->profile().pushCommand();
}

void SettingsBridge::setAutoClose(const bool value) const {
    config().general.autoClose = value;

    _hub->scheduleSave();
    push();
}

void SettingsBridge::setLaunchZdlImmediately(const bool value) const {
    config().general.launchZdlImmediately = value;

    _hub->scheduleSave();
    push();
}

#ifdef _WIN32
void SettingsBridge::setZdlAssociated(const bool value) const {
    std::string error;

    if (!Association::set(value, &error)) {
        _notifier->error(error + ".");
    }

    push();
}
#endif

void SettingsBridge::setShowPaths(const bool value) const {
    config().general.showPaths = value;

    _hub->scheduleSave();
    push();
}

void SettingsBridge::setStartView(const StartView value) const {
    config().general.startView = value;

    _hub->scheduleSave();
    push();
}

void SettingsBridge::setProfileConfigs(const bool value) const {
    config().general.profileConfigs = value;

    _hub->scheduleSave();
    push();
    _hub->profile().push();
    _hub->profile().pushCommand();
}

void SettingsBridge::setPortableWanted(const bool value) const {
    std::string error;

    if (!Session::get().setPortableWanted(value, &error)) {
        _notifier->error(error);
    }

    pushPath();
}

void SettingsBridge::clearEverything() const {
    const GeneralSettings kept = config().general;

    config().reset();

    // Clear leaves portable mode as it is, and the user config holds that choice.
    config().general.noUserConf = kept.noUserConf;
    config().general.moveAtNextStart = kept.moveAtNextStart;
    _hub->scheduleSave();
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
    showFormatNote();
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

void SettingsBridge::showFormatNote() const {
    const Session::FormatNote &note = Session::get().formatNote();

    if (note.text.empty()) {
        return;
    }

    if (note.atRisk) {
        _notifier->warning(note.text);
    } else {
        _notifier->info(note.text);
    }
}
