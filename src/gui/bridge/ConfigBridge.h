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

#include <chrono>
#include <functional>
#include <string>

#include "core/config/Config.h"
#include "main.h"
#include "gui/bridge/LibraryBridge.h"
#include "gui/bridge/ListsBridge.h"
#include "gui/bridge/ProfileBridge.h"
#include "gui/bridge/ProfilePanels.h"
#include "gui/bridge/SettingsBridge.h"
#include "gui/components/Notifier.h"
#include "gui/components/Runs.h"

// Owns the slices of the Cfg global. Every setter in them writes the config,
// pushes what the interface reads and schedules a save.
class ConfigBridge {
public:
    ConfigBridge(const ui::Zdl *window, Notifier *notifier, Runs *runs);

    void reload();

    // For changes made behind this object's back.
    void scheduleSave();

    void flush();

    void bumpRev();

    bool start(const std::string &key, const std::string &title, const Config &what);

    [[nodiscard]] static std::string profileKey(const std::string &id);
    [[nodiscard]] static std::string gameKey(const std::string &iwad);

    [[nodiscard]] ProfileBridge &profile() { return _profile; }
    [[nodiscard]] ProfilePanels &panels() { return _panels; }
    [[nodiscard]] ListsBridge &lists() { return _lists; }
    [[nodiscard]] SettingsBridge &settings() { return _settings; }
    [[nodiscard]] LibraryBridge &library() { return _library; }

    std::function<void()> launched;

    // detect is false for a config emptied on purpose.
    std::function<void(bool detect)> replaced;

private:
    static constexpr std::chrono::milliseconds AUTOSAVE{400};

    const ui::Zdl *_window;
    Notifier *_notifier;
    Runs *_runs;

    ProfileBridge _profile;
    ProfilePanels _panels;
    ListsBridge _lists;
    SettingsBridge _settings;
    LibraryBridge _library;

    slint::Timer _autosave;
    bool _pendingSave{false};
    bool _warnedSave{false};
    int _rev{0};
};
