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

#include "core/ports/Detect.h"
#include "core/system/Paths.h"
#include "gui/Convert.h"
#include "gui/bridge/ConfigBridge.h"
#include "gui/bridge/SettingsBridge.h"

void SettingsBridge::push() {
    const ui::Cfg &state = cfg();
    const GeneralSettings &general = config().general;

    state.set_game_port(Convert::text(general.gamePort));
    state.set_always_add(Convert::text(general.alwaysAdd));
    state.set_dosbox(Convert::text(general.dosbox));
    state.set_system_dosbox(Convert::fromPath(Detect::dosbox()));
    state.set_auto_close(general.autoClose);
    state.set_launch_zdl_immediately(general.launchZdlImmediately);
    state.set_show_paths(general.showPaths);
    state.set_start_view(Convert::text(general.startView == "games" ? "games" : "profiles"));
    state.set_profile_configs(general.profileConfigs);
    state.set_ignore_user_config(Session::get().userConfigIgnored());

    _hub->library().pushGameRev();
    _hub->scheduleSave();
}

void SettingsBridge::pushPath() {
    const ui::Cfg &state = cfg();

    state.set_path(Convert::fromPath(Session::get().path()));
    state.set_user_config(Session::get().path() == Paths::get().configPath(Paths::USER));
}

void SettingsBridge::bind() {
    const ui::Cfg &state = cfg();

    state.on_set_game_port([this](const slint::SharedString &value) {
        config().general.gamePort = Convert::plain(value);

        push();
        _hub->library().pushShelf();
    });

    state.on_set_always_add([this](const slint::SharedString &value) {
        config().general.alwaysAdd = Convert::plain(value);

        push();
        _hub->profile().pushCommand();
    });

    state.on_set_dosbox([this](const slint::SharedString &value) {
        config().general.dosbox = Convert::plain(value);

        push();
        _hub->profile().pushCommand();
    });

    state.on_set_auto_close([this](const bool value) {
        config().general.autoClose = value;

        push();
    });

    state.on_set_launch_zdl_immediately([this](const bool value) {
        config().general.launchZdlImmediately = value;

        push();
    });

    state.on_set_show_paths([this](const bool value) {
        config().general.showPaths = value;

        push();
    });

    state.on_set_start_view([this](const slint::SharedString &value) {
        config().general.startView = value == "games" ? "games" : "profiles";

        push();
    });

    state.on_set_profile_configs([this](const bool value) {
        config().general.profileConfigs = value;

        push();
        _hub->profile().push();
        _hub->profile().pushCommand();
    });

    state.on_set_ignore_user_config([this](const bool value) {
        std::string error;

        if (!Session::get().setUserConfigIgnored(value, &error)) {
            _notifier->error("Could not write the user config: " + error);

            return;
        }

        push();
    });

    state.on_clear_everything([this] {
        config().reset();
        _hub->reload();

        if (_hub->replaced) {
            _hub->replaced(false);
        }
    });

    state.on_save_as([this](const slint::SharedString &path) {
        std::string error;

        if (!Session::get().saveAs(Convert::toPath(path), &error)) {
            _notifier->error("Could not save to " + Convert::plain(path) + ": " + error);

            return;
        }

        pushPath();
        _notifier->success("Saved to " + Convert::plain(path) + ".");
    });

    state.on_load([this](const slint::SharedString &path) {
        std::string error;

        // Pending changes belong to the config being left.
        _hub->flush();

        if (!Session::get().load(Convert::toPath(path), &error)) {
            _notifier->error("Could not read " + Convert::plain(path) + ": " + error);

            return;
        }

        _hub->reload();

        if (_hub->replaced) {
            _hub->replaced(true);
        }

        _notifier->success("Loaded " + Convert::plain(path) + ".");
    });

    state.on_adopt_as_user_config([this] {
        std::string error;

        if (!Session::get().adoptAsUserConfig(&error)) {
            _notifier->error("Could not write the user config: " + error);

            return;
        }

        pushPath();
        _notifier->success("This config is now the one ZDL4 opens by default.");
    });
}
