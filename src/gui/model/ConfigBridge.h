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

#include <functional>
#include <string>

#include "core/config/Config.h"
#include "gui/model/LibraryBridge.h"
#include "gui/model/ListsBridge.h"
#include "gui/model/Notifier.h"
#include "gui/model/ProfileBridge.h"
#include "gui/model/ProfilePanels.h"
#include "gui/model/Runs.h"
#include "gui/model/SettingsBridge.h"

class Shell;

// Owns the interface's view of the config. Every setter in its slices writes the
// config, pushes what the interface reads and schedules a save.
class ConfigBridge {
public:
    ConfigBridge(Shell *shell, Notifier *notifier, Runs *runs);

    void reload();

    // For changes made behind this object's back.
    void scheduleSave();

    // The command line preview opens the game, so it is debounced.
    void schedulePreview();

    void flush();

    void bumpRev();

    // A failure is reported and logged here, not returned.
    // NOLINTNEXTLINE(modernize-use-nodiscard)
    bool start(const std::string &key, const std::string &title, const Config &what) const;

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
    static constexpr double AUTOSAVE = 0.4;
    static constexpr double PREVIEW = 0.05;

    Shell *_shell;
    Notifier *_notifier;
    Runs *_runs;

    ProfileBridge _profile;
    ProfilePanels _panels;
    ListsBridge _lists;
    SettingsBridge _settings;
    LibraryBridge _library;

    int _autosave{0};
    int _preview{0};
    bool _pendingSave{false};
    bool _warnedSave{false};
    int _rev{0};
};
