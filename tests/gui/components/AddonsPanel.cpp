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

#include "ttk/draw/Theme.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/Pill.h"

#include "Mount.h"
#include "core/config/Session.h"
#include "gui/components/AddonsPanel.h"
#include "gui/state/State.h"
#include "support/Corpus.h"
#include "support/Rig.h"

namespace {

constexpr BLRect SPOT{20.0, 20.0, 560.0, 420.0};

Config grounded(const int files) {
    Config made = tests::Fixtures::grounded(3, files);

    made.general.showPaths = false;

    return made;
}

class AddonsPanelTest : public parts::Screened {
protected:
    AddonsPanelTest() {
        wide = 800;
        tall = 520;
    }

    components::AddonsPanel *make(Config what = grounded(5), const BLRect &spot = SPOT) {
        parts::install(rig(), std::move(what));

        _panel = parts::place(screen(), std::make_unique<components::AddonsPanel>(&rig().reach()), spot);

        sync();

        return _panel;
    }

    void sync() {
        rig().shell().advance(0.0);
        _panel->sync();
        screen().ui().relayout();
        screen().frame();
    }

    ttk::Pill &loaded() { return *parts::find<ttk::Pill>(_panel); }

    ttk::GlyphButton &add() { return *parts::hinted<ttk::GlyphButton>(_panel, "Add files"); }

    ttk::GlyphButton &clear() {
        return *parts::hinted<ttk::GlyphButton>(_panel, "Remove every file from this profile");
    }

    components::AddonList &list() { return *parts::find<components::AddonList>(_panel); }

    // The pill sizes itself to what it says, so a pill saying the same is as wide.
    double widthOf(const std::string &text) {
        ttk::Pill said(text);

        said.kind(ttk::Pill::Kind::Muted)->dot(false);

        return said.natural_width(screen().type());
    }

    static std::vector<FileEntry> &files() { return Session::get().config().activeProfile().files; }

    components::AddonsPanel *_panel = nullptr;
};

TEST_F(AddonsPanelTest, SaysHowManyOfTheFilesAreLoaded) {
    make(grounded(5));

    EXPECT_TRUE(loaded().visible());
    EXPECT_DOUBLE_EQ(loaded().natural_width(screen().type()), widthOf("3 of 5 loaded"));
}

TEST_F(AddonsPanelTest, SaysOnlyTheCountWhenEveryFileIsLoaded) {
    Config what = grounded(3);

    for (FileEntry &entry : what.profiles.front().files) {
        entry.enabled = true;
    }

    make(std::move(what));

    EXPECT_DOUBLE_EQ(loaded().natural_width(screen().type()), widthOf("3 loaded"));
}

TEST_F(AddonsPanelTest, TheCountFollowsACheckClicked) {
    make(grounded(5));
    parts::click(screen().ui(), list().box().x + 33.0, list().box().y + 17.0);
    sync();

    EXPECT_DOUBLE_EQ(loaded().natural_width(screen().type()), widthOf("4 of 5 loaded"));
}

TEST_F(AddonsPanelTest, SaysNothingWithNoFiles) {
    make(grounded(0));

    EXPECT_FALSE(loaded().visible());
}

TEST_F(AddonsPanelTest, ClearIsOffWithNoFiles) {
    make(grounded(0));

    EXPECT_FALSE(clear().enabled());
}

TEST_F(AddonsPanelTest, ClearAsksFirst) {
    make();
    parts::click(screen().ui(), clear());

    ASSERT_EQ(rig().asks.size(), 1U);
    EXPECT_EQ(rig().asks.front().title, "Clear the file list?");
    EXPECT_TRUE(rig().asks.front().danger);
    EXPECT_EQ(files().size(), 5U);
}

TEST_F(AddonsPanelTest, AcceptingTheClearEmptiesTheList) {
    make();
    parts::click(screen().ui(), clear());

    ASSERT_EQ(rig().asks.size(), 1U);

    rig().asks.front().accepted();
    sync();

    EXPECT_TRUE(files().empty());
    EXPECT_FALSE(loaded().visible());
    EXPECT_FALSE(clear().enabled());
}

TEST_F(AddonsPanelTest, AddOpensAPickerForManyFiles) {
    make();
    parts::click(screen().ui(), add());

    EXPECT_TRUE(rig().files().state().open);
    EXPECT_EQ(rig().files().state().title, "Add files");
    EXPECT_TRUE(rig().files().state().multiple);
}

TEST_F(AddonsPanelTest, FilesPickedGoOnTheEndOfTheList) {
    make(grounded(2));
    parts::click(screen().ui(), add());

    const std::string picked = tests::Corpus::pwad(3).string();

    rig().files().choose({picked});

    ASSERT_EQ(files().size(), 3U);
    EXPECT_EQ(files().back().file, picked);
    EXPECT_TRUE(files().back().enabled);
}

TEST_F(AddonsPanelTest, TheHeadingButtonsSitAtTheRightEdge) {
    make();

    EXPECT_DOUBLE_EQ(clear().box().x + clear().box().w, SPOT.x + SPOT.w - 16.0);
    EXPECT_DOUBLE_EQ(add().box().x + add().box().w + 8.0, clear().box().x);
    EXPECT_LT(loaded().box().x + loaded().box().w, add().box().x);
}

TEST_F(AddonsPanelTest, TheListFillsThePanelUnderTheHeading) {
    make();

    const BLRect &held = list().box();

    EXPECT_DOUBLE_EQ(held.x, SPOT.x + 16.0 + 6.0);
    EXPECT_DOUBLE_EQ(held.w, SPOT.w - 32.0 - 12.0);
    EXPECT_GE(held.y, add().box().y + add().box().h + 12.0);
    EXPECT_DOUBLE_EQ(held.y + held.h, SPOT.y + SPOT.h - 16.0 - 6.0);
}

TEST_F(AddonsPanelTest, ANarrowerPanelNarrowsTheList) {
    make(grounded(5), BLRect{20.0, 20.0, 300.0, 420.0});

    EXPECT_DOUBLE_EQ(list().box().w, 300.0 - 44.0);
    EXPECT_DOUBLE_EQ(clear().box().x + clear().box().w, 20.0 + 300.0 - 16.0);
}

TEST_F(AddonsPanelTest, ARefreshedCountRepaintsLikeAFullFrame) {
    make();
    screen().full();
    parts::click(screen().ui(), list().box().x + 33.0, list().box().y + 17.0);
    sync();

    const parts::Ink off = parts::mismatch(screen());

    EXPECT_EQ(off.pixels, 0U) << off;
}

class AddonsPanelOnThePage : public parts::Settled {};

TEST_F(AddonsPanelOnThePage, ThePillWidensWithWhatItSays) {
    Config what = grounded(3);

    for (FileEntry &entry : what.profiles.front().files) {
        entry.enabled = true;
    }

    parts::install(rig(), std::move(what));
    rig().go(State::Page::Profile);
    rig().turn();

    components::AddonsPanel *panel = parts::find<components::AddonsPanel>(&rig().profile());

    ASSERT_NE(panel, nullptr);

    ttk::Pill &loaded = *parts::find<ttk::Pill>(panel);
    const BLRect &list = parts::find<components::AddonList>(panel)->box();

    parts::click(rig().ui(), list.x + 33.0, list.y + 17.0);
    rig().turn();

    ASSERT_FALSE(Session::get().config().activeProfile().files.front().enabled);
    EXPECT_DOUBLE_EQ(loaded.box().w, loaded.natural_width(canvas().type()));
}

TEST_F(AddonsPanelOnThePage, TheListReachesNoFurtherOnceAFileIsRemoved) {
    parts::install(rig(), grounded(12));
    rig().go(State::Page::Profile);
    rig().turn();

    components::AddonList &list = *parts::find<components::AddonList>(&rig().profile());
    const BLRect box = list.box();

    parts::click(rig().ui(), box.x + box.w - 19.0, box.y + 17.0);
    rig().turn();

    ASSERT_EQ(Session::get().config().activeProfile().files.size(), 11U);
    EXPECT_DOUBLE_EQ(list.reach(), 11 * components::AddonList::rowHeight());
}

TEST_F(AddonsPanelOnThePage, TheListReachesTheFilesAdded) {
    parts::install(rig(), grounded(12));
    rig().go(State::Page::Profile);
    rig().turn();

    components::AddonList &list = *parts::find<components::AddonList>(&rig().profile());

    rig().config().lists().addFiles({tests::Corpus::pwad(2).string(), tests::Corpus::pwad(3).string()});
    rig().turn();

    EXPECT_DOUBLE_EQ(list.reach(), 14 * components::AddonList::rowHeight());
}

}
