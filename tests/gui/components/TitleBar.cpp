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

#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/draw/Theme.h"
#include "ttk/toolkit/Event.h"

#include "Mount.h"
#include "gui/components/TitleBar.h"
#include "gui/state/State.h"
#include "support/Rig.h"

namespace {

using parts::key;
namespace Code = ttk::Code;

constexpr double MIDDLE = ttk::Theme::barHeight / 2.0;

struct Span {
    double from = 0.0;
    double to = 0.0;

    [[nodiscard]] double centre() const { return (from + to) / 2.0; }
};

// The tabs are the runs the window manager may not take, left of the buttons.
std::vector<Span> spansOf(const components::TitleBar &bar) {
    std::vector<Span> found;
    bool inside = false;

    for (double x = 0.0; x < bar.children().front()->box().x; x += 1.0) {
        const bool held = !bar.draggable(x, MIDDLE);

        if (held && !inside) {
            found.push_back(Span{.from = x, .to = x});
        }

        if (held) {
            found.back().to = x + 1.0;
        }

        inside = held;
    }

    return found;
}

// The underline under a tab, two pixels along the bottom of the bar.
int underlineOf(const tests::Canvas &canvas, const Span &tab) {
    const BLRectI strip{static_cast<int>(tab.from),
                        static_cast<int>(ttk::Theme::barHeight) - 3,
                        static_cast<int>(tab.to - tab.from),
                        2};

    return parts::count_near(canvas, strip, ttk::Theme::palette().accent, 40);
}

class TitleBarTest : public parts::Settled {
protected:
    void SetUp() override {
        parts::Settled::SetUp();

        if (IsSkipped()) {
            return;
        }

        canvas().run(30);
        canvas().full();
    }

    components::TitleBar &bar() { return rig().bar(); }

    ttk::Widget &button(const size_t index) { return *bar().children()[index]; }

    ttk::Widget &shade() { return button(0); }
    ttk::Widget &minimize() { return button(1); }
    ttk::Widget &maximize() { return button(2); }
    ttk::Widget &close() { return button(3); }

    std::vector<Span> tabs() { return spansOf(bar()); }

    void clickTab(const size_t index) { parts::click(rig().ui(), tabs()[index].centre(), MIDDLE); }

    int underline(const Span &tab) { return underlineOf(canvas(), tab); }

    // The badge sits 11 in from the tab's right edge and 16 down.
    int badge(const Span &tab) {
        const BLRectI dot{static_cast<int>(tab.to) - 14, 13, 6, 6};

        return parts::count_near(canvas(), dot, ttk::Theme::palette().accent, 60);
    }

    std::uint32_t label(const Span &tab) {
        const BLRectI box{static_cast<int>(tab.from) + 4, 12, static_cast<int>(tab.to - tab.from) - 8, 28};

        return parts::peak(canvas(), box, ttk::Theme::palette().background.value);
    }
};

TEST_F(TitleBarTest, HasFourTabsLeftOfTheButtons) {
    const std::vector<Span> found = tabs();

    ASSERT_EQ(found.size(), 4U);
    EXPECT_LT(found.back().to, shade().box().x);
}

TEST_F(TitleBarTest, TabsAreLaidLeftToRightTwoApart) {
    const std::vector<Span> found = tabs();

    ASSERT_EQ(found.size(), 4U);

    for (size_t at = 1; at < found.size(); ++at) {
        EXPECT_NEAR(found[at].from - found[at - 1].to, 2.0, 1.0) << "between " << at - 1 << " and " << at;
    }
}

TEST_F(TitleBarTest, AWideWindowCentresTheTabs) {
    const std::vector<Span> found = tabs();

    ASSERT_EQ(found.size(), 4U);
    EXPECT_NEAR((found.front().from + found.back().to) / 2.0, 640.0, 1.5);
}

TEST_F(TitleBarTest, ANarrowWindowKeepsTheTabsClearOfTheButtons) {
    canvas().resize(600, 400);
    rig().turn();

    const std::vector<Span> found = tabs();

    ASSERT_EQ(found.size(), 4U);
    EXPECT_NEAR(found.back().to, shade().box().x - 16.0, 1.5);
}

TEST_F(TitleBarTest, TheNarrowestWindowStartsTheTabsBesideTheMark) {
    canvas().resize(400, 300);
    rig().turn();

    double first = -1.0;

    for (double x = 0.0; x < 400.0 && first < 0.0; x += 1.0) {
        if (!bar().draggable(x, MIDDLE)) {
            first = x;
        }
    }

    EXPECT_DOUBLE_EQ(first, 16.0 + 22.0 + 22.0);
}

TEST_F(TitleBarTest, TheButtonsSitAtTheFarEdgeInARow) {
    EXPECT_DOUBLE_EQ(close().box().x + close().box().w, 1280.0 - 8.0);
    EXPECT_DOUBLE_EQ(maximize().box().x + maximize().box().w + 2.0, close().box().x);
    EXPECT_DOUBLE_EQ(minimize().box().x + minimize().box().w + 2.0, maximize().box().x);
    EXPECT_DOUBLE_EQ(shade().box().x + shade().box().w + 8.0, minimize().box().x);
    EXPECT_DOUBLE_EQ(close().box().y, (ttk::Theme::barHeight - ttk::Theme::controlSmall) / 2.0);
}

TEST_F(TitleBarTest, TheNameIsWrittenOnlyOnAWideWindow) {
    const BLRectI name{48, 12, 60, 30};
    const std::uint32_t ground = ttk::Theme::palette().background.value;

    EXPECT_GT(parts::ink(canvas(), name, ground).pixels, 0U);

    canvas().resize(800, 600);
    rig().turn();
    canvas().full();

    EXPECT_EQ(parts::ink(canvas(), name, ground).pixels, 0U);
}

TEST_F(TitleBarTest, ClickingATabGoesToItsPage) {
    clickTab(2);

    EXPECT_EQ(rig().visited, (std::vector<State::Page>{State::Page::Engines}));
}

TEST_F(TitleBarTest, EachTabGoesToItsOwnPage) {
    for (size_t at = 0; at < 4; ++at) {
        clickTab(at);
    }

    EXPECT_EQ(rig().visited,
              (std::vector<State::Page>{
                      State::Page::Library, State::Page::Profile, State::Page::Engines, State::Page::Settings}));
}

TEST_F(TitleBarTest, BetweenTheTabsAndTheButtonsIsTheWindowManagers) {
    const double x = (tabs().back().to + shade().box().x) / 2.0;

    EXPECT_TRUE(bar().draggable(x, MIDDLE));
    EXPECT_FALSE(bar().press(ttk::Pointer{.x = x, .y = MIDDLE}));

    parts::click(rig().ui(), x, MIDDLE);

    EXPECT_TRUE(rig().visited.empty());
}

TEST_F(TitleBarTest, TheMarkIsTheWindowManagers) {
    EXPECT_TRUE(bar().draggable(20.0, MIDDLE));
}

TEST_F(TitleBarTest, NothingBelowTheBarIsDraggable) {
    EXPECT_FALSE(bar().draggable(20.0, ttk::Theme::barHeight + 1.0));
}

TEST_F(TitleBarTest, TheButtonsAreNotDraggable) {
    for (size_t at = 0; at < 4; ++at) {
        const BLPoint middle = parts::middle(button(at).box());

        EXPECT_FALSE(bar().draggable(middle.x, middle.y)) << "button " << at;
    }
}

TEST_F(TitleBarTest, ArrowsWalkThePages) {
    rig().ui().focus(&bar());

    EXPECT_TRUE(rig().ui().key(key(Code::Right)));
    EXPECT_TRUE(rig().ui().key(key(Code::Right)));
    EXPECT_TRUE(rig().ui().key(key(Code::Left)));

    EXPECT_EQ(rig().visited,
              (std::vector<State::Page>{State::Page::Profile, State::Page::Engines, State::Page::Profile}));
}

TEST_F(TitleBarTest, LeftOnTheFirstPageGoesNowhere) {
    rig().ui().focus(&bar());

    EXPECT_TRUE(rig().ui().key(key(Code::Left)));
    EXPECT_TRUE(rig().visited.empty());
}

TEST_F(TitleBarTest, RightOnTheLastPageGoesNowhere) {
    State::get().sys.page = State::Page::Settings;
    rig().ui().focus(&bar());

    EXPECT_TRUE(rig().ui().key(key(Code::Right)));
    EXPECT_TRUE(rig().visited.empty());
}

// The root would click the middle of a focused control, which is wherever a tab happens to be.
TEST_F(TitleBarTest, ReturnAndSpaceAreTakenAndClickNothing) {
    rig().ui().focus(&bar());

    EXPECT_TRUE(rig().ui().key(key(Code::Return)));
    EXPECT_TRUE(rig().ui().key(key(Code::Space)));
    EXPECT_TRUE(rig().visited.empty());
}

TEST_F(TitleBarTest, OtherKeysAreLeftToTheWindow) {
    EXPECT_FALSE(bar().key(key('a')));
    EXPECT_FALSE(bar().key(key(Code::Escape)));
}

TEST_F(TitleBarTest, MinimizeReachesTheShell) {
    parts::click(rig().ui(), minimize());

    EXPECT_EQ(rig().shell().minimized, 1);
}

TEST_F(TitleBarTest, MaximizeTogglesTheWindowEachClick) {
    parts::click(rig().ui(), maximize());

    EXPECT_TRUE(rig().shell().maximized());

    parts::click(rig().ui(), maximize());

    EXPECT_FALSE(rig().shell().maximized());
    EXPECT_EQ(rig().shell().toggled, 2);
}

TEST_F(TitleBarTest, CloseStopsTheShell) {
    parts::click(rig().ui(), close());

    EXPECT_TRUE(rig().shell().stopped);
}

TEST_F(TitleBarTest, TheShadeButtonCyclesTheShade) {
    parts::click(rig().ui(), shade());

    EXPECT_EQ(rig().shadeCycles, 1);
}

TEST_F(TitleBarTest, TheShadeButtonSaysWhichShadeItIsOn) {
    ttk::Theme::set_mode(ttk::Theme::Mode::Light);
    bar().sync();

    EXPECT_EQ(shade().hint, "Light theme. Click for the dark one");

    ttk::Theme::set_mode(ttk::Theme::Mode::Dark);
    bar().sync();

    EXPECT_EQ(shade().hint, "Dark theme. Click to follow the desktop again");

    ttk::Theme::set_mode(ttk::Theme::Mode::System);
    bar().sync();

    EXPECT_EQ(shade().hint, "Following the desktop. Click for the light theme");
}

TEST_F(TitleBarTest, TheActiveTabIsUnderlined) {
    const std::vector<Span> found = tabs();

    EXPECT_GT(underline(found[0]), 0);
    EXPECT_EQ(underline(found[1]), 0);
}

// A fresh tree, so this is the bar's very first frame.
TEST_F(TitleBarTest, TheOpeningPageIsUnderlinedOnTheFirstFrame) {
    tests::Canvas fresh(1280, 200);
    auto holder = std::make_unique<parts::Holder>();

    holder->spot = BLRect{0.0, 0.0, 1280.0, ttk::Theme::barHeight};

    components::TitleBar *opened = holder->append(std::make_unique<components::TitleBar>(&rig().reach()));

    fresh.ui().content()->add(std::move(holder));

    opened->sync();
    fresh.full();

    EXPECT_GT(underlineOf(fresh, spansOf(*opened)[0]), 0);
}

TEST_F(TitleBarTest, AQuickSecondPageChangeLeavesOnlyTheLastTabUnderlined) {
    clickTab(1);
    rig().turn();
    canvas().run(1);
    clickTab(2);
    rig().turn();
    canvas().run(30);

    const std::vector<Span> found = tabs();

    EXPECT_EQ(underline(found[1]), 0);
    EXPECT_GT(underline(found[2]), 0);
}

TEST_F(TitleBarTest, TheUnderlineMovesToThePageGoneTo) {
    clickTab(1);
    rig().turn();
    canvas().run(30);

    const std::vector<Span> found = tabs();

    EXPECT_EQ(underline(found[0]), 0);
    EXPECT_GT(underline(found[1]), 0);
}

TEST_F(TitleBarTest, TheUnderlineTweenRepaintsLikeAFullFrame) {
    clickTab(3);
    rig().turn();
    canvas().run(4);

    const parts::Ink off = parts::mismatch(canvas(), BLRectI{0, 0, 1280, static_cast<int>(ttk::Theme::barHeight)});

    EXPECT_EQ(off.pixels, 0U) << off;
}

TEST_F(TitleBarTest, TheEnginesTabCarriesADotWhileThereAreNoPorts) {
    EXPECT_GT(badge(tabs()[2]), 0);
    EXPECT_EQ(badge(tabs()[3]), 0);
}

TEST_F(TitleBarTest, TheDotGoesOnceThereIsAPort) {
    parts::install(rig(), tests::Fixtures::config(1, 0));
    rig().turn();

    EXPECT_EQ(badge(tabs()[2]), 0);
    EXPECT_TRUE(canvas().matchesFull());
}

TEST_F(TitleBarTest, TheDotIsNotShownOnTheEnginesPageItself) {
    clickTab(2);
    rig().turn();
    canvas().run(30);

    EXPECT_EQ(badge(tabs()[2]), 0);
}

TEST_F(TitleBarTest, AnInactiveTabIsFaintUntilHovered) {
    const Span tab = tabs()[1];
    const ttk::Theme::Palette &palette = ttk::Theme::palette();

    EXPECT_TRUE(parts::leans(label(tab), palette.faint, palette.text));

    rig().ui().motion(tab.centre(), MIDDLE);
    canvas().frame();

    EXPECT_TRUE(parts::leans(label(tab), palette.text, palette.faint));
}

TEST_F(TitleBarTest, LeavingTheBarDimsTheTabAgain) {
    const Span tab = tabs()[1];
    const ttk::Theme::Palette &palette = ttk::Theme::palette();

    rig().ui().motion(tab.centre(), MIDDLE);
    canvas().frame();
    rig().ui().motion(640.0, 400.0);
    canvas().frame();

    EXPECT_TRUE(parts::leans(label(tab), palette.faint, palette.text));
    EXPECT_TRUE(canvas().matchesFull());
}

TEST_F(TitleBarTest, HoveringAcrossTheTabsRepaintsLikeAFullFrame) {
    for (const Span &tab : tabs()) {
        rig().ui().motion(tab.centre(), MIDDLE);
        canvas().frame();
    }

    EXPECT_TRUE(canvas().matchesFull());
}

TEST_F(TitleBarTest, ASyncWithNothingChangedPaintsNothing) {
    bar().sync();

    EXPECT_EQ(canvas().frame(), 0U);
}

TEST_F(TitleBarTest, TheBarIsTheFirstTabStop) {
    rig().ui().key(key(Code::Tab));

    EXPECT_EQ(rig().ui().focused(), &bar());
}

TEST_F(TitleBarTest, RestingOnTheShadeButtonOffersItsHint) {
    const BLPoint at = parts::middle(shade().box());

    rig().ui().motion(at.x, at.y);

    EXPECT_EQ(rig().ui().hovered(), &shade());
    EXPECT_FALSE(rig().ui().hovered()->hint.empty());
}

TEST_F(TitleBarTest, RestingOnATabOffersNoHint) {
    rig().ui().motion(tabs()[1].centre(), MIDDLE);

    ASSERT_NE(rig().ui().hovered(), nullptr);
    EXPECT_TRUE(rig().ui().hovered()->hint.empty());
}

// The shell reports a maximize as a change of shade, which the window answers with a touch.
TEST_F(TitleBarTest, AMaximizeTheShellReportsTurnsTheButtonIntoRestore) {
    const BLRectI glyph = parts::pixels_of(maximize().box());
    const std::vector<std::uint32_t> before = parts::pixels_in(canvas(), glyph);

    rig().shell().toggle_maximize();
    State::get().touch();
    rig().turn();
    canvas().run(30);

    EXPECT_GT(parts::changed(parts::pixels_in(canvas(), glyph), before), 0);
}

}
