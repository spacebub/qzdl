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

#include "gui/app/Reach.h"
#include "gui/app/Shell.h"
#include "gui/components/Frame.h"
#include "gui/components/LogDock.h"
#include "gui/components/TitleBar.h"
#include "gui/components/Toasts.h"
#include "gui/components/sheets/EntrySheet.h"
#include "gui/components/sheets/SheetLayer.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/Engines.h"
#include "gui/model/IwadArt.h"
#include "gui/model/Notifier.h"
#include "gui/model/Picker.h"
#include "gui/model/Runs.h"
#include "gui/pages/Engines.h"
#include "gui/pages/Library.h"
#include "gui/pages/Profile.h"
#include "gui/pages/Settings.h"
#include "gui/toolkit/overlays/Sheet.h"
#include "gui/toolkit/overlays/Tips.h"

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

    void go(State::Page page);
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

    // Declared last of the services, so every reference in it is already built.
    Reach _reach;

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

    std::vector<State::Page> _history;
    std::vector<State::Page> _ahead;

    // Pages stay built once visited, keeping their scroll position.
    bool _sawProfile = false;
    bool _sawEngines = false;
    bool _sawSettings = false;

    bool _dirty = true;
};
