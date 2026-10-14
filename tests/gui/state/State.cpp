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

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "core/config/Config.h"
#include "gui/state/State.h"

namespace {

class StateTest : public ::testing::Test {
protected:
    void SetUp() override { State::get() = State::All{}; }

    void TearDown() override { State::get() = State::All{}; }
};

State::ProfileCard card() {
    return State::ProfileCard{
            .index = 2,
            .id = "id",
            .key = "profile:id",
            .name = "Name",
            .iwad = "Doom",
            .artKey = "/doom.wad",
            .port = "GZDoom",
            .dosPort = false,
            .warp = "MAP01",
            .files = 3,
            .loaded = 2,
            .netRole = NetRole::Alone,
            .ready = true,
            .netSupported = false,
            .replayMode = ReplayMode::Off,
    };
}

TEST_F(StateTest, GetAnswersTheSameTreeEveryTime) {
    EXPECT_EQ(&State::get(), &State::get());
}

TEST_F(StateTest, AFreshTreeStartsOnTheLibraryShowingProfiles) {
    const State::All &all = State::get();

    EXPECT_EQ(all.sys.page, State::Page::Library);
    EXPECT_EQ(all.nav.shelf, State::Shelf::Profiles);
    EXPECT_EQ(all.nav.engines, State::EnginesTab::Installed);
    EXPECT_FALSE(all.nav.tuning);
}

TEST_F(StateTest, AFreshTreeHasNoRevisionsAndNoActiveIndexes) {
    const State::Cfg &cfg = State::get().cfg;

    EXPECT_EQ(cfg.rev, 0);
    EXPECT_EQ(cfg.gameRev, 0);
    EXPECT_EQ(cfg.shelfRev, 0);
    EXPECT_EQ(cfg.filesRev, 0);
    EXPECT_EQ(cfg.replayIndex, -1);
    EXPECT_EQ(cfg.saveIndex, -1);
    EXPECT_EQ(State::get().sys.artRev, 0);
    EXPECT_EQ(State::get().runs.rev, 0);
}

TEST_F(StateTest, TouchWithNothingListeningDoesNothing) {
    State::get().touch();

    SUCCEED();
}

TEST_F(StateTest, TouchCallsWhatTheApplicationSet) {
    int calls = 0;

    State::get().changed = [&calls] { ++calls; };

    State::get().touch();
    State::get().touch();

    EXPECT_EQ(calls, 2);
}

TEST_F(StateTest, AssigningAFreshTreeDropsTheListener) {
    int calls = 0;

    State::get().changed = [&calls] { ++calls; };
    State::get() = State::All{};
    State::get().touch();

    EXPECT_EQ(calls, 0);
}

TEST_F(StateTest, AssigningAFreshTreeClearsWhatWasPushed) {
    State::get().cfg.profileCards.push_back(card());
    State::get().cfg.rev = 9;
    State::get().sys.page = State::Page::Settings;

    State::get() = State::All{};

    EXPECT_TRUE(State::get().cfg.profileCards.empty());
    EXPECT_EQ(State::get().cfg.rev, 0);
    EXPECT_EQ(State::get().sys.page, State::Page::Library);
}

TEST_F(StateTest, BadgesAreEqualOnlyWhenEveryFieldIs) {
    const State::BadgeSpec one{.text = "DOS", .kind = State::BadgeKind::Muted, .dot = true};

    EXPECT_EQ(one, (State::BadgeSpec{.text = "DOS", .kind = State::BadgeKind::Muted, .dot = true}));
    EXPECT_NE(one, (State::BadgeSpec{.text = "dos", .kind = State::BadgeKind::Muted, .dot = true}));
    EXPECT_NE(one, (State::BadgeSpec{.text = "DOS", .kind = State::BadgeKind::Danger, .dot = true}));
    EXPECT_NE(one, (State::BadgeSpec{.text = "DOS", .kind = State::BadgeKind::Muted, .dot = false}));
}

TEST_F(StateTest, ABadgeDefaultsToTheAccentToneWithNoDot) {
    const State::BadgeSpec plain{};

    EXPECT_EQ(plain.kind, State::BadgeKind::None);
    EXPECT_FALSE(plain.dot);
}

TEST_F(StateTest, ProfileCardsCompareEveryField) {
    const State::ProfileCard base = card();

    EXPECT_EQ(base, card());

    State::ProfileCard other = card();

    other.loaded = 3;

    EXPECT_NE(base, other);

    other = card();
    other.netSupported = true;

    EXPECT_NE(base, other);

    other = card();
    other.replayMode = ReplayMode::Play;

    EXPECT_NE(base, other);

    other = card();
    other.artKey = "/other.wad";

    EXPECT_NE(base, other);
}

TEST_F(StateTest, FileRowsCompareEveryField) {
    State::FileRow base;

    base.file = "/a.wad";
    base.name = "a.wad";
    base.loaded = true;

    State::FileRow other = base;

    EXPECT_EQ(base, other);

    other.missing = true;

    EXPECT_NE(base, other);

    other = base;
    other.loaded = false;

    EXPECT_NE(base, other);
}

TEST_F(StateTest, NameRowsCompareEveryField) {
    State::NameRow base;

    base.name = "Doom";
    base.file = "/doom.wad";

    for (int field = 0; field < 4; ++field) {
        State::NameRow other = base;

        if (field == 0) {
            other.dosbox = true;
        } else if (field == 1) {
            other.fetched = true;
        } else if (field == 2) {
            other.detected = true;
        } else {
            other.missing = true;
        }

        EXPECT_NE(base, other) << field;
    }

    EXPECT_EQ(base, State::NameRow(base));
}

TEST_F(StateTest, EngineRowsCompareTheirProgressAndAsking) {
    State::EngineRow base;

    base.name = "GZDoom";
    base.status = State::EngineState::Fetching;

    State::EngineRow other = base;

    EXPECT_EQ(base, other);

    other.progress = 0.5F;

    EXPECT_NE(base, other);

    other = base;
    other.asking = true;

    EXPECT_NE(base, other);

    other = base;
    other.status = State::EngineState::Unpacking;

    EXPECT_NE(base, other);
}

TEST_F(StateTest, AnEngineRowStartsWaiting) {
    EXPECT_EQ(State::EngineRow{}.status, State::EngineState::Waiting);
}

TEST_F(StateTest, LogRowsCompareTheLineAndWhoseItIs) {
    EXPECT_EQ((State::LogRow{.line = "x", .own = true}), (State::LogRow{.line = "x", .own = true}));
    EXPECT_NE((State::LogRow{.line = "x", .own = true}), (State::LogRow{.line = "x", .own = false}));
    EXPECT_NE((State::LogRow{.line = "x", .own = false}), (State::LogRow{.line = "y", .own = false}));
}

TEST_F(StateTest, ShelvesAndStartViewsShareTheirNumbers) {
    EXPECT_EQ(static_cast<int>(State::Shelf::Profiles), static_cast<int>(StartView::Profiles));
    EXPECT_EQ(static_cast<int>(State::Shelf::Games), static_cast<int>(StartView::Games));
}

TEST_F(StateTest, PagesAreNumberedInTheOrderTheTitleBarShowsThem) {
    EXPECT_EQ(static_cast<int>(State::Page::Library), 0);
    EXPECT_EQ(static_cast<int>(State::Page::Profile), 1);
    EXPECT_EQ(static_cast<int>(State::Page::Engines), 2);
    EXPECT_EQ(static_cast<int>(State::Page::Settings), 3);
}

TEST_F(StateTest, EngineTabsMatchTheSwitchValues) {
    EXPECT_EQ(static_cast<int>(State::EnginesTab::Installed), 0);
    EXPECT_EQ(static_cast<int>(State::EnginesTab::Browse), 1);
}

}
