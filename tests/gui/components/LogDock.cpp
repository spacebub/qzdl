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
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/draw/Theme.h"
#include "ttk/notices/Notice.h"
#include "ttk/system/Process.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/TextView.h"
#include "ttk/toolkit/layout/Scroll.h"

#include "Mount.h"
#include "gui/components/LogDock.h"
#include "gui/state/State.h"
#include "support/Rig.h"

namespace {

constexpr double STRIP = 34.0;
constexpr double PANEL = 300.0;

// Never handed out by the process table, so stopping it reaches nothing.
constexpr ttk::Process::Id NOBODY = 987654321;

class LogDockTest : public parts::Settled {
protected:
    components::LogDock &dock() { return rig().logs(); }

    // A run that would not start, docked and shown.
    void refused(const std::string &key, const std::string &title) {
        rig().runs().refused(key, title, "No such file.");
        rig().runs().show(key);
        rig().turn();
    }

    // A run still going, which ZDL4 has to ask before it closes.
    void running(const std::string &key, const std::string &title) {
        rig().runs().began(key, title, "doom -iwad doom.wad", NOBODY, ttk::Process::NOTHING);
        rig().runs().show(key);
        rig().turn();
    }

    ttk::TextView &view() { return *parts::find<ttk::TextView>(&dock()); }

    ttk::GlyphButton &copy() { return *parts::hinted<ttk::GlyphButton>(&dock(), "Copy all of it"); }

    ttk::GlyphButton &fold() { return *parts::hinted<ttk::GlyphButton>(&dock(), "Fold it away"); }

    double top() { return dock().box().y + dock().box().h - STRIP; }

    // Each tab is as wide as its label and 74 more, up to 240, with 8 between.
    BLRect tab(const size_t index, const std::vector<std::string> &titles) {
        const BLFont &face = canvas().type().at(400, ttk::Theme::fontSmall);
        double x = dock().box().x;

        for (size_t at = 0;; ++at) {
            const double wide = std::min(240.0, canvas().type().width(face, titles[at]) + 74.0);

            if (at == index) {
                return BLRect{x, top(), wide, STRIP};
            }

            x += wide + 8.0;
        }
    }

    static BLPoint cross(const BLRect &tab) { return BLPoint{tab.x + tab.w - 27.0 + 11.0, tab.y + (STRIP / 2.0)}; }

    void clickTab(const BLRect &tab) { parts::click(rig().ui(), tab.x + 30.0, tab.y + (STRIP / 2.0)); }

    void clickCross(const BLRect &tab) {
        const BLPoint at = cross(tab);

        parts::click(rig().ui(), at.x, at.y);
        rig().turn();
    }

    // The state dot, 11 in from the tab's left edge.
    int dot(const BLRect &tab, const BLRgba32 tone) {
        const BLRectI around{static_cast<int>(tab.x) + 12, static_cast<int>(tab.y + (STRIP / 2.0)) - 2, 5, 5};

        return parts::count_near(canvas(), around, tone, 40);
    }

    void addLines(const int count) {
        for (int at = 0; at < count; ++at) {
            State::get().runs.lines.push_back(State::LogRow{.line = "line " + std::to_string(at)});
        }

        dock().sync();
    }
};

TEST_F(LogDockTest, IsHiddenAndTakesNoRoomWhileNothingIsDocked) {
    EXPECT_FALSE(dock().visible());
    EXPECT_DOUBLE_EQ(dock().wanted(), 0.0);
}

TEST_F(LogDockTest, AShownRunOpensThePanelOverItsTab) {
    refused("game:Doom", "Doom");

    EXPECT_TRUE(dock().visible());
    EXPECT_DOUBLE_EQ(dock().wanted(), STRIP + PANEL + 8.0);
}

TEST_F(LogDockTest, FoldingKeepsTheTabAndDropsThePanel) {
    refused("game:Doom", "Doom");
    rig().runs().hide();
    rig().turn();

    EXPECT_TRUE(dock().visible());
    EXPECT_DOUBLE_EQ(dock().wanted(), STRIP);
}

TEST_F(LogDockTest, TheCopyAndFoldButtonsShowOnlyWithThePanel) {
    refused("game:Doom", "Doom");

    EXPECT_TRUE(copy().visible());
    EXPECT_TRUE(fold().visible());

    rig().runs().hide();
    rig().turn();

    EXPECT_FALSE(copy().visible());
    EXPECT_FALSE(fold().visible());
}

TEST_F(LogDockTest, TheButtonsSitInThePanelsTopRightCorner) {
    refused("game:Doom", "Doom");

    const BLRect &box = dock().box();

    EXPECT_DOUBLE_EQ(fold().box().x, box.x + box.w - 12.0 - 26.0);
    EXPECT_DOUBLE_EQ(fold().box().y, box.y + 12.0);
    EXPECT_DOUBLE_EQ(copy().box().x + copy().box().w + 1.0, fold().box().x);
}

TEST_F(LogDockTest, TheViewHoldsTheShownRunsLines) {
    refused("game:Doom", "Doom");

    EXPECT_FALSE(State::get().runs.lines.empty());
    EXPECT_EQ(view().rows(), State::get().runs.lines.size());
}

TEST_F(LogDockTest, NewLinesAreAddedToTheEnd) {
    refused("game:Doom", "Doom");

    const size_t was = view().rows();

    addLines(3);

    EXPECT_EQ(view().rows(), was + 3);
}

TEST_F(LogDockTest, LinesGoneOffTheFrontGoFromTheView) {
    refused("game:Doom", "Doom");
    addLines(4);

    State::RunsState &runs = State::get().runs;

    runs.lines.erase(runs.lines.begin(), runs.lines.begin() + 2);
    runs.linesDropped += 2;
    dock().sync();

    EXPECT_EQ(view().rows(), runs.lines.size());
}

TEST_F(LogDockTest, LinesDroppedAndAddedAtOnceKeepTheCount) {
    refused("game:Doom", "Doom");
    addLines(4);

    State::RunsState &runs = State::get().runs;

    runs.lines.erase(runs.lines.begin(), runs.lines.begin() + 3);
    runs.linesDropped += 3;
    addLines(5);

    EXPECT_EQ(view().rows(), runs.lines.size());
}

TEST_F(LogDockTest, ANewGenerationReplacesEveryRow) {
    refused("game:Doom", "Doom");
    addLines(6);

    State::RunsState &runs = State::get().runs;

    runs.lines = {State::LogRow{.line = "fresh"}};
    runs.linesDropped = 0;
    ++runs.lineGeneration;
    dock().sync();

    EXPECT_EQ(view().rows(), 1U);
}

TEST_F(LogDockTest, ShowingAnotherRunReplacesEveryRow) {
    refused("game:Doom", "Doom");
    addLines(6);
    refused("game:Heretic", "Heretic");

    EXPECT_EQ(view().rows(), State::get().runs.lines.size());
}

TEST_F(LogDockTest, TheCopyButtonIsOffWithNothingToCopy) {
    refused("game:Doom", "Doom");

    EXPECT_TRUE(copy().enabled());

    State::get().runs.lines.clear();
    ++State::get().runs.lineGeneration;
    dock().sync();

    EXPECT_FALSE(copy().enabled());
}

TEST_F(LogDockTest, CopyingSaysSoInANotice) {
    refused("game:Doom", "Doom");

    const size_t was = rig().notify().messages().size();

    parts::click(rig().ui(), copy());

    ASSERT_EQ(rig().notify().messages().size(), was + 1);
    EXPECT_EQ(rig().notify().messages().back().severity, ttk::Severity::Success);
    EXPECT_EQ(rig().notify().messages().back().body, "The output is on the clipboard.");
}

TEST_F(LogDockTest, TheFoldButtonFoldsTheLog) {
    refused("game:Doom", "Doom");
    parts::click(rig().ui(), fold());
    rig().turn();

    EXPECT_TRUE(State::get().runs.showing.empty());
    EXPECT_DOUBLE_EQ(dock().wanted(), STRIP);
}

TEST_F(LogDockTest, ClickingAFoldedTabShowsItsLog) {
    refused("game:Doom", "Doom");
    rig().runs().hide();
    rig().turn();
    clickTab(tab(0, {"Doom"}));

    EXPECT_EQ(State::get().runs.showing, "game:Doom");
}

TEST_F(LogDockTest, ClickingTheShownTabFoldsIt) {
    refused("game:Doom", "Doom");
    clickTab(tab(0, {"Doom"}));

    EXPECT_TRUE(State::get().runs.showing.empty());
}

TEST_F(LogDockTest, TabsAreLaidAlongTheBottomInTheOrderTheyDocked) {
    refused("game:Doom", "Doom");
    refused("game:Heretic", "Heretic");
    rig().runs().hide();
    rig().turn();

    clickTab(tab(1, {"Doom", "Heretic"}));

    EXPECT_EQ(State::get().runs.showing, "game:Heretic");

    rig().turn();
    clickTab(tab(0, {"Doom", "Heretic"}));

    EXPECT_EQ(State::get().runs.showing, "game:Doom");
}

TEST_F(LogDockTest, ALongTitleMakesATabNoWiderThan240) {
    const std::string title(120, 'W');

    refused("game:Long", title);
    refused("game:Doom", "Doom");
    rig().runs().hide();
    rig().turn();

    parts::click(rig().ui(), dock().box().x + 240.0 + 8.0 + 30.0, top() + (STRIP / 2.0));

    EXPECT_EQ(State::get().runs.showing, "game:Doom");
}

TEST_F(LogDockTest, TheCrossClosesARunThatHasEnded) {
    refused("game:Doom", "Doom");
    clickCross(tab(0, {"Doom"}));

    EXPECT_TRUE(rig().asks.empty());
    EXPECT_TRUE(State::get().runs.docked.empty());
    EXPECT_FALSE(dock().visible());
}

TEST_F(LogDockTest, TheCrossAsksBeforeStoppingARunStillGoing) {
    running("game:Doom", "Doom");
    clickCross(tab(0, {"Doom"}));

    ASSERT_EQ(rig().asks.size(), 1U);
    EXPECT_EQ(rig().asks.front().title, "Stop Doom?");
    EXPECT_EQ(rig().asks.front().accept, "Stop it");
    EXPECT_TRUE(rig().asks.front().danger);
    EXPECT_EQ(rig().runs().stateOf("game:Doom"), State::RunState::Launching);
}

TEST_F(LogDockTest, AcceptingTheAskStopsTheRun) {
    running("game:Doom", "Doom");
    clickCross(tab(0, {"Doom"}));

    ASSERT_EQ(rig().asks.size(), 1U);

    rig().asks.front().accepted();

    EXPECT_EQ(rig().runs().stateOf("game:Doom"), State::RunState::Stopping);
}

TEST_F(LogDockTest, TheCrossOnAStoppingRunForcesWithoutAsking) {
    running("game:Doom", "Doom");
    rig().runs().close("game:Doom");
    rig().turn();
    clickCross(tab(0, {"Doom"}));

    EXPECT_TRUE(rig().asks.empty());
}

TEST_F(LogDockTest, OnlyTheStripTakesAPress) {
    refused("game:Doom", "Doom");

    const BLRect &box = dock().box();

    EXPECT_FALSE(dock().press(ttk::Pointer{.x = box.x + 100.0, .y = box.y + 100.0}));
    EXPECT_TRUE(dock().press(ttk::Pointer{.x = box.x + 100.0, .y = top() + 10.0}));
}

TEST_F(LogDockTest, IsNoTabStop) {
    refused("game:Doom", "Doom");

    EXPECT_FALSE(dock().takes_focus());
}

TEST_F(LogDockTest, ARunThatWouldNotStartHasADangerDot) {
    refused("game:Doom", "Doom");
    canvas().full();

    EXPECT_GT(dot(tab(0, {"Doom"}), ttk::Theme::palette().danger), 0);
}

TEST_F(LogDockTest, ARunLaunchingHasAnAccentDot) {
    running("game:Doom", "Doom");
    canvas().full();

    EXPECT_GT(dot(tab(0, {"Doom"}), ttk::Theme::palette().accent), 0);
    EXPECT_EQ(dot(tab(0, {"Doom"}), ttk::Theme::palette().danger), 0);
}

TEST_F(LogDockTest, ADotThatChangesRepaintsLikeAFullFrame) {
    running("game:Doom", "Doom");
    canvas().full();
    rig().runs().close("game:Doom");
    rig().turn();

    const parts::Ink off = parts::mismatch(canvas());

    EXPECT_EQ(off.pixels, 0U) << off;
}

TEST_F(LogDockTest, HoveringATabAndItsCrossRepaintsLikeAFullFrame) {
    refused("game:Doom", "Doom");
    rig().runs().hide();
    rig().turn();
    canvas().full();

    const BLRect first = tab(0, {"Doom"});
    const BLPoint shut = cross(first);

    rig().ui().motion(first.x + 30.0, first.y + 10.0);
    canvas().frame();
    rig().ui().motion(shut.x, shut.y);
    canvas().frame();

    const parts::Ink off = parts::mismatch(canvas());

    EXPECT_EQ(off.pixels, 0U) << off;
}

TEST_F(LogDockTest, HoveringTheCrossTurnsItToDanger) {
    refused("game:Doom", "Doom");
    canvas().full();

    const BLPoint shut = cross(tab(0, {"Doom"}));
    const BLRectI around{static_cast<int>(shut.x) - 6, static_cast<int>(shut.y) - 6, 12, 12};

    EXPECT_EQ(parts::count_near(canvas(), around, ttk::Theme::palette().danger, 60), 0);

    rig().ui().motion(shut.x, shut.y);
    canvas().frame();

    EXPECT_GT(parts::count_near(canvas(), around, ttk::Theme::palette().danger, 60), 0);
}

TEST_F(LogDockTest, LinesArrivingRepaintLikeAFullFrame) {
    refused("game:Doom", "Doom");
    canvas().full();
    addLines(40);
    canvas().frame();

    const parts::Ink off = parts::mismatch(canvas());

    EXPECT_EQ(off.pixels, 0U) << off;
}

TEST_F(LogDockTest, TheViewFollowsTheEndAsLinesArrive) {
    refused("game:Doom", "Doom");
    addLines(200);
    canvas().frame();

    const ttk::Scroll &scroll = *parts::find<ttk::Scroll>(&dock());

    EXPECT_GT(scroll.offset(), 0.0);
    EXPECT_TRUE(scroll.at_end());

    addLines(20);
    canvas().frame();

    EXPECT_TRUE(scroll.at_end());
}

TEST_F(LogDockTest, AViewScrolledBackStaysPutAsLinesArrive) {
    refused("game:Doom", "Doom");
    addLines(200);
    canvas().frame();

    ttk::Scroll &scroll = *parts::find<ttk::Scroll>(&dock());
    const BLPoint inside = parts::middle(scroll.box());

    rig().ui().wheel(5.0, inside.x, inside.y);
    canvas().run(30);

    const double held = scroll.offset();

    ASSERT_FALSE(scroll.at_end());

    addLines(20);
    canvas().frame();

    EXPECT_DOUBLE_EQ(scroll.offset(), held);
}

TEST_F(LogDockTest, LinesGoneOffTheFrontKeepAScrolledBackViewOnItsRows) {
    refused("game:Doom", "Doom");
    addLines(200);
    canvas().frame();

    ttk::Scroll &scroll = *parts::find<ttk::Scroll>(&dock());
    const BLPoint inside = parts::middle(scroll.box());

    rig().ui().wheel(5.0, inside.x, inside.y);
    canvas().run(30);

    const double held = scroll.offset();
    State::RunsState &runs = State::get().runs;

    runs.lines.erase(runs.lines.begin(), runs.lines.begin() + 10);
    runs.linesDropped += 10;
    dock().sync();

    EXPECT_DOUBLE_EQ(scroll.offset(), held - (10.0 * view().row_height(canvas().type())));
}

}
