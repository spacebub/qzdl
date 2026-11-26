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
#include "ttk/toolkit/Root.h"
#include "ttk/util/Format.h"

#include "Mount.h"
#include "core/config/Session.h"
#include "gui/components/AddonList.h"
#include "gui/draw/Band.h"
#include "gui/state/State.h"
#include "support/Rig.h"
#include "support/Sandbox.h"

namespace {

constexpr BLRect SPOT{20.0, 20.0, 480.0, 300.0};
constexpr double ROW = 34.0;
constexpr double HEAD = 30.0;
constexpr double GAP = 8.0;

// Every file on disk, each loaded but every fourth from the first.
Config grounded(const int files) {
    Config made = tests::Fixtures::grounded(3, files);

    made.general.showPaths = false;

    return made;
}

// Maps holds the second and fifth file, Gameplay the first and fourth, and the third is ungrouped.
Config sorted(const int files = 5) {
    Config made = grounded(files);
    Profile &profile = made.profiles.front();

    made.general.groupAddons = true;
    profile.fileGroups = {{.id = 1, .name = "Maps", .collapsed = false},
                          {.id = 2, .name = "Gameplay", .collapsed = false}};

    for (size_t at = 0; at < profile.files.size(); ++at) {
        profile.files[at].group = at % 3 == 0 ? 2 : at % 3 == 1 ? 1 : 0;
    }

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

    static double lineHeight(const int line) {
        const std::vector<State::AddonLine> &lines = State::get().cfg.addonLines;

        return std::cmp_less(line, lines.size()) && lines[static_cast<size_t>(line)].file < 0 ? HEAD : ROW;
    }

    // The middle of a line. A header is 30 tall, and every section after the first opens with a gap.
    static double rowY(const int row) {
        const std::vector<State::AddonLine> &lines = State::get().cfg.addonLines;
        const int gaps = std::cmp_less(row, lines.size()) ? std::max(0, lines[static_cast<size_t>(row)].section) : 0;
        double y = SPOT.y + (gaps * GAP);

        for (int line = 0; line < row; ++line) {
            y += lineHeight(line);
        }

        return y + (lineHeight(row) / 2.0);
    }

    // The middle of the gap above the header at `row`.
    static double gapAbove(const int row) { return rowY(row) - ((HEAD + GAP) / 2.0); }

    static double gripX() { return SPOT.x + 10.0; }
    static double checkX() { return SPOT.x + 33.0; }
    static double nameX() { return SPOT.x + 120.0; }
    static double eyeX() { return SPOT.x + SPOT.w - 75.0; }
    static double folderX() { return SPOT.x + SPOT.w - 47.0; }
    static double trashX() { return SPOT.x + SPOT.w - 19.0; }

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

    // With sections the row is held by its body.
    void hold(const int from, const double toY) {
        parts::sweep(ui(), nameX(), rowY(from), nameX(), toY);
        screen().run(5);
    }

    static double rowCheckX() { return SPOT.x + 55.0; }

    static std::vector<int> groups() {
        std::vector<int> out;

        for (const FileEntry &entry : files()) {
            out.push_back(entry.group);
        }

        return out;
    }

    static std::vector<Group> &fileGroups() { return Session::get().config().activeProfile().fileGroups; }

    ttk::Widget *popups() { return ui().layer(ttk::Root::POPUPS); }

    // Rows of the group menu are 32 tall under a pad of 5.
    void pick(const int header, const int row) {
        parts::click(ui(), trashX(), rowY(header));

        ASSERT_EQ(popups()->children().size(), 1U);

        const BLRect &menu = popups()->children().front()->box();

        parts::click(ui(), menu.x + 60.0, menu.y + 5.0 + (row * 32.0) + 16.0);
    }

    components::AddonList *_list = nullptr;
};

TEST_F(AddonListTest, DroppedFilesGoInAtTheGapNearestThePointer) {
    make(grounded(5));

    // A little past the line between the second and third rows.
    const double y = SPOT.y + (2 * ROW) + 5.0;

    ui().drag_files(nameX(), y);

    EXPECT_EQ(ui().drag_target(), _list);
    EXPECT_TRUE(ui().drop_files({"/drops/new.wad"}, nameX(), y));
    EXPECT_EQ(ui().drag_target(), nullptr);

    ASSERT_EQ(files().size(), 6U);
    EXPECT_EQ(order()[2], "new.wad");
    EXPECT_TRUE(files()[2].enabled);
}

TEST_F(AddonListTest, TheGapIsTheNearestOneAndNotTheRowAbove) {
    make(grounded(5));

    // Most of the way down the second row, so the line below it is nearer.
    EXPECT_TRUE(ui().drop_files({"/drops/new.wad"}, nameX(), SPOT.y + ROW + (ROW * 0.8)));

    EXPECT_EQ(order()[2], "new.wad");
}

TEST_F(AddonListTest, AScrolledListTakesADropAtTheGapUnderThePointer) {
    make(grounded(12));
    _list->scroll_to(ROW * 2.0);
    screen().frame();

    ASSERT_DOUBLE_EQ(_list->offset(), ROW * 2.0);

    EXPECT_TRUE(ui().drop_files({"/drops/new.wad"}, nameX(), SPOT.y + ROW + 3.0));

    EXPECT_EQ(order()[3], "new.wad");
}

TEST_F(AddonListTest, DraggingFilesOverRepaintsLikeAFullFrame) {
    make();

    ui().drag_files(nameX(), rowY(1));
    screen().frame();

    EXPECT_EQ(parts::mismatch(screen()).pixels, 0U) << parts::mismatch(screen());

    ui().drag_files(nameX(), rowY(3) + 10.0);
    screen().frame();

    EXPECT_EQ(parts::mismatch(screen()).pixels, 0U) << parts::mismatch(screen());

    ui().drag_files_ended();
    screen().frame();

    EXPECT_EQ(parts::mismatch(screen()).pixels, 0U) << parts::mismatch(screen());
}

TEST_F(AddonListTest, DroppedFilesBelowTheLastRowGoOnTheEnd) {
    make(grounded(3));

    EXPECT_TRUE(ui().drop_files({"/drops/a.wad", "/drops/b.wad"}, nameX(), SPOT.y + SPOT.h - 10.0));

    ASSERT_EQ(files().size(), 5U);
    EXPECT_EQ(order()[3], "a.wad");
    EXPECT_EQ(order()[4], "b.wad");
}

TEST_F(AddonListTest, DroppedFilesAboveTheFirstRowGoFirst) {
    make(grounded(3));

    EXPECT_TRUE(ui().drop_files({"/drops/a.wad"}, nameX(), SPOT.y + 4.0));

    EXPECT_EQ(order().front(), "a.wad");
}

TEST_F(AddonListTest, OnlyAddOnsAndFoldersAreTakenFromADrop) {
    make(grounded(2));

    const std::string folder = tests::Sandbox::scratch("drop-folder").string();

    EXPECT_TRUE(ui().drop_files({"/drops/a.wad", "/drops/notes.txt", folder, "/drops/b.zdl"}, nameX(), SPOT.y + 4.0));

    ASSERT_EQ(files().size(), 4U);
    EXPECT_EQ(files()[0].file, "/drops/a.wad");
    EXPECT_EQ(files()[1].file, ttk::Format::from_path(folder));
    ASSERT_FALSE(rig().notify().messages().empty());
    EXPECT_EQ(rig().notify().messages().back().body, "notes.txt and b.zdl are not add-ons, so they were left out.");
}

TEST_F(AddonListTest, AnEmptyListTakesDroppedFiles) {
    make(grounded(0));

    ui().drag_files(nameX(), SPOT.y + 40.0);

    EXPECT_EQ(ui().drag_target(), _list);
    EXPECT_TRUE(ui().drop_files({"/drops/a.wad"}, nameX(), SPOT.y + 40.0));
    EXPECT_EQ(order(), (std::vector<std::string>{"a.wad"}));
}

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

TEST_F(AddonListTest, DoubleClickingTheNameShowsTheReadmes) {
    make();

    parts::click(ui(), nameX(), rowY(1));
    EXPECT_TRUE(rig().readmesShown.empty());

    parts::click(ui(), nameX(), rowY(1));
    ASSERT_EQ(rig().readmesShown.size(), 1U);
    EXPECT_EQ(rig().readmesShown[0], files()[1].file);
}

TEST_F(AddonListTest, TheEyeShowsTheReadmes) {
    make();

    parts::click(ui(), eyeX(), rowY(2));

    ASSERT_EQ(rig().readmesShown.size(), 1U);
    EXPECT_EQ(rig().readmesShown[0], files()[2].file);
}

TEST_F(AddonListTest, SlowClicksOrClicksOnTwoRowsShowNothing) {
    make();

    parts::click(ui(), nameX(), rowY(1));
    screen().frameAt(screen().now() + 0.5);
    parts::click(ui(), nameX(), rowY(1));
    parts::click(ui(), nameX(), rowY(2));

    EXPECT_TRUE(rig().readmesShown.empty());
}

TEST_F(AddonListTest, ClickingTheTrashRemovesTheFile) {
    make();

    const std::string second = order()[1];

    parts::click(ui(), trashX(), rowY(1));

    const std::vector<std::string> left = order();

    ASSERT_EQ(left.size(), 4U);
    EXPECT_EQ(std::ranges::count(left, second), 0);
}

TEST_F(AddonListTest, ClickingBelowTheLastRowChangesNothing) {
    make();
    parts::click(ui(), checkX(), rowY(7));
    parts::click(ui(), trashX(), rowY(7));

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

TEST_F(AddonListTest, TheCheckAndTheRowButtonsShowThePointingHand) {
    make();

    EXPECT_EQ(_list->cursor_at(checkX(), rowY(1)), ttk::Cursor::Pointer);
    EXPECT_EQ(_list->cursor_at(eyeX(), rowY(1)), ttk::Cursor::Pointer);
    EXPECT_EQ(_list->cursor_at(folderX(), rowY(1)), ttk::Cursor::Pointer);
    EXPECT_EQ(_list->cursor_at(trashX(), rowY(1)), ttk::Cursor::Pointer);
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

    parts::click(ui(), trashX(), rowY(1));
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
    make(parts::onEngine("gzdoom", 3));

    const BLRectI name{static_cast<int>(SPOT.x) + 52, static_cast<int>(SPOT.y) + 34, 300, 34};

    EXPECT_GT(parts::count_near(screen(), name, ttk::Theme::palette().danger, 60), 0);
}

TEST_F(AddonListTest, AFileOnDiskIsNotWrittenInDanger) {
    make();

    const BLRectI name{static_cast<int>(SPOT.x) + 52, static_cast<int>(SPOT.y) + 34, 300, 34};

    EXPECT_EQ(parts::count_near(screen(), name, ttk::Theme::palette().danger, 60), 0);
}

TEST_F(AddonListTest, HoverShowsTheTrashOnlyOnTheRowUnderThePointer) {
    make();

    const BLRectI first{static_cast<int>(trashX()) - 6, static_cast<int>(rowY(1)) - 6, 12, 12};
    const BLRectI second{static_cast<int>(trashX()) - 6, static_cast<int>(rowY(2)) - 6, 12, 12};
    const std::vector<std::uint32_t> resting = parts::pixels_in(screen(), second);
    const std::uint32_t ground = ttk::Theme::palette().background.value;

    EXPECT_EQ(parts::ink(screen(), first, ground).pixels, 0U);

    ui().motion(nameX(), rowY(1));
    screen().frame();

    EXPECT_GT(parts::ink(screen(), first, ground).pixels, 0U);
    EXPECT_EQ(parts::pixels_in(screen(), second), resting);
}

TEST_F(AddonListTest, HoveringTheTrashTurnsItToDanger) {
    make();

    const BLRectI trash{static_cast<int>(trashX()) - 6, static_cast<int>(rowY(1)) - 6, 12, 12};

    ui().motion(nameX(), rowY(1));
    screen().frame();

    EXPECT_EQ(parts::count_near(screen(), trash, ttk::Theme::palette().danger, 60), 0);

    ui().motion(trashX(), rowY(1));
    screen().frame();

    EXPECT_GT(parts::count_near(screen(), trash, ttk::Theme::palette().danger, 60), 0);
}

TEST_F(AddonListTest, HoveringDownTheRowsRepaintsLikeAFullFrame) {
    make();

    for (int row = 0; row < 5; ++row) {
        ui().motion(gripX(), rowY(row));
        screen().frame();
        ui().motion(trashX(), rowY(row));
        screen().frame();
    }

    ui().leave();
    screen().frame();

    const parts::Ink off = parts::mismatch(screen());

    EXPECT_EQ(off.pixels, 0U) << off;
}

// Lines: Maps 0, files 1 and 4, Gameplay 3, files 0 and 3, Ungrouped 6, file 2.

TEST_F(AddonListTest, ReachesALinePerHeaderAndShownFile) {
    make(sorted());

    EXPECT_DOUBLE_EQ(_list->reach(), (3 * HEAD) + (5 * ROW) + (2 * GAP));
}

TEST_F(AddonListTest, APressOnTheHeaderAndOnTheFirstRowBelowItHitsEach) {
    make(sorted());

    ASSERT_TRUE(files()[1].enabled);

    parts::click(ui(), rowCheckX(), SPOT.y + HEAD + 1.0);

    EXPECT_FALSE(files()[1].enabled);
    EXPECT_FALSE(fileGroups()[0].collapsed);

    parts::click(ui(), nameX(), SPOT.y + HEAD - 1.0);
    screen().run(20);

    EXPECT_TRUE(fileGroups()[0].collapsed);
}

TEST_F(AddonListTest, TheHeaderMenuAnswersOnlyInsideItsBox) {
    make(sorted());

    const BLRect button = Band::menuBox(BLRect{SPOT.x, SPOT.y, SPOT.w, HEAD});

    parts::click(ui(), button.x - 1.0, rowY(0));
    screen().run(20);

    EXPECT_TRUE(popups()->children().empty());
    EXPECT_TRUE(fileGroups()[0].collapsed);

    parts::click(ui(), button.x + 1.0, rowY(0));

    EXPECT_EQ(popups()->children().size(), 1U);
}

TEST_F(AddonListTest, TheHeaderMenuShowsThePointingHand) {
    make(sorted());

    EXPECT_EQ(_list->cursor_at(trashX(), rowY(0)), ttk::Cursor::Pointer);
}

TEST_F(AddonListTest, AFoldSlidesTheRowsShutThenFoldsTheGroup) {
    make(sorted());

    const double open = _list->reach();

    ui().motion(nameX(), rowY(0));
    parts::click(ui(), nameX(), rowY(0));
    screen().run(6);

    // Mid slide the group is still open in the config, and the lines below have moved up by what it hides.
    const double hidden = open - _list->reach();
    const double cut = SPOT.y + HEAD + (2 * ROW) - hidden;

    EXPECT_FALSE(fileGroups()[0].collapsed);
    EXPECT_GT(hidden, 0.0);
    EXPECT_LT(hidden, 2 * ROW);

    const ttk::Theme::Palette &palette = ttk::Theme::palette();
    const auto lane = [](const double middle) {
        return BLRectI{static_cast<int>(SPOT.x) + 4, static_cast<int>(middle) - 7, 14, 14};
    };

    EXPECT_GT(parts::ink(screen(), lane(rowY(3) - hidden), palette.background.value).pixels, 0U);
    const BLRectI gap{static_cast<int>(SPOT.x) + 40, static_cast<int>(cut) + 1, 200, static_cast<int>(GAP) - 2};

    EXPECT_EQ(parts::ink(screen(), gap, palette.background.value).pixels, 0U);

    screen().run(20);
    _list->sync();
    screen().frame();

    EXPECT_TRUE(fileGroups()[0].collapsed);
    EXPECT_DOUBLE_EQ(_list->reach(), (3 * HEAD) + (3 * ROW) + (2 * GAP));
    EXPECT_EQ(screen().frame(), 0U);
    EXPECT_EQ(parts::mismatch(screen()).pixels, 0U);
}

TEST_F(AddonListTest, ANamedHeaderHasTheChevronAndUngroupedHasNone) {
    make(sorted());

    const std::uint32_t ground = ttk::Theme::palette().background.value;
    const auto lane = [](const int row) {
        return BLRectI{static_cast<int>(SPOT.x) + 4, static_cast<int>(rowY(row)) - 7, 14, 14};
    };

    EXPECT_GT(parts::ink(screen(), lane(0), ground).pixels, 0U);
    EXPECT_EQ(parts::ink(screen(), lane(6), ground).pixels, 0U);
}

TEST_F(AddonListTest, AGroupWithSomeTickedShowsADash) {
    make(sorted());

    const BLRectI some{static_cast<int>(SPOT.x) + 24, static_cast<int>(rowY(0)) - 9, 18, 18};
    const BLRectI all{static_cast<int>(SPOT.x) + 24, static_cast<int>(rowY(6)) - 9, 18, 18};
    const ttk::Theme::Palette &palette = ttk::Theme::palette();

    EXPECT_TRUE(parts::leans(screen().at(some.x + 3, some.y + 5), palette.accent, palette.field));
    EXPECT_GT(parts::changed(parts::pixels_in(screen(), some), parts::pixels_in(screen(), all)), 0);
}

TEST_F(AddonListTest, ClickingAHeaderFoldsItsGroup) {
    make(sorted());
    parts::click(ui(), nameX(), rowY(0));
    screen().run(20);
    _list->sync();

    EXPECT_TRUE(fileGroups()[0].collapsed);
    EXPECT_DOUBLE_EQ(_list->reach(), (3 * HEAD) + (3 * ROW) + (2 * GAP));

    parts::click(ui(), gripX(), rowY(0));

    EXPECT_FALSE(fileGroups()[0].collapsed);
}

TEST_F(AddonListTest, TheUngroupedHeaderDoesNotFold) {
    make(sorted());

    const std::vector<State::AddonLine> was = State::get().cfg.addonLines;

    parts::click(ui(), nameX(), rowY(6));

    EXPECT_EQ(State::get().cfg.addonLines, was);
}

TEST_F(AddonListTest, TheHeaderCheckTicksTheWholeGroup) {
    make(sorted());

    ASSERT_FALSE(files()[4].enabled);

    parts::click(ui(), checkX(), rowY(0));

    EXPECT_TRUE(files()[1].enabled);
    EXPECT_TRUE(files()[4].enabled);

    parts::click(ui(), checkX(), rowY(6));

    EXPECT_FALSE(files()[2].enabled);
}

TEST_F(AddonListTest, AFoldRepaintsWithoutALayout) {
    make(sorted());
    ui().motion(nameX(), rowY(0));
    screen().frame();

    parts::click(ui(), nameX(), rowY(0));
    screen().run(20);
    _list->sync();
    screen().frame();

    const parts::Ink off = parts::mismatch(screen());

    EXPECT_EQ(off.pixels, 0U) << off;
}

TEST_F(AddonListTest, TheHeaderMenuRenamesWithThePrompt) {
    make(sorted());
    pick(0, 0);

    EXPECT_TRUE(popups()->children().empty());
    ASSERT_EQ(rig().prompts.size(), 1U);
    EXPECT_EQ(rig().prompts.front().value, "Maps");

    rig().prompts.front().accepted("Levels");

    EXPECT_EQ(fileGroups()[0].name, "Levels");
}

TEST_F(AddonListTest, TheHeaderMenuMovesTheGroup) {
    make(sorted());
    pick(0, 2);

    EXPECT_EQ(fileGroups()[0].name, "Gameplay");

    pick(3, 1);

    EXPECT_EQ(fileGroups()[0].name, "Maps");
}

TEST_F(AddonListTest, TheHeaderMenuDeletesTheGroupWithoutAsking) {
    make(sorted());
    pick(0, 3);

    EXPECT_TRUE(rig().asks.empty());
    ASSERT_EQ(fileGroups().size(), 1U);
    EXPECT_EQ(files().size(), 5U);
    EXPECT_EQ(files()[1].group, 0);
}

TEST_F(AddonListTest, TheUngroupedHeaderHasNoMenu) {
    make(sorted());
    parts::click(ui(), trashX(), rowY(6));

    EXPECT_TRUE(popups()->children().empty());
    EXPECT_EQ(files().size(), 5U);
}

TEST_F(AddonListTest, AGroupedRowHasNoGrip) {
    make(sorted());

    EXPECT_EQ(_list->cursor_at(gripX(), rowY(1)), ttk::Cursor::Grab);
    EXPECT_EQ(_list->cursor_at(nameX(), rowY(1)), ttk::Cursor::Grab);
    EXPECT_EQ(_list->cursor_at(rowCheckX(), rowY(1)), ttk::Cursor::Pointer);
}

TEST_F(AddonListTest, AGroupedRowActsAsBefore) {
    make(sorted());
    parts::click(ui(), rowCheckX(), rowY(1));

    EXPECT_FALSE(files()[1].enabled);

    parts::click(ui(), eyeX(), rowY(4));

    ASSERT_EQ(rig().readmesShown.size(), 1U);
    EXPECT_EQ(rig().readmesShown[0], files()[0].file);

    parts::click(ui(), nameX(), rowY(5));
    parts::click(ui(), nameX(), rowY(5));

    ASSERT_EQ(rig().readmesShown.size(), 2U);
    EXPECT_EQ(rig().readmesShown[1], files()[3].file);

    const std::string gone = files()[2].file;

    parts::click(ui(), trashX(), rowY(7));

    EXPECT_EQ(files().size(), 4U);
    EXPECT_TRUE(std::ranges::none_of(files(), [&gone](const FileEntry &entry) { return entry.file == gone; }));
}

TEST_F(AddonListTest, APressWithinTheSlackIsAClick) {
    make(sorted());

    const std::vector<int> was = groups();

    parts::sweep(ui(), nameX(), rowY(1), nameX() + 2.0, rowY(1) + 2.0);
    parts::sweep(ui(), nameX(), rowY(1), nameX() + 2.0, rowY(1) + 2.0);

    EXPECT_EQ(groups(), was);
    EXPECT_EQ(rig().readmesShown.size(), 1U);
}

TEST_F(AddonListTest, ACarryOntoAnotherGroupSetsTheGroupAndMovesNothing) {
    make(sorted());

    const std::vector<std::string> was = order();

    hold(1, rowY(4));

    EXPECT_EQ(order(), was);
    EXPECT_EQ(groups(), (std::vector<int>{2, 2, 0, 2, 1}));

    // Lines: Maps 0, file 4, Gameplay 2, files 0, 1 and 3, Ungrouped 6, file 2.
    hold(1, rowY(7));

    EXPECT_EQ(groups(), (std::vector<int>{2, 2, 0, 2, 0}));
    EXPECT_EQ(order(), was);
}

TEST_F(AddonListTest, ACarryInsideItsGroupChangesNothing) {
    make(sorted());

    const std::vector<std::string> was = order();
    const int rev = State::get().cfg.filesRev;

    hold(1, rowY(2) + 8.0);

    EXPECT_EQ(order(), was);
    EXPECT_EQ(groups(), (std::vector<int>{2, 1, 0, 2, 1}));
    EXPECT_EQ(State::get().cfg.filesRev, rev);
}

TEST_F(AddonListTest, AFoldedHeaderTakesACarry) {
    Config what = sorted();

    what.profiles.front().fileGroups.front().collapsed = true;
    make(std::move(what));

    // Lines: Maps 0, Gameplay 1, files 0 and 3, Ungrouped 4, file 2.
    hold(5, rowY(0));

    EXPECT_EQ(files()[2].group, 1);
}

TEST_F(AddonListTest, TheGroupUnderACarryLights) {
    make(sorted());
    ui().motion(nameX(), rowY(1));
    ui().press(ttk::Pointer{.x = nameX(), .y = rowY(1)});
    ui().motion(nameX(), rowY(4));
    screen().frame();

    const ttk::Theme::Palette &palette = ttk::Theme::palette();
    const auto lit = [this, &palette](const int row) {
        const BLRectI line{static_cast<int>(SPOT.x) + 300, static_cast<int>(rowY(row)) - 4, 40, 8};

        return parts::count_near(screen(), line, palette.accentSoft, 6);
    };

    EXPECT_GT(lit(3), 0);
    EXPECT_GT(lit(5), 0);
    EXPECT_EQ(lit(2), 0);
    EXPECT_EQ(lit(6), 0);

    const BLRectI name{static_cast<int>(SPOT.x) + 52, static_cast<int>(rowY(3)) - 8, 80, 16};

    EXPECT_GT(parts::count_near(screen(), name, palette.accent, 60), 0);
    EXPECT_EQ(parts::mismatch(screen()).pixels, 0U);
}

TEST_F(AddonListTest, APressInAGapHitsNothingAndTheLinesBelowKeepTheirPlace) {
    make(sorted());

    const double gameplayGap = gapAbove(3);
    const double ungroupedGap = gapAbove(6);

    EXPECT_EQ(_list->cursor_at(nameX(), gameplayGap), ttk::Cursor::Default);

    parts::click(ui(), nameX(), gameplayGap);
    parts::click(ui(), nameX(), ungroupedGap);

    EXPECT_FALSE(fileGroups()[1].collapsed);

    ASSERT_FALSE(files()[0].enabled);

    // The foot of the first Gameplay file.
    parts::click(ui(), rowCheckX(), rowY(4) + (ROW / 2.0) - 2.0);

    EXPECT_TRUE(files()[0].enabled);
    EXPECT_TRUE(files()[3].enabled);
}

TEST_F(AddonListTest, TheCarriedRowFollowsThePointerAndTheRestStayStill) {
    make(sorted());
    ui().motion(nameX(), rowY(1));
    ui().press(ttk::Pointer{.x = nameX(), .y = rowY(1)});
    ui().motion(nameX(), rowY(4) + 6.0);
    screen().frame();

    const ttk::Theme::Palette &palette = ttk::Theme::palette();
    const auto lifted = [this, &palette](const double y) {
        const BLRectI line{static_cast<int>(SPOT.x) + 300, static_cast<int>(y) - 4, 40, 8};

        return parts::count_near(screen(), line, palette.mutedSoft, 4);
    };
    const BLRectI left{static_cast<int>(SPOT.x) + 52, static_cast<int>(rowY(1)) - 8, 200, 16};
    const BLRectI under{static_cast<int>(SPOT.x) + 74, static_cast<int>(rowY(5)) - 8, 200, 16};

    EXPECT_GT(lifted(rowY(4) + 6.0), 0);
    EXPECT_EQ(lifted(rowY(1)), 0);
    EXPECT_EQ(parts::ink(screen(), left, palette.background.value).pixels, 0U);
    EXPECT_GT(parts::ink(screen(), under, palette.accentSoft.value).pixels, 0U);

    ui().release(ttk::Pointer{.x = nameX(), .y = rowY(4) + 6.0});
    screen().run(20);

    EXPECT_EQ(files()[1].group, 2);
    EXPECT_EQ(parts::mismatch(screen()).pixels, 0U);
}

TEST_F(AddonListTest, AnEdgeScrollLightsTheGroupBelowAGap) {
    make(sorted(12));
    _list->scroll_to(_list->reach() - SPOT.h);
    screen().frame();

    // Lines: Maps 0, files 1 to 4, Gameplay 5, files 6 to 9, Ungrouped 10, files 11 to 14.
    const double from = rowY(12) - _list->offset();

    ui().motion(nameX(), from);
    ui().press(ttk::Pointer{.x = nameX(), .y = from});
    ui().motion(nameX(), SPOT.y + 2.0);
    screen().run(60);

    ASSERT_DOUBLE_EQ(_list->offset(), 0.0);

    const double gap = gapAbove(5);

    ui().motion(nameX(), gap);
    screen().frame();

    const BLRectI file{static_cast<int>(SPOT.x) + 300, static_cast<int>(rowY(7)) - 4, 40, 8};

    EXPECT_GT(parts::count_near(screen(), file, ttk::Theme::palette().accentSoft, 6), 0);

    ui().release(ttk::Pointer{.x = nameX(), .y = gap});

    EXPECT_EQ(files()[5].group, 2);
}

TEST_F(AddonListTest, DroppedFilesJoinTheGroupUnderThePointer) {
    make(sorted());
    ui().drag_files(nameX(), rowY(4));
    screen().frame();

    EXPECT_GT(parts::count_near(screen(),
                                BLRectI{static_cast<int>(SPOT.x) + 300, static_cast<int>(rowY(5)) - 4, 40, 8},
                                ttk::Theme::palette().accentSoft,
                                6),
              0);

    EXPECT_TRUE(ui().drop_files({"/drops/new.wad"}, nameX(), rowY(4)));

    ASSERT_EQ(files().size(), 6U);
    EXPECT_EQ(files().back().file, "/drops/new.wad");
    EXPECT_EQ(files().back().group, 2);
}

TEST_F(AddonListTest, ACarryNearTheBottomScrollsAGroupedList) {
    make(sorted(12));

    ASSERT_TRUE(_list->scrollable());

    ui().motion(nameX(), rowY(1));
    ui().press(ttk::Pointer{.x = nameX(), .y = rowY(1)});
    ui().motion(nameX(), SPOT.y + SPOT.h - 4.0);
    screen().run(60);

    EXPECT_DOUBLE_EQ(_list->offset(), _list->reach() - SPOT.h);

    ui().release(ttk::Pointer{.x = nameX(), .y = SPOT.y + SPOT.h - 4.0});

    EXPECT_EQ(files()[1].group, 0);
}

TEST_F(AddonListTest, ACarryNearTheTopScrollsBack) {
    make(grounded(12));
    _list->scroll_to(ROW * 3.0);
    screen().frame();

    ui().motion(gripX(), rowY(2));
    ui().press(ttk::Pointer{.x = gripX(), .y = rowY(2)});
    ui().motion(gripX(), SPOT.y + 2.0);
    screen().run(60);

    EXPECT_DOUBLE_EQ(_list->offset(), 0.0);

    const std::string carried = order()[5];

    ui().release(ttk::Pointer{.x = gripX(), .y = SPOT.y + 2.0});
    screen().run(20);

    EXPECT_EQ(order().front(), carried);
}

TEST_F(AddonListTest, ACarryAwayFromTheEdgesDoesNotScroll) {
    make(grounded(12));
    ui().motion(gripX(), rowY(1));
    ui().press(ttk::Pointer{.x = gripX(), .y = rowY(1)});
    ui().motion(gripX(), rowY(4));
    screen().run(30);

    EXPECT_DOUBLE_EQ(_list->offset(), 0.0);
}

TEST_F(AddonListTest, AStillPressOnAGripAtAnEdgeMovesAndScrollsNothing) {
    make(grounded(12));
    _list->scroll_to(ROW * 3.0);
    screen().frame();

    const std::vector<std::string> was = order();

    for (const double y : {SPOT.y + 4.0, SPOT.y + SPOT.h - 4.0}) {
        ui().motion(gripX(), y);
        ui().press(ttk::Pointer{.x = gripX(), .y = y});
        screen().run(6);
        ui().release(ttk::Pointer{.x = gripX(), .y = y});
        screen().run(20);

        EXPECT_DOUBLE_EQ(_list->offset(), ROW * 3.0);
        EXPECT_EQ(order(), was);
    }
}

TEST_F(AddonListTest, TheLoadOrderViewCarriesByTheGripAndKeepsTheGroups) {
    make(sorted());
    rig().config().lists().setAddonView(State::AddonView::LoadOrder);
    _list->sync();

    EXPECT_DOUBLE_EQ(_list->reach(), 5 * ROW);
    EXPECT_EQ(_list->cursor_at(gripX(), rowY(1)), ttk::Cursor::Resize);

    const std::vector<std::string> was = order();

    carry(0, rowY(2));

    EXPECT_EQ(order(), (std::vector<std::string>{was[1], was[2], was[0], was[3], was[4]}));
    EXPECT_EQ(groups(), (std::vector<int>{1, 0, 2, 2, 1}));
}

TEST_F(AddonListTest, TheLoadOrderViewTagsARowWithItsGroup) {
    make(sorted());

    const BLRectI tag{static_cast<int>(SPOT.x + SPOT.w) - 80, static_cast<int>(rowY(1)) - 8, 68, 16};
    const BLRectI none{static_cast<int>(SPOT.x + SPOT.w) - 80, static_cast<int>(rowY(2)) - 8, 68, 16};
    const std::uint32_t ground = ttk::Theme::palette().background.value;

    rig().config().lists().setAddonView(State::AddonView::LoadOrder);
    _list->sync();
    screen().frame();

    EXPECT_GT(parts::ink(screen(), tag, ground).pixels, 0U);
    EXPECT_EQ(parts::ink(screen(), none, ground).pixels, 0U);
}

}
