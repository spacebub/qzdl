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

#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/Toggle.h"
#include "ttk/toolkit/layout/Scroll.h"

#include "Pages.h"
#include "core/config/Import.h"
#include "gui/pages/ProfilePage.h"
#include "gui/state/State.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

class ProfilePageTest : public paging::Paged {
protected:
    pages::ProfilePage &page() { return rig().profile(); }

    ttk::Label *emptyTitle() {
        for (ttk::Label *label : paging::all<ttk::Label>(&page())) {
            if (label->text() == "No profiles") {
                return label;
            }
        }

        return nullptr;
    }

    std::vector<ttk::Button *> emptyButtons() {
        std::vector<ttk::Button *> out;

        for (ttk::Button *each : paging::all<ttk::Button>(emptyTitle()->parent())) {
            out.push_back(each);
        }

        return out;
    }

    ttk::Toggle *toggle(const std::string &hint) { return paging::hinted<ttk::Toggle>(&page(), hint); }

    void flip(const std::string &hint) {
        ttk::Toggle *found = toggle(hint);

        ASSERT_NE(found, nullptr) << hint;

        // Through the keyboard, since a switch lower down the page may be scrolled out of reach.
        rig().ui().focus(found);
        rig().ui().key(ttk::Key{.code = ttk::Code::Space});
    }
};

TEST_F(ProfilePageTest, ItIsNotBuiltBeforeItIsVisited) {
    load(paging::world(2));

    EXPECT_FALSE(rig().hasProfile());
}

TEST_F(ProfilePageTest, AVisitBuildsAndShowsIt) {
    load(paging::world(2));
    show(State::Page::Profile);

    ASSERT_TRUE(rig().hasProfile());
    EXPECT_TRUE(page().visible());
    EXPECT_FALSE(rig().library().visible());
}

TEST_F(ProfilePageTest, LeavingHidesItAndKeepsIt) {
    load(paging::world(2));
    show(State::Page::Profile);

    const pages::ProfilePage *built = &page();

    show(State::Page::Library);

    EXPECT_FALSE(page().visible());
    EXPECT_EQ(&page(), built);
}

TEST_F(ProfilePageTest, WithNoProfilesItSaysSo) {
    load(paging::world(0));
    show(State::Page::Profile);

    ASSERT_NE(emptyTitle(), nullptr);
    EXPECT_TRUE(paging::seen(emptyTitle()));
    EXPECT_FALSE(paging::seen(toggle("Use a custom command")));
}

TEST_F(ProfilePageTest, WithAProfileTheEmptyStateIsHidden) {
    load(paging::world(1));
    show(State::Page::Profile);

    ASSERT_NE(emptyTitle(), nullptr);
    EXPECT_FALSE(paging::seen(emptyTitle()));
    EXPECT_TRUE(paging::seen(toggle("Use a custom command")));
}

TEST_F(ProfilePageTest, NewProfileAsksForAName) {
    load(paging::world(0));
    show(State::Page::Profile);

    ASSERT_EQ(emptyButtons().size(), 2U);

    paging::click(rig().ui(), emptyButtons().front());

    ASSERT_EQ(rig().prompts.size(), 1U);
    EXPECT_EQ(rig().prompts.front().title, "New profile");
    EXPECT_EQ(rig().prompts.front().accept, "Create");
}

TEST_F(ProfilePageTest, ANewProfileReplacesTheEmptyState) {
    load(paging::world(0));
    show(State::Page::Profile);

    paging::click(rig().ui(), emptyButtons().front());
    rig().prompts.front().accepted("Zulu");
    rig().turn();

    EXPECT_EQ(cfg().profileName, "Zulu");
    EXPECT_FALSE(paging::seen(emptyTitle()));
}

TEST_F(ProfilePageTest, ImportOpensThePickerForAZdl) {
    load(paging::world(0));
    show(State::Page::Profile);

    paging::click(rig().ui(), emptyButtons().back());

    EXPECT_TRUE(rig().files().state().open);
    EXPECT_EQ(rig().files().state().title, "Load a .zdl launch config");
}

TEST_F(ProfilePageTest, AZdlPickedBecomesTheProfile) {
    load(paging::world(0));
    show(State::Page::Profile);

    Profile written;

    written.name = "Imported";
    written.iwad = "Doom II";

    const fs::path file = tests::Sandbox::scratch("profile-page") / "imported.zdl";

    ASSERT_TRUE(Import::saveZdlFile(file, written));

    paging::click(rig().ui(), emptyButtons().back());
    rig().files().choose({file.string()});
    rig().turn();

    EXPECT_EQ(cfg().profileName, "Imported");
    EXPECT_FALSE(paging::seen(emptyTitle()));
}

TEST_F(ProfilePageTest, TheLevelstatSwitchShowsOnlyForAPortThatWritesOne) {
    load(paging::world(1));
    show(State::Page::Profile);

    EXPECT_FALSE(paging::seen(toggle("Writes a levelstat.txt")));

    rig().config().profile().setPort("PrBoom");
    rig().turn();

    EXPECT_TRUE(paging::seen(toggle("Writes a levelstat.txt")));
}

TEST_F(ProfilePageTest, FlippingLevelstatWritesTheProfile) {
    Config made = paging::world(1);

    made.profiles.front().port = "PrBoom";
    load(made);
    show(State::Page::Profile);

    flip("Writes a levelstat.txt");

    EXPECT_TRUE(config().activeProfile().levelstat);

    rig().turn();

    EXPECT_TRUE(toggle("Writes a levelstat.txt")->checked);
}

TEST_F(ProfilePageTest, TheSharedConfigSwitchNeedsPerProfileConfigs) {
    load(paging::world(1));
    show(State::Page::Profile);

    EXPECT_FALSE(paging::seen(toggle("Launch on the settings the source port keeps")));

    rig().config().settings().setProfileConfigs(true);
    rig().turn();

    EXPECT_TRUE(paging::seen(toggle("Launch on the settings the source port keeps")));
}

TEST_F(ProfilePageTest, FlippingTheOverrideWritesTheProfile) {
    load(paging::world(1));
    show(State::Page::Profile);

    flip("Use a custom command");

    EXPECT_TRUE(config().activeProfile().customCommand);
    EXPECT_FALSE(config().activeProfile().command.empty());
}

TEST_F(ProfilePageTest, SelectingAnotherProfileIsShownWithoutARebuild) {
    Config made = paging::world(2);

    made.profiles[1].port = "PrBoom";
    load(made);
    show(State::Page::Profile);

    const pages::ProfilePage *built = &page();

    rig().config().profile().setProfileIndex(1);
    rig().turn();

    EXPECT_EQ(&page(), built);
    EXPECT_TRUE(paging::seen(toggle("Writes a levelstat.txt")));
}

TEST_F(ProfilePageTest, RemovingTheLastProfileShowsTheEmptyState) {
    load(paging::world(1));
    show(State::Page::Profile);

    rig().config().profile().removeProfile();
    rig().turn();

    EXPECT_TRUE(paging::seen(emptyTitle()));
}

TEST_F(ProfilePageTest, ATallPageScrolls) {
    load(paging::world(1));

    canvas().resize(1280, 480);
    rig().ui().relayout();
    show(State::Page::Profile);
    rig().ready();

    const std::vector<ttk::Scroll *> scrolls = paging::all<ttk::Scroll>(&page());
    ttk::Scroll *body = nullptr;

    for (ttk::Scroll *each : scrolls) {
        if (each->scrollable() && (body == nullptr || each->box().h > body->box().h)) {
            body = each;
        }
    }

    ASSERT_NE(body, nullptr);

    const BLRect box = body->box();

    rig().ui().wheel(-3.0, box.x + (box.w / 2.0), box.y + (box.h / 2.0));
    canvas().run(40);

    EXPECT_GT(body->offset(), 0.0);
}

}
