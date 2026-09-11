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
#include <vector>

#include "gui/app/Shell.h"
#include "gui/components/Toasts.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/Engines.h"
#include "gui/model/IwadArt.h"
#include "gui/model/Notifier.h"
#include "gui/model/Picker.h"
#include "gui/model/Runs.h"
#include "gui/model/State.h"
#include "gui/toolkit/overlays/Tips.h"

namespace components {

class TitleBar;
class Frame;
class LogDock;
class Toasts;
class SheetLayer;
class EntrySheet;

}

namespace pages {

class LibraryPage;
class ProfilePage;
class EnginesPage;
class SettingsPage;

}

namespace toolkit {

class Sheet;

}

// The window's contents and everything behind them.
class App {
public:
    App();
    ~App();

    App(const App &) = delete;
    App &operator=(const App &) = delete;
    App(App &&) = delete;
    App &operator=(App &&) = delete;

    bool start();

    void run();

    // --- what the views reach for ---

    Shell &shell() { return _shell; }

    ConfigBridge &config() { return _config; }

    Notifier &notify() { return _notifier; }

    Runs &runs() { return _runs; }

    Picker &picker() { return _picker; }

    Engines &engines() { return _engines; }

    IwadArt &art() { return _art; }

    static State::All &state() { return State::get(); }

    void go(const std::string &page);
    void back();
    void forward();

    // The sheets. Each is made when it is asked for and gone when it closes, and
    // each is told what to do rather than leaving an action for this to look up.
    void ask(const std::string &title, const std::string &body, const std::string &accept,
             bool danger, std::function<void()> accepted);

    void prompt(const std::string &title, const std::string &label, const std::string &value,
                const std::string &accept, std::function<void(const std::string &)> accepted);

    void edit(const std::string &title, const std::string &kind,
              const std::vector<std::string> &filters, const std::string &remember,
              const std::string &name, const std::string &file, bool offerDos, bool dosbox,
              std::function<void(const std::string &, const std::string &, bool)> accepted);

    void showAbout();
    void showCommand();
    void copyConfig();

    // True while anything is over the page; what Escape closes.
    bool covered() const;
    void dismissTop();

    // Marks the interface for a sync at the next frame.
    void touch();

    // The theme button: system, light, dark and round again.
    void cycleShade();

    // What a file picker came back with.
    void picked(const std::string &action, const std::vector<std::string> &paths, bool option);

    // Filters, as the pickers ask for them.
    static const std::vector<std::string> &wadFilters();
    static const std::vector<std::string> &portFilters();
    static const std::vector<std::string> &zdlFilters();
    static const std::vector<std::string> &configFilters();
    static const std::vector<std::string> &saveFilters();
    static const std::vector<std::string> &replayFilters();

private:
    void build();

    void sync();

    void restoreGeometry();
    void rememberGeometry();

    void persist();

    bool shortcut(const toolkit::Key &pressed);

    static constexpr size_t HISTORY = 24;

    Shell _shell;

    Notifier _notifier;
    IwadArt _art;
    Runs _runs;
    ConfigBridge _config;
    Picker _picker;
    Engines _engines;

    components::TitleBar *_bar = nullptr;
    pages::LibraryPage *_library = nullptr;
    pages::ProfilePage *_profile = nullptr;
    pages::EnginesPage *_enginesView = nullptr;
    pages::SettingsPage *_settings = nullptr;
    components::LogDock *_logs = nullptr;
    components::SheetLayer *_sheets = nullptr;

    // Only while they are up: the picker's answer comes back to the entry sheet,
    // and the picker's own sheet follows whether the picker is open.
    components::EntrySheet *_entry = nullptr;
    toolkit::Sheet *_pick = nullptr;
    components::Toasts *_toasts = nullptr;
    toolkit::Tips *_tips = nullptr;

    // The page area, which the views fill in turn.
    toolkit::Widget *_pages = nullptr;

    std::vector<std::string> _history;
    std::vector<std::string> _ahead;

    // Pages stay built once visited, keeping their scroll position.
    bool _sawProfile = false;
    bool _sawEngines = false;
    bool _sawSettings = false;

    bool _dirty = true;
};
