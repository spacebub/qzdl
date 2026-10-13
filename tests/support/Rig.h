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

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ttk/dialogs/DialogLayer.h"
#include "ttk/dialogs/FilePicker.h"
#include "ttk/notices/Notifier.h"
#include "ttk/notices/Toasts.h"
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
#include "gui/state/State.h"
#include "support/Canvas.h"
#include "support/Shell.h"

namespace tests {

// What a view asked the window to put up, held so a case can answer it.
struct Ask {
    std::string title;
    std::string body;
    std::string accept;
    bool danger = false;
    std::function<void()> accepted;
};

struct Prompt {
    std::string title;
    std::string label;
    std::string value;
    std::string accept;
    std::function<void(const std::string &)> accepted;
};

struct Edit {
    std::string title;
    dialogs::EntryDialog::Kind kind{};
    std::vector<std::string> filters;
    std::string remember;
    std::string name;
    std::string file;
    bool offerDos = false;
    bool dosbox = false;
    std::function<void(const std::string &, const std::string &, bool)> accepted;
};

// App without the window: the same services, the same tree, the same wiring and
// the same sync, mounted in a Canvas and clocked by a Shell a case advances by hand.
//
// Building one empties the Session's config and the State tree first, since both
// are singletons the last case may have left something in. Install a fixture with
// Fixtures::install() and then config().reload() to push it into the tree.
class Rig {
public:
    explicit Rig(Canvas &canvas);
    ~Rig();

    Rig(const Rig &) = delete;
    Rig &operator=(const Rig &) = delete;
    Rig(Rig &&) = delete;
    Rig &operator=(Rig &&) = delete;

    // What App does once per turn when anything touched the state.
    void sync();

    void go(State::Page page);

    // One turn of the frame loop: the clock moves `seconds`, posted functions and
    // due timers run, the tree is synced when anything touched it, and a frame is
    // painted. Answers the pixels painted.
    std::size_t turn(double seconds = 0.0);

    // Settles what the page is built from: the art thread goes quiet, the start-up
    // detection has landed, what was due at once has run and the first frame is out.
    void ready();

    [[nodiscard]] bool dirty() const { return _dirty; }

    Shell &shell() { return _shell; }
    ConfigBridge &config() { return _config; }
    ttk::Notifier &notify() { return _notifier; }
    Runs &runs() { return _runs; }
    ttk::FilePicker &files() { return _files; }
    Engines &engines() { return _engines; }
    IwadArt &art() { return _art; }
    Reach &reach() { return _reach; }

    Canvas &canvas() { return _canvas; }

    ttk::Root &ui() { return _canvas.ui(); }

    components::TitleBar &bar() { return *_bar; }
    components::LogDock &logs() { return *_logs; }
    components::Frame &frame() { return *_frame; }

    pages::LibraryPage &library() { return *_library; }

    // Built on the first visit, as App builds them.
    pages::ProfilePage &profile();
    pages::EnginesPage &enginesPage();
    pages::SettingsPage &settings();

    [[nodiscard]] bool hasProfile() const { return _profile != nullptr; }
    [[nodiscard]] bool hasEnginesPage() const { return _enginesView != nullptr; }
    [[nodiscard]] bool hasSettings() const { return _settings != nullptr; }

    ttk::DialogLayer &dialogs() { return *_dialogs; }
    ttk::Toasts &toasts() { return *_toasts; }
    ttk::Tips &tips() { return *_tips; }

    // What the views asked the window for, oldest first. Nothing is answered until
    // a case calls the stored callback.
    std::vector<Ask> asks;
    std::vector<Prompt> prompts;
    std::vector<Edit> edits;

    int aboutShown = 0;
    int commandShown = 0;
    int copyConfigShown = 0;
    int shadeCycles = 0;

    // Every page the views asked for, in order, the current one included.
    std::vector<State::Page> visited;

private:
    struct Fresh {
        Fresh();
    };

    void build();

    // Before the services, which read both.
    Fresh _fresh;

    Canvas &_canvas;

    Shell _shell;

    ttk::Notifier _notifier;
    IwadArt _art;
    Runs _runs;
    ConfigBridge _config;
    ttk::FilePicker _files;
    Engines _engines;

    Reach _reach;

    components::Frame *_frame = nullptr;
    components::TitleBar *_bar = nullptr;
    components::LogDock *_logs = nullptr;
    ttk::Widget *_pages = nullptr;
    ttk::DialogLayer *_dialogs = nullptr;
    ttk::Toasts *_toasts = nullptr;
    ttk::Tips *_tips = nullptr;

    pages::LibraryPage *_library = nullptr;
    pages::ProfilePage *_profile = nullptr;
    pages::EnginesPage *_enginesView = nullptr;
    pages::SettingsPage *_settings = nullptr;

    bool _dirty = true;
};

// A case against the whole interface. Skips when no font is installed.
class Rigged : public Painted {
protected:
    void SetUp() override {
        Painted::SetUp();

        if (IsSkipped()) {
            return;
        }

        _canvas = std::make_unique<Canvas>();
        _rig = std::make_unique<Rig>(*_canvas);
    }

    void TearDown() override {
        _rig.reset();
        _canvas.reset();

        Painted::TearDown();
    }

    Canvas &canvas() { return *_canvas; }
    Rig &rig() { return *_rig; }

private:
    std::unique_ptr<Canvas> _canvas;
    std::unique_ptr<Rig> _rig;
};

}
