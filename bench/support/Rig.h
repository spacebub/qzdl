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

#include "gui/app/Shell.h"
#include "gui/components/Reach.h"
#include "gui/components/Frame.h"
#include "gui/components/LogDock.h"
#include "gui/components/TitleBar.h"
#include "gui/components/Toasts.h"
#include "gui/dialogs/DialogLayer.h"
#include "gui/pages/EnginesPage.h"
#include "gui/pages/LibraryPage.h"
#include "gui/pages/ProfilePage.h"
#include "gui/pages/SettingsPage.h"
#include "gui/state/State.h"
#include "gui/toolkit/overlays/Tips.h"
#include "support/Canvas.h"

namespace bench {

// App without the window: the same services, the same tree and the same sync,
// mounted in a Canvas. The Shell it holds is never started.
class Rig {
public:
    explicit Rig(Canvas &canvas);
    ~Rig();

    Rig(const Rig &) = delete;
    Rig &operator=(const Rig &) = delete;
    Rig(Rig &&) = delete;
    Rig &operator=(Rig &&) = delete;

    void sync();

    void go(State::Page page);

    // Puts back what a benchmark before this one left on the shared rig. The shelf
    // filter decides what the whole library is built from, so a stale one makes a
    // number depend on which file ran first.
    void forget();

    // Settles what the page is built from: the art thread goes quiet and the first
    // frame is out, so timing starts from the same picture every run. Called after
    // the fixture is in and the page is synced.
    void ready();

    ConfigBridge &config() { return _config; }
    Notifier &notify() { return _notifier; }
    Runs &runs() { return _runs; }

    Canvas &canvas() { return _canvas; }

    toolkit::Root &ui() { return _canvas.ui(); }

    pages::LibraryPage &library() { return *_library; }
    pages::ProfilePage &profile();
    pages::EnginesPage &enginesPage();
    pages::SettingsPage &settings();

    dialogs::DialogLayer &dialogs() { return *_dialogs; }

private:
    void build();


    Canvas &_canvas;

    Shell _shell;

    Notifier _notifier;
    IwadArt _art;
    Runs _runs;
    ConfigBridge _config;
    FilePicker _picker;
    Engines _engines;

    Reach _reach;

    components::TitleBar *_bar = nullptr;
    components::LogDock *_logs = nullptr;
    toolkit::Widget *_pages = nullptr;
    dialogs::DialogLayer *_dialogs = nullptr;
    components::Toasts *_toasts = nullptr;
    toolkit::Tips *_tips = nullptr;

    pages::LibraryPage *_library = nullptr;
    pages::ProfilePage *_profile = nullptr;
    pages::EnginesPage *_enginesView = nullptr;
    pages::SettingsPage *_settings = nullptr;

    bool _dirty = true;
};

// One rig for the process: the State and the Session it drives are singletons,
// so two of them would fight over the same tree.
Rig &shared();

}
