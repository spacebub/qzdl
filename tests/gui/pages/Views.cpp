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

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/toolkit/controls/Field.h"

#include "Pages.h"
#include "gui/app/Views.h"
#include "gui/dialogs/AboutDialog.h"
#include "gui/pages/EnginesPage.h"
#include "gui/pages/LibraryPage.h"
#include "gui/pages/ProfilePage.h"
#include "gui/pages/SettingsPage.h"
#include "gui/state/State.h"

namespace {

class Views : public paging::Paged {
protected:
    void visitAll() {
        for (const State::Page page :
             {State::Page::Profile, State::Page::Engines, State::Page::Settings, State::Page::Library}) {
            show(page);
        }
    }

    views::Tree tree() {
        return views::Tree{.bar = &rig().bar(),
                           .logs = &rig().logs(),
                           .dialogs = &rig().dialogs(),
                           .library = &rig().library(),
                           .profile = rig().hasProfile() ? &rig().profile() : nullptr,
                           .engines = rig().hasEnginesPage() ? &rig().enginesPage() : nullptr,
                           .settings = rig().hasSettings() ? &rig().settings() : nullptr};
    }

    ttk::Field *alwaysAdd() { return paging::all<ttk::Field>(&rig().settings()).front(); }
};

TEST_F(Views, OnlyTheLibraryIsBuiltAtTheStart) {
    load(paging::world(1));

    EXPECT_FALSE(rig().hasProfile());
    EXPECT_FALSE(rig().hasEnginesPage());
    EXPECT_FALSE(rig().hasSettings());
}

TEST_F(Views, OnlyTheCurrentPageIsShown) {
    load(paging::world(1));
    visitAll();

    EXPECT_TRUE(rig().library().visible());
    EXPECT_FALSE(rig().profile().visible());
    EXPECT_FALSE(rig().enginesPage().visible());
    EXPECT_FALSE(rig().settings().visible());

    show(State::Page::Engines);

    EXPECT_FALSE(rig().library().visible());
    EXPECT_TRUE(rig().enginesPage().visible());
    EXPECT_FALSE(rig().settings().visible());
}

TEST_F(Views, VisitingAPageAgainDoesNotBuildItAgain) {
    load(paging::world(1));

    show(State::Page::Settings);
    show(State::Page::Library);
    show(State::Page::Settings);

    EXPECT_EQ(paging::all<pages::SettingsPage>(rig().ui().content()).size(), 1U);
}

TEST_F(Views, AHiddenPageIsNotSynced) {
    load(paging::world(1));

    show(State::Page::Settings);
    show(State::Page::Library);

    rig().config().settings().setAlwaysAdd("-nosound");
    rig().turn();

    EXPECT_EQ(alwaysAdd()->text(), "");
}

TEST_F(Views, AHiddenPageCatchesUpWhenShown) {
    load(paging::world(1));

    show(State::Page::Settings);
    show(State::Page::Library);

    rig().config().settings().setAlwaysAdd("-nosound");
    rig().turn();
    show(State::Page::Settings);

    EXPECT_EQ(alwaysAdd()->text(), "-nosound");
}

TEST_F(Views, TheDialogOnTopIsSyncedEveryTurn) {
    load(paging::world(1));

    rig().dialogs().show(std::make_unique<dialogs::AboutDialog>());

    State::get().sys.version = "7.7.7";
    State::get().sys.runtime = "tinytk 9.9";
    State::get().touch();
    rig().turn();

    EXPECT_TRUE(paging::shows(rig().dialogs().top(), "Version 7.7.7 · tinytk 9.9"));
}

TEST_F(Views, SyncingWithOnlyTheLibraryBuiltIsSafe) {
    load(paging::world(1));

    views::syncAll(tree(), State::Page::Settings);

    EXPECT_FALSE(rig().library().visible());

    views::syncAll(tree(), State::Page::Library);

    EXPECT_TRUE(rig().library().visible());
}

TEST_F(Views, SyncingShowsThePageAskedFor) {
    load(paging::world(1));
    visitAll();

    views::syncAll(tree(), State::Page::Profile);

    EXPECT_TRUE(rig().profile().visible());
    EXPECT_FALSE(rig().library().visible());
    EXPECT_FALSE(rig().enginesPage().visible());
    EXPECT_FALSE(rig().settings().visible());
}

TEST_F(Views, ThePageShownIsSyncedInTheSameCall) {
    load(paging::world(1));
    visitAll();

    rig().config().settings().setAlwaysAdd("-fast");

    views::syncAll(tree(), State::Page::Settings);

    EXPECT_EQ(alwaysAdd()->text(), "-fast");
}

TEST_F(Views, ATurnSyncsOnlyWhenSomethingMoved) {
    load(paging::world(1));

    EXPECT_FALSE(rig().dirty());

    rig().config().profile().setSkill(2);

    EXPECT_TRUE(rig().dirty());

    rig().turn();

    EXPECT_FALSE(rig().dirty());
}

}
