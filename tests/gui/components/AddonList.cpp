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

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/draw/Theme.h"
#include "ttk/toolkit/Event.h"

#include "Mount.h"
#include "core/config/Session.h"
#include "gui/components/AddonList.h"
#include "gui/state/State.h"
#include "support/Rig.h"

namespace {

constexpr BLRect SPOT{20.0, 20.0, 480.0, 300.0};
constexpr double ROW = 34.0;

// Every file on disk, each loaded but every fourth from the first.
Config grounded(const int files) {
    Config made = tests::Fixtures::grounded(3, files);

    made.general.showPaths = false;

    return made;
}

class AddonListTest : public parts::Screened {
protected:
    AddonListTest() {
        wide = 640;
        tall = 480;
    }

    components::AddonList *make(Config what = grounded(5)) {
        parts::install(rig(), std::move(what));

        _list = parts::place(screen(), std::make_unique<components::AddonList>(&rig().reach()), SPOT);

        screen().full();

        return _list;
    }

    // The list reads the files from the state on every paint, so a bridge call is all a change needs.
    void settle() {
        screen().ui().relayout();
        screen().frame();
    }

    static double rowY(const int row) { return SPOT.y + (row * ROW) + (ROW / 2.0); }

    static double gripX() { return SPOT.x + 10.0; }
    static double checkX() { return SPOT.x + 33.0; }
    static double nameX() { return SPOT.x + 120.0; }
    static double crossX() { return SPOT.x + SPOT.w - 19.0; }

    static std::vector<FileEntry> &files() { return Session::get().config().activeProfile().files; }

    static std::vector<std::string> order() {
        std::vector<std::string> out;

        for (const FileEntry &entry : files()) {
            out.push_back(std::filesystem::path(entry.file).filename().string());
        }

        return out;
    }

    void carry(const int from, const double toY) {
        parts::sweep(ui(), gripX(), rowY(from), gripX(), toY);
        screen().run(20);
    }

    components::AddonList *_list = nullptr;
};

TEST_F(AddonListTest, RowsAre34TallOr46WithPaths) {
    State::get().cfg.showPaths = false;

    EXPECT_DOUBLE_EQ(components::AddonList::rowHeight(), 34.0);

    State::get().cfg.showPaths = true;

    EXPECT_DOUBLE_EQ(components::AddonList::rowHeight(), 46.0);
}

TEST_F(AddonListTest, ReachesARowPerFile) {
    make(grounded(5));

    EXPECT_DOUBLE_EQ(_list->reach(), 5 * ROW);
    EXPECT_FALSE(_list->scrollable());
}

TEST_F(AddonListTest, ReachesFurtherWithPathsShown) {
    Config what = grounded(5);

    what.general.showPaths = true;
    make(std::move(what));

    EXPECT_DOUBLE_EQ(_list->reach(), 5 * 46.0);
}

TEST_F(AddonListTest, ALongListScrolls) {
    make(grounded(12));

    EXPECT_TRUE(_list->scrollable());
}

TEST_F(AddonListTest, ClickingTheCheckUnloadsALoadedFile) {
    make();

    ASSERT_TRUE(files()[1].enabled);

    parts::click(ui(), checkX(), rowY(1));

    EXPECT_FALSE(files()[1].enabled);
    EXPECT_FALSE(State::get().cfg.files[1].loaded);
}

TEST_F(AddonListTest, ClickingTheCheckLoadsAnUnloadedFile) {
    make();

    ASSERT_FALSE(files()[0].enabled);

    parts::click(ui(), checkX(), rowY(0));

    EXPECT_TRUE(files()[0].enabled);
    EXPECT_EQ(State::get().cfg.enabledCount, 4);
}

TEST_F(AddonListTest, ClickingTheNameChangesNothing) {
    make();

    const std::vector<std::string> was = order();

    parts::click(ui(), nameX(), rowY(1));

    EXPECT_TRUE(files()[1].enabled);
    EXPECT_EQ(order(), was);
}

TEST_F(AddonListTest, ClickingTheCrossRemovesTheFile) {
    make();

    const std::string second = order()[1];

    parts::click(ui(), crossX(), rowY(1));

    const std::vector<std::string> left = order();

    ASSERT_EQ(left.size(), 4U);
    EXPECT_EQ(std::ranges::count(left, second), 0);
}

TEST_F(AddonListTest, ClickingBelowTheLastRowChangesNothing) {
    make();
    parts::click(ui(), checkX(), rowY(7));
    parts::click(ui(), crossX(), rowY(7));

    EXPECT_EQ(files().size(), 5U);
}

TEST_F(AddonListTest, APressBelowTheRowsIsNotTaken) {
    make();

    EXPECT_FALSE(_list->press(ttk::Pointer{.x = nameX(), .y = rowY(7)}));
    EXPECT_TRUE(_list->press(ttk::Pointer{.x = nameX(), .y = rowY(2)}));
}

TEST_F(AddonListTest, CarryingAFileByItsGripMovesIt) {
    make();

    const std::vector<std::string> was = order();

    carry(0, rowY(2));

    EXPECT_EQ(order(), (std::vector<std::string>{was[1], was[2], was[0], was[3], was[4]}));
}

TEST_F(AddonListTest, CarryingAFileUpMovesIt) {
    make();

    const std::vector<std::string> was = order();

    carry(3, rowY(1));

    EXPECT_EQ(order(), (std::vector<std::string>{was[0], was[3], was[1], was[2], was[4]}));
}

TEST_F(AddonListTest, CarryingPastTheEndLandsLast) {
    make();

    const std::vector<std::string> was = order();

    carry(1, SPOT.y + SPOT.h + 200.0);

    EXPECT_EQ(order().back(), was[1]);
}

TEST_F(AddonListTest, CarryingAboveTheTopLandsFirst) {
    make();

    const std::vector<std::string> was = order();

    carry(3, SPOT.y - 100.0);

    EXPECT_EQ(order().front(), was[3]);
}

TEST_F(AddonListTest, AFileDroppedWhereItStartedStays) {
    make();

    const std::vector<std::string> was = order();

    carry(2, rowY(2) + 8.0);

    EXPECT_EQ(order(), was);
}

TEST_F(AddonListTest, TheListChangesOnlyOnceTheRowHasWalkedToItsGap) {
    make();

    const std::vector<std::string> was = order();

    parts::sweep(ui(), gripX(), rowY(0), gripX(), rowY(3));

    EXPECT_EQ(order(), was);

    screen().run(20);

    EXPECT_NE(order(), was);
}

TEST_F(AddonListTest, APressWhileARowWalksLandsItAtOnce) {
    make();

    const std::vector<std::string> was = order();

    parts::sweep(ui(), gripX(), rowY(0), gripX(), rowY(3));
    ui().press(ttk::Pointer{.x = nameX(), .y = rowY(4)});

    EXPECT_EQ(order().at(3), was[0]);
}

TEST_F(AddonListTest, PressingTheNameDoesNotCarry) {
    make();

    const std::vector<std::string> was = order();

    parts::sweep(ui(), nameX(), rowY(0), nameX(), rowY(3));
    screen().run(20);

    EXPECT_EQ(order(), was);
}

TEST_F(AddonListTest, ACarryRepaintsLikeAFullFrame) {
    make();
    ui().motion(gripX(), rowY(0));
    ui().press(ttk::Pointer{.x = gripX(), .y = rowY(0)});
    ui().motion(gripX(), rowY(2) + 5.0);
    screen().frame();

    const parts::Ink off = parts::mismatch(screen());

    EXPECT_EQ(off.pixels, 0U) << off;
}

TEST_F(AddonListTest, ALandingRepaintsLikeAFullFrame) {
    make();
    carry(0, rowY(3));
    settle();

    const parts::Ink off = parts::mismatch(screen());

    EXPECT_EQ(off.pixels, 0U) << off;
}

TEST_F(AddonListTest, TheGripShowsTheResizeCursor) {
    make();

    EXPECT_EQ(_list->cursor_at(gripX(), rowY(1)), ttk::Cursor::Resize);
}

TEST_F(AddonListTest, TheCheckAndTheCrossShowThePointingHand) {
    make();

    EXPECT_EQ(_list->cursor_at(checkX(), rowY(1)), ttk::Cursor::Pointer);
    EXPECT_EQ(_list->cursor_at(crossX(), rowY(1)), ttk::Cursor::Pointer);
}

TEST_F(AddonListTest, TheNameAndTheSpaceUnderTheRowsShowTheArrow) {
    make();

    EXPECT_EQ(_list->cursor_at(nameX(), rowY(1)), ttk::Cursor::Default);
    EXPECT_EQ(_list->cursor_at(gripX(), rowY(7)), ttk::Cursor::Default);
}

TEST_F(AddonListTest, TheLaneOfALongListIsNoRow) {
    make(grounded(12));

    const double lane = SPOT.x + SPOT.w - (ttk::Theme::lane / 2.0);

    EXPECT_EQ(_list->cursor_at(lane, rowY(1)), ttk::Cursor::Default);

    parts::click(ui(), lane, rowY(1));

    EXPECT_EQ(files().size(), 12U);
}

TEST_F(AddonListTest, TheWheelScrollsALongList) {
    make(grounded(12));
    ui().wheel(-3.0, nameX(), rowY(2));
    screen().run(30);

    EXPECT_GT(_list->offset(), 0.0);
}

TEST_F(AddonListTest, AScrolledListClicksTheRowUnderThePointer) {
    make(grounded(12));
    _list->scroll_to(ROW * 2.0);
    screen().frame();

    ASSERT_DOUBLE_EQ(_list->offset(), ROW * 2.0);

    parts::click(ui(), checkX(), rowY(0));

    EXPECT_FALSE(files()[0].enabled);
    EXPECT_FALSE(files()[2].enabled);
}

TEST_F(AddonListTest, TheWheelLeavesAShortListAlone) {
    make();
    ui().wheel(-3.0, nameX(), rowY(2));
    screen().run(30);

    EXPECT_DOUBLE_EQ(_list->offset(), 0.0);
}

TEST_F(AddonListTest, AnEmptyListSaysNothingIsLoaded) {
    make(grounded(0));

    const parts::Ink said = parts::ink(screen(), parts::pixels_of(SPOT), ttk::Theme::palette().background.value);

    EXPECT_GT(said.pixels, 0U);
    // The crossbar of a DejaVu "T" bleeds a pixel left of its pen.
    EXPECT_GE(said.left, static_cast<int>(SPOT.x) + 23);
}

TEST_F(AddonListTest, AnEmptyListTakesNoPress) {
    make(grounded(0));

    EXPECT_FALSE(_list->press(ttk::Pointer{.x = nameX(), .y = rowY(0)}));
}

TEST_F(AddonListTest, ALoadedFileHasAFilledCheck) {
    make();

    const ttk::Theme::Palette &palette = ttk::Theme::palette();

    EXPECT_TRUE(parts::leans(
            screen().at(static_cast<int>(SPOT.x) + 27, static_cast<int>(rowY(1)) + 5), palette.accent, palette.field));
    EXPECT_TRUE(parts::leans(
            screen().at(static_cast<int>(SPOT.x) + 27, static_cast<int>(rowY(0)) + 5), palette.field, palette.accent));
}

// The pointer is already on the row, so no hover change repaints it for the click.
TEST_F(AddonListTest, AClickedCheckRepaintsWithoutALayout) {
    make();

    const ttk::Theme::Palette &palette = ttk::Theme::palette();
    const int x = static_cast<int>(SPOT.x) + 27;
    const int y = static_cast<int>(rowY(1)) + 5;

    ui().motion(checkX(), rowY(1));
    screen().frame();

    ASSERT_TRUE(parts::leans(screen().at(x, y), palette.accent, palette.field));

    parts::click(ui(), checkX(), rowY(1));
    _list->sync();
    screen().frame();

    EXPECT_TRUE(parts::leans(screen().at(x, y), palette.field, palette.accent));

    const parts::Ink off = parts::mismatch(screen());

    EXPECT_EQ(off.pixels, 0U) << off;
}

TEST_F(AddonListTest, ARemovedFileShortensTheReachWithoutALayout) {
    make(grounded(12));

    ASSERT_DOUBLE_EQ(_list->reach(), 12 * ROW);

    parts::click(ui(), crossX(), rowY(1));
    _list->sync();

    EXPECT_DOUBLE_EQ(_list->reach(), 11 * ROW);
}

TEST_F(AddonListTest, SyncingAgainWithNothingChangedDamagesNothing) {
    make();

    _list->sync();
    screen().frame();

    _list->sync();

    EXPECT_EQ(screen().frame(), 0U);
}

TEST_F(AddonListTest, AMissingFileIsWrittenInDanger) {
    make(parts::onPort("gzdoom", 3));

    const BLRectI name{static_cast<int>(SPOT.x) + 52, static_cast<int>(SPOT.y) + 34, 300, 34};

    EXPECT_GT(parts::count_near(screen(), name, ttk::Theme::palette().danger, 60), 0);
}

TEST_F(AddonListTest, AFileOnDiskIsNotWrittenInDanger) {
    make();

    const BLRectI name{static_cast<int>(SPOT.x) + 52, static_cast<int>(SPOT.y) + 34, 300, 34};

    EXPECT_EQ(parts::count_near(screen(), name, ttk::Theme::palette().danger, 60), 0);
}

TEST_F(AddonListTest, HoverShowsTheCrossOnlyOnTheRowUnderThePointer) {
    make();

    const BLRectI first{static_cast<int>(crossX()) - 6, static_cast<int>(rowY(1)) - 6, 12, 12};
    const BLRectI second{static_cast<int>(crossX()) - 6, static_cast<int>(rowY(2)) - 6, 12, 12};
    const std::vector<std::uint32_t> resting = parts::pixels_in(screen(), second);
    const std::uint32_t ground = ttk::Theme::palette().background.value;

    EXPECT_EQ(parts::ink(screen(), first, ground).pixels, 0U);

    ui().motion(nameX(), rowY(1));
    screen().frame();

    EXPECT_GT(parts::ink(screen(), first, ground).pixels, 0U);
    EXPECT_EQ(parts::pixels_in(screen(), second), resting);
}

TEST_F(AddonListTest, HoveringTheCrossTurnsItToDanger) {
    make();

    const BLRectI cross{static_cast<int>(crossX()) - 6, static_cast<int>(rowY(1)) - 6, 12, 12};

    ui().motion(nameX(), rowY(1));
    screen().frame();

    EXPECT_EQ(parts::count_near(screen(), cross, ttk::Theme::palette().danger, 60), 0);

    ui().motion(crossX(), rowY(1));
    screen().frame();

    EXPECT_GT(parts::count_near(screen(), cross, ttk::Theme::palette().danger, 60), 0);
}

TEST_F(AddonListTest, HoveringDownTheRowsRepaintsLikeAFullFrame) {
    make();

    for (int row = 0; row < 5; ++row) {
        ui().motion(gripX(), rowY(row));
        screen().frame();
        ui().motion(crossX(), rowY(row));
        screen().frame();
    }

    ui().leave();
    screen().frame();

    const parts::Ink off = parts::mismatch(screen());

    EXPECT_EQ(off.pixels, 0U) << off;
}

}
