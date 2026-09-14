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

#include "gui/components/LogDock.h"
#include "gui/components/TitleBar.h"
#include "gui/dialogs/DialogLayer.h"
#include "gui/pages/EnginesPage.h"
#include "gui/pages/LibraryPage.h"
#include "gui/pages/ProfilePage.h"
#include "gui/pages/SettingsPage.h"
#include "gui/state/State.h"

// Showing and syncing what the window holds. The benchmark rig drives the same tree
// from its own main loop, and a copy of this in it would be a second thing to keep
// in step with the pages.
namespace views {

struct Tree {
    components::TitleBar *bar = nullptr;
    components::LogDock *logs = nullptr;
    dialogs::DialogLayer *dialogs = nullptr;

    pages::LibraryPage *library = nullptr;

    // Built on the first visit, so null until then.
    pages::ProfilePage *profile = nullptr;
    pages::EnginesPage *engines = nullptr;
    pages::SettingsPage *settings = nullptr;
};

inline void syncAll(const Tree &tree, const State::Page page) {
    tree.dialogs->sync();
    tree.bar->sync();
    tree.logs->sync();

    tree.library->setVisible(page == State::Page::Library);
    tree.library->sync();

    if (tree.profile != nullptr) {
        tree.profile->setVisible(page == State::Page::Profile);
        tree.profile->sync();
    }

    if (tree.engines != nullptr) {
        tree.engines->setVisible(page == State::Page::Engines);
        tree.engines->sync();
    }

    if (tree.settings != nullptr) {
        tree.settings->setVisible(page == State::Page::Settings);
        tree.settings->sync();
    }
}

}
