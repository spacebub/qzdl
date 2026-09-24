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

#include "ttk/dialogs/DialogLayer.h"
#include "ttk/dialogs/FilePicker.h"
#include "ttk/notices/Notifier.h"
#include "ttk/notices/Toasts.h"
#include "ttk/shell/Shell.h"
#include "ttk/toolkit/overlays/Dialog.h"
#include "ttk/toolkit/overlays/Tips.h"

#include "gui/components/Frame.h"
#include "gui/components/LogDock.h"
#include "gui/components/Reach.h"
#include "gui/components/TitleBar.h"
#include "gui/dialogs/EntryDialog.h"
#include "gui/model/ConfigBridge.h"
#include "gui/pages/EnginesPage.h"
#include "gui/pages/LibraryPage.h"
#include "gui/pages/ProfilePage.h"
#include "gui/pages/SettingsPage.h"
#include "gui/services/Engines.h"
#include "gui/services/IwadArt.h"
#include "gui/services/Runs.h"

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

    ttk::Shell &shell() { return _shell; }

    ConfigBridge &config() { return _config; }

    ttk::Notifier &notify() { return _notifier; }

    Runs &runs() { return _runs; }

    ttk::FilePicker &files() { return _files; }

    Engines &engines() { return _engines; }

    IwadArt &art() { return _art; }

    void go(State::Page page);
    void back();
    void forward();

    // The dialogs. Each is made when it is asked for and gone when it closes, and
    // each is told what to do rather than leaving an action for this to look up.
    void ask(const std::string &title, const std::string &body, const std::string &accept,
             bool danger, std::function<void()> accepted);

    void prompt(const std::string &title, const std::string &label, const std::string &value,
                const std::string &accept, std::function<void(const std::string &)> accepted);

    void edit(const std::string &title, dialogs::EntryDialog::Kind kind,
              const std::vector<std::string> &filters, const std::string &remember,
              const std::string &name, const std::string &file, bool offerDos, bool dosbox,
              std::function<void(const std::string &, const std::string &, bool)> accepted);

    void showAbout() const;
    void showCommand();
    void copyConfig();

    // True while anything is over the page. What Escape closes.
    bool covered() const;
    void dismissTop() const;

    // Marks the interface for a sync at the next frame.
    void touch();

    // The theme button: system, light, dark and round again.
    void cycleShade();

private:
    void wireReach();
    void wireServices();
    void wireConfig();
    void wireShell();

    static void describeRuntime();
    static void applySavedSettings();

    void build();

    void sync();

    void restoreGeometry() const;
    void rememberGeometry() const;

    void persist();

    bool shortcut(const ttk::Key &pressed);

    static constexpr size_t HISTORY = 24;

    ttk::Shell _shell;

    ttk::Notifier _notifier;
    IwadArt _art;
    Runs _runs;
    ConfigBridge _config;
    ttk::FilePicker _files;
    Engines _engines;

    // Declared last of the services, so every reference in it is already built.
    Reach _reach;

    components::TitleBar *_bar = nullptr;
    pages::LibraryPage *_library = nullptr;
    pages::ProfilePage *_profile = nullptr;
    pages::EnginesPage *_enginesView = nullptr;
    pages::SettingsPage *_settings = nullptr;
    components::LogDock *_logs = nullptr;
    ttk::DialogLayer *_dialogs = nullptr;

    // Only while it is up: follows whether the picker is open.
    ttk::Dialog *_pick = nullptr;
    ttk::Toasts *_toasts = nullptr;
    ttk::Tips *_tips = nullptr;

    // The page area, which the views fill in turn.
    ttk::Widget *_pages = nullptr;

    std::vector<State::Page> _history;
    std::vector<State::Page> _ahead;

    // Pages stay built once visited, keeping their scroll position.
    bool _sawProfile = false;
    bool _sawEngines = false;
    bool _sawSettings = false;

    bool _dirty = true;
};
