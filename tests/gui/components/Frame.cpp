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

#include <gtest/gtest.h>

#include "ttk/draw/Theme.h"

#include "Mount.h"
#include "gui/components/Frame.h"
#include "support/Rig.h"

namespace {

constexpr double STRIP = 34.0;
constexpr double PANEL = 300.0;

class FrameTest : public parts::Settled {
protected:
    void dock(const std::string &key = "game:Doom") {
        rig().runs().refused(key, "Doom", "No such file.");
        rig().runs().show(key);
        rig().turn();
    }

    void fold() {
        rig().runs().hide();
        rig().turn();
    }

    void expectBox(const BLRect &box, const double x, const double y, const double w, const double h) {
        EXPECT_DOUBLE_EQ(box.x, x);
        EXPECT_DOUBLE_EQ(box.y, y);
        EXPECT_DOUBLE_EQ(box.w, w);
        EXPECT_DOUBLE_EQ(box.h, h);
    }
};

TEST_F(FrameTest, FillsTheWindow) {
    rig().turn();

    expectBox(rig().frame().box(), 0.0, 0.0, 1280.0, 800.0);
}

TEST_F(FrameTest, TheBarRunsAcrossTheTopAtItsHeight) {
    rig().turn();

    expectBox(rig().bar().box(), 0.0, 0.0, 1280.0, ttk::Theme::barHeight);
}

TEST_F(FrameTest, AWideWindowCentresThePageAtItsWidest) {
    rig().turn();

    expectBox(rig().library().box(), 50.0, 76.0, 1180.0, 800.0 - 76.0 - ttk::Theme::pageMargin);
}

TEST_F(FrameTest, ANarrowWindowGivesThePageAllButItsMargins) {
    canvas().resize(900, 600);
    rig().turn();

    expectBox(rig().library().box(), 22.0, 76.0, 856.0, 600.0 - 76.0 - ttk::Theme::pageMargin);
    expectBox(rig().bar().box(), 0.0, 0.0, 900.0, ttk::Theme::barHeight);
}

TEST_F(FrameTest, WithNothingDockedTheDockTakesNoRoom) {
    rig().turn();

    EXPECT_FALSE(rig().logs().visible());
    EXPECT_DOUBLE_EQ(rig().logs().box().h, 0.0);
}

TEST_F(FrameTest, AFoldedDockSitsAlongTheBottomUnderThePage) {
    dock();
    fold();

    expectBox(rig().logs().box(), 50.0, 800.0 - STRIP - 12.0, 1180.0, STRIP);
    expectBox(rig().library().box(), 50.0, 76.0, 1180.0, 800.0 - 76.0 - STRIP - 20.0);
}

TEST_F(FrameTest, AnOpenDockTakesItsPanelOffThePage) {
    dock();

    const double tall = STRIP + PANEL + 8.0;

    expectBox(rig().logs().box(), 50.0, 800.0 - tall - 12.0, 1180.0, tall);
    expectBox(rig().library().box(), 50.0, 76.0, 1180.0, 800.0 - 76.0 - tall - 20.0);
}

TEST_F(FrameTest, TheDockFollowsThePageOnANarrowWindow) {
    canvas().resize(900, 600);
    dock();
    fold();

    expectBox(rig().logs().box(), 22.0, 600.0 - STRIP - 12.0, 856.0, STRIP);
}

TEST_F(FrameTest, AWindowTooShortForTheDockLeavesThePageNoHeight) {
    canvas().resize(900, 300);
    dock();

    EXPECT_DOUBLE_EQ(rig().library().box().h, 0.0);
}

TEST_F(FrameTest, ClosingTheLastRunGivesThePageItsRoomBack) {
    dock();
    rig().runs().close("game:Doom");
    rig().turn();

    EXPECT_FALSE(rig().logs().visible());
    EXPECT_DOUBLE_EQ(rig().library().box().h, 800.0 - 76.0 - ttk::Theme::pageMargin);
}

TEST_F(FrameTest, OpeningTheDockRepaintsLikeAFullFrame) {
    rig().turn();
    canvas().full();
    dock();
    rig().turn();

    EXPECT_TRUE(canvas().matchesFull());
}

TEST_F(FrameTest, FoldingTheDockRepaintsLikeAFullFrame) {
    dock();
    canvas().full();
    fold();

    EXPECT_TRUE(canvas().matchesFull());
}

}
