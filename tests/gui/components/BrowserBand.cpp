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
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/draw/Theme.h"
#include "ttk/toolkit/Event.h"
#include "ttk/toolkit/Root.h"
#include "ttk/toolkit/controls/Field.h"

#include "Mount.h"
#include "core/config/Session.h"
#include "gui/components/AddonsPanel.h"
#include "gui/components/BrowserBand.h"
#include "gui/draw/Band.h"
#include "gui/state/State.h"
#include "support/Rig.h"
#include "support/Sandbox.h"

namespace {

constexpr BLRect SPOT{20.0, 20.0, 560.0, 480.0};
constexpr double ROW = components::BrowserRows::ROW;

class BrowserBandTest : public parts::Screened {
protected:
    BrowserBandTest() {
        wide = 800;
        tall = 560;
    }

    // Two add-ons from elsewhere and a.wad from a folder of five files in a tree, open unless `open` is false. With
    // both folders folded the lines are sub, two, a.wad, b.wad and c.wad. `more` adds files to the tree.
    void make(const bool open = true, const std::vector<std::string> &more = {}) {
        _folder = tests::Sandbox::scratch("band");

        std::vector<std::string> names = {"a.wad", "b.wad", "c.wad", "sub/d.pk3", "two/e.wad"};

        names.insert(names.end(), more.begin(), more.end());

        for (const std::string &name : names) {
            std::filesystem::create_directories((_folder / name).parent_path());
            std::ofstream(_folder / name) << "x";
        }

        Config made = tests::Fixtures::grounded(3, 2);

        made.general.showPaths = false;
        made.general.fileBrowser = true;
        made.general.fileBrowserOpen = open;
        made.general.fileBrowserFolder = _folder.generic_string();
        made.profiles.front().files.push_back(
                FileEntry{.file = path("a.wad"), .enabled = true, .gone = false, .group = 0});

        parts::install(rig(), std::move(made));

        _panel = parts::place(screen(), std::make_unique<components::AddonsPanel>(&rig().reach()), SPOT);

        sync();
    }

    void sync() {
        rig().shell().advance(0.0);
        _panel->sync();
        screen().ui().relayout();
        screen().frame();
    }

    [[nodiscard]] std::string path(const std::string &name) const { return (_folder / name).generic_string(); }

    components::BrowserBand &band() { return *parts::find<components::BrowserBand>(_panel); }
    components::BrowserRows &rows() { return *parts::find<components::BrowserRows>(_panel); }
    components::AddonList &list() { return *parts::find<components::AddonList>(_panel); }
    ttk::Field &filter() { return *parts::find<ttk::Field>(&band()); }

    void clickRow(const int row, const bool ctrl = false, const bool shift = false) {
        const double x = rows().box().x + 100.0;
        const double y = rows().box().y + (row * ROW) + (ROW / 2.0);

        ui().motion(x, y);
        ui().press(ttk::Pointer{.x = x, .y = y, .button = ttk::Click::Left, .ctrl = ctrl, .shift = shift});
        ui().release(ttk::Pointer{.x = x, .y = y});
        sync();
    }

    void clickMenu(const int row) {
        const BLRect dots = Band::menuBox(band().box());

        parts::click(ui(), dots.x + 16.0, dots.y + 15.0);

        ASSERT_EQ(ui().layer(ttk::Root::POPUPS)->children().size(), 1U);

        const BLRect &menu = ui().layer(ttk::Root::POPUPS)->children().front()->box();

        // Rows of the menu are 32 tall under a pad of 5.
        parts::click(ui(), menu.x + 60.0, menu.y + 5.0 + (row * 32.0) + 16.0);
        sync();
    }

    // The files of the profile from the folder, by name.
    [[nodiscard]] std::vector<std::string> picks() const {
        std::vector<std::string> out;

        for (const FileEntry &entry : Session::get().config().activeProfile().files) {
            if (entry.file.starts_with(_folder.generic_string())) {
                out.push_back(std::filesystem::path(entry.file).filename().string());
            }
        }

        std::ranges::sort(out);

        return out;
    }

    std::filesystem::path _folder;
    components::AddonsPanel *_panel = nullptr;
};

TEST_F(BrowserBandTest, AClickTogglesTheFile) {
    make();
    clickRow(3);

    EXPECT_EQ(picks(), (std::vector<std::string>{"a.wad", "b.wad"}));

    clickRow(2);

    EXPECT_EQ(picks(), std::vector<std::string>{"b.wad"});
}

TEST_F(BrowserBandTest, AShiftRunSkipsTheFolderRowsAndASecondShiftClickMovesIt) {
    make();
    clickRow(1);
    screen().run(20);
    clickRow(0);
    screen().run(20);
    sync();

    // The lines are sub, d.pk3, two, e.wad, a.wad, b.wad and c.wad.
    ASSERT_EQ(State::get().browser.shown.size(), 7U);

    clickRow(3);
    clickRow(1, false, true);

    EXPECT_EQ(picks(), (std::vector<std::string>{"a.wad", "d.pk3", "e.wad"}));

    clickRow(5, false, true);

    EXPECT_EQ(picks(), (std::vector<std::string>{"a.wad", "b.wad", "e.wad"}));
}

TEST_F(BrowserBandTest, ShiftClickWithNoAnchorTogglesTheRow) {
    make();
    clickRow(4, false, true);

    EXPECT_EQ(picks(), (std::vector<std::string>{"a.wad", "c.wad"}));
}

TEST_F(BrowserBandTest, AFoldSlidesTheRowsAwayAndKeepsThePicksInside) {
    make();
    clickRow(0);
    screen().run(20);
    sync();
    clickRow(1);

    EXPECT_DOUBLE_EQ(rows().reach(), 6 * ROW);

    clickRow(0);
    screen().run(6);

    // Mid slide the folder is still open and the lines below have moved up by part of a line.
    EXPECT_FALSE(State::get().browser.rows[0].folded);
    EXPECT_GT(rows().reach(), 5 * ROW);
    EXPECT_LT(rows().reach(), 6 * ROW);

    screen().run(20);
    sync();

    EXPECT_TRUE(State::get().browser.rows[0].folded);
    EXPECT_DOUBLE_EQ(rows().reach(), 5 * ROW);

    clickRow(3);

    EXPECT_EQ(picks(), (std::vector<std::string>{"a.wad", "b.wad", "d.pk3"}));
}

// A folder sliding shut lifts its rows above its own line, and the lines above it must still paint.
TEST_F(BrowserBandTest, TheLinesAboveAFolderStayWhileItSlides) {
    make(true, {"two/f.wad", "two/g.wad"});
    clickRow(1);
    screen().run(20);
    sync();

    ASSERT_EQ(State::get().browser.shown.size(), 8U);

    const BLRectI name{static_cast<int>(rows().box().x) + 70,
                       static_cast<int>(rows().box().y) + 6,
                       60,
                       static_cast<int>(ROW) - 12};
    const std::uint32_t ground = screen().at(name.x, name.y);

    for (int fold = 0; fold < 2; ++fold) {
        clickRow(1);

        for (int frame = 0; frame < 16; ++frame) {
            screen().frameAt(screen().tick());

            EXPECT_GT(parts::ink(screen(), name, ground).pixels, 0U) << "fold " << fold << " frame " << frame;
        }

        sync();
    }
}

// A folder listed first moves sub to the row number that two slides shut under.
TEST_F(BrowserBandTest, ANewListingDropsTheFolderSlides) {
    make();
    clickRow(0);
    screen().run(20);
    sync();
    clickRow(2);
    screen().run(20);
    sync();
    clickRow(2);

    std::filesystem::create_directories(_folder / "aaa");
    std::ofstream(_folder / "aaa" / "z.wad") << "x";
    rig().config().browser().setOpen(true);
    sync();
    screen().run(20);
    sync();

    ASSERT_EQ(State::get().browser.rows[2].name, "sub");
    EXPECT_FALSE(State::get().browser.rows[2].folded);
}

TEST_F(BrowserBandTest, AFoldedFolderThatHoldsPicksSaysHowMany) {
    make();
    clickRow(0);
    screen().run(20);
    sync();
    clickRow(1);
    clickRow(0);
    screen().run(20);
    sync();

    const auto end = [this](const int line) {
        return BLRectI{static_cast<int>(rows().box().x + rows().box().w) - 90,
                       static_cast<int>(rows().box().y + (line * ROW)) + 6,
                       80,
                       static_cast<int>(ROW) - 12};
    };
    const std::uint32_t ground = screen().at(end(1).x, end(1).y);

    ASSERT_EQ(State::get().browser.rows[0].picks, 1);
    EXPECT_GT(parts::ink(screen(), end(0), ground).pixels, 0U);
    EXPECT_EQ(parts::ink(screen(), end(1), ground).pixels, 0U);
}

TEST_F(BrowserBandTest, TheFilterOpensTheFoldersAboveAMatchAndKeepsTheHiddenPicks) {
    make();
    type(filter(), "e.w");
    sync();

    EXPECT_EQ(State::get().browser.shown, (std::vector<int>{2, 3}));

    clickRow(0);
    clickRow(1);

    EXPECT_EQ(picks(), (std::vector<std::string>{"a.wad", "e.wad"}));
    EXPECT_TRUE(State::get().browser.rows[2].folded);
}

TEST_F(BrowserBandTest, CtrlAPicksEveryFileShown) {
    make();
    type(filter(), ".wad");
    sync();
    clickRow(1);
    ui().key(parts::key('a', true));
    sync();

    EXPECT_EQ(picks(), (std::vector<std::string>{"a.wad", "b.wad", "c.wad", "e.wad"}));
}

// Synced and painted without a layout: the pointer stays on the row, so only the sync can repaint it.
TEST_F(BrowserBandTest, AClickedRowRepaintsWithoutALayout) {
    make();

    const double x = rows().box().x + 100.0;
    const double y = rows().box().y + (3 * ROW) + (ROW / 2.0);

    ui().motion(x, y);
    screen().frame();
    parts::click(ui(), x, y);
    rig().shell().advance(0.0);
    _panel->sync();
    screen().frame();

    const parts::Ink off = parts::mismatch(screen());

    EXPECT_EQ(off.pixels, 0U) << off;
}

// Painted without a layout: only the row the pointer leaves and the one it enters repaint.
TEST_F(BrowserBandTest, TheHoverRepaintsTheRowsItMovesBetween) {
    make();

    const double x = rows().box().x + 100.0;

    ui().motion(x, rows().box().y + (3 * ROW) + (ROW / 2.0));
    screen().frame();
    ui().motion(x, rows().box().y + (4 * ROW) + (ROW / 2.0));
    screen().frame();

    const parts::Ink off = parts::mismatch(screen());

    EXPECT_EQ(off.pixels, 0U) << off;
}

// Mid slide the add-on list and the band move without a layout of the window.
TEST_F(BrowserBandTest, TheBandSlidesWithoutALayout) {
    make();
    parts::click(ui(), band().box().x + 60.0, band().box().y + 15.0);
    sync();
    screen().run(6);

    ASSERT_GT(band().shown(), 0.0F);
    ASSERT_LT(band().shown(), 1.0F);

    const parts::Ink off = parts::mismatch(screen());

    EXPECT_EQ(off.pixels, 0U) << off;
}

TEST_F(BrowserBandTest, ClickingTheHeaderFoldsAndStoresIt) {
    make();
    parts::click(ui(), band().box().x + 60.0, band().box().y + 15.0);
    sync();

    EXPECT_FALSE(Session::get().config().general.fileBrowserOpen);
    EXPECT_FALSE(filter().visible());

    parts::click(ui(), band().box().x + 60.0, band().box().y + 15.0);
    sync();

    EXPECT_TRUE(Session::get().config().general.fileBrowserOpen);
}

TEST_F(BrowserBandTest, TheMenuSetsAndClearsTheOwnFolder) {
    make();
    clickMenu(0);

    ASSERT_TRUE(rig().files().state().open);
    EXPECT_TRUE(rig().files().state().directories);

    rig().files().choose({path("sub")});
    sync();

    EXPECT_EQ(Session::get().config().activeProfile().fileBrowserFolder, path("sub"));
    ASSERT_EQ(State::get().browser.rows.size(), 1U);

    clickMenu(1);

    EXPECT_TRUE(Session::get().config().activeProfile().fileBrowserFolder.empty());
    EXPECT_EQ(State::get().browser.rows.size(), 7U);
}

TEST_F(BrowserBandTest, OpenTheBandTakesTwoThirdsWhateverTheListHolds) {
    make();

    const BLRect before = band().box();

    EXPECT_DOUBLE_EQ(list().box().h, std::floor((SPOT.h - 32.0 - 12.0 - 32.0 - 12.0) / 3.0));
    EXPECT_DOUBLE_EQ(before.y, list().box().y + list().box().h + 4.0);
    EXPECT_DOUBLE_EQ(before.y + before.h, SPOT.y + SPOT.h - 16.0 - 6.0);

    for (int at = 0; at < 12; ++at) {
        rig().config().lists().addFiles({tests::Fixtures::rooted("/more/" + std::to_string(at) + ".wad")});
    }

    sync();

    EXPECT_DOUBLE_EQ(band().box().y, before.y);
    EXPECT_DOUBLE_EQ(band().box().h, before.h);
}

TEST_F(BrowserBandTest, FoldedTheBandIsItsHeaderAtTheBottom) {
    make(false);

    const BLRect &held = list().box();
    const BLRect &shown = band().box();

    EXPECT_DOUBLE_EQ(shown.h, Band::LINE);
    EXPECT_DOUBLE_EQ(shown.y + shown.h, SPOT.y + SPOT.h - 16.0 - 6.0);
    EXPECT_DOUBLE_EQ(held.y + held.h + 4.0, shown.y);
}

}
