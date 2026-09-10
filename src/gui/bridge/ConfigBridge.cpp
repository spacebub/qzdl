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

#include "core/config/Session.h"
#include "core/launch/Command.h"
#include "core/launch/Launcher.h"
#include "gui/bridge/ConfigBridge.h"

ConfigBridge::ConfigBridge(const ui::Zdl *window, Notifier *notifier, Runs *runs)
    : _window(window), _notifier(notifier), _runs(runs),
      _profile(window, notifier, this),
      _panels(window, notifier, this),
      _lists(window, notifier, this),
      _settings(window, notifier, this),
      _library(window, notifier, this) {
    _profile.bind();
    _panels.bind();
    _lists.bind();
    _settings.bind();
    _library.bind();

    reload();
}

void ConfigBridge::reload() {
    _lists.push();
    _profile.pushCards();
    _profile.push();
    _panels.pushMultiplayer();
    _settings.push();
    _settings.pushPath();
    _profile.touch();
}

void ConfigBridge::scheduleSave() {
    _pendingSave = true;

    // Debounced: a field being typed into changes on every key.
    _autosave.start(slint::TimerMode::SingleShot, AUTOSAVE, [this] { flush(); });
}

void ConfigBridge::flush() {
    if (!_pendingSave) {
        return;
    }

    _autosave.stop();
    _pendingSave = false;

    std::string error;

    if (Session::get().save(&error)) {
        _warnedSave = false;

        return;
    }

    if (!_warnedSave) {
        _warnedSave = true;
        _notifier->error("Could not save the config: " + error);
    }
}

void ConfigBridge::bumpRev() {
    _window->global<ui::Cfg>().set_rev(++_rev);
}

std::string ConfigBridge::profileKey(const std::string &id) {
    return "profile:" + id;
}

std::string ConfigBridge::gameKey(const std::string &iwad) {
    return "game:" + iwad;
}

bool ConfigBridge::start(const std::string &key, const std::string &title, const Config &what) const {
    std::string error;
    Process::Id started = 0;
    Process::Stream output = Process::NOTHING;

    // DOSBox prints into its own window, and auto close would leave a pipe nobody drains.
    const bool capture = what.activeProfile().captureOutput
        && !Launcher::isDosPort(what)
        && !what.general.autoClose;
    const std::string line = Command::line(what);

    if (!Launcher::launch(what, &started, capture ? &output : nullptr, &error)) {
        _notifier->error(error, "Nothing was launched");
        _runs->refused(key, title, error);

        return false;
    }

    _runs->began(key, title, line, started, output);

    if (launched) {
        launched();
    }

    return true;
}
