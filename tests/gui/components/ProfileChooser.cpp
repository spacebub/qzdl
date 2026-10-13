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
#include "ttk/toolkit/Root.h"
#include "ttk/toolkit/controls/StatusIndicator.h"
#include "ttk/toolkit/layout/Scroll.h"

#include "Mount.h"
#include "core/config/Session.h"
#include "gui/components/ProfileChooser.h"
#include "gui/draw/Cards.h"
#include "gui/state/State.h"
#include "support/Rig.h"

namespace {

constexpr BLRect SPOT{20.0, 20.0, 700.0, 74.0};
constexpr double ROW = 52.0;
constexpr double ADDER = 38.0;

using Status = ttk::StatusIndicator::Status;

class ProfileChooserTest : public parts::Screened {
protected:
    ProfileChooserTest() {
        wide = 800;
        tall = 700;
    }

    components::ProfileChooser *make(Config what = tests::Fixtures::config(3, 2), const BLRect &spot = SPOT) {
        parts::install(rig(), std::move(what));

        _chooser = parts::place(screen(), std::make_unique<components::ProfileChooser>(&rig().reach()), spot);

        _chooser->setSaid("Port 0 · Game 0", true);
        screen().full();

        return _chooser;
    }

    ttk::Widget *popups() { return ui().layer(ttk::Root::POPUPS); }

    ttk::Widget *list() { return popups()->children().empty() ? nullptr : popups()->children().front().get(); }

    ttk::Scroll &scroll() { return *parts::find<ttk::Scroll>(list()); }

    void open() {
        parts::click(ui(), *_chooser);
        screen().frame();
    }

    // The middle of the row `index` places down from the top of what is scrolled into view.
    BLPoint row(const int index) {
        const BLRect &view = scroll().box();

        return BLPoint{view.x + 120.0, view.y + (index * ROW) + (ROW / 2.0)};
    }

    void clickRow(const int index) {
        const BLPoint at = row(index);

        parts::click(ui(), at.x, at.y);
    }

    static Config &config() { return Session::get().config(); }

    components::ProfileChooser *_chooser = nullptr;
};

TEST_F(ProfileChooserTest, ClickingDropsTheProfileList) {
    make();
    open();

    EXPECT_NE(list(), nullptr);
}

TEST_F(ProfileChooserTest, TheListHangsUnderTheChooser) {
    make();
    open();

    ASSERT_NE(list(), nullptr);

    const BLRect &box = list()->box();

    EXPECT_DOUBLE_EQ(box.x, SPOT.x);
    EXPECT_DOUBLE_EQ(box.y, SPOT.y + SPOT.h + 6.0);
}

TEST_F(ProfileChooserTest, TheListIsARowPerProfileAndTheAdder) {
    make(tests::Fixtures::config(3, 2));
    open();

    ASSERT_NE(list(), nullptr);
    EXPECT_DOUBLE_EQ(list()->box().h, (3 * ROW) + 10.0 + ADDER);
}

TEST_F(ProfileChooserTest, TheListStopsGrowingAtEightRows) {
    make(tests::Fixtures::config(12, 1));
    open();

    ASSERT_NE(list(), nullptr);
    EXPECT_DOUBLE_EQ(list()->box().h, (8 * ROW) + 10.0 + ADDER);
}

TEST_F(ProfileChooserTest, AWideChooserDropsAListNoWiderThan460) {
    make();
    open();

    ASSERT_NE(list(), nullptr);
    EXPECT_DOUBLE_EQ(list()->box().w, 460.0);
}

TEST_F(ProfileChooserTest, ANarrowChooserDropsAListNoNarrowerThan280) {
    make(tests::Fixtures::config(3, 2), BLRect{20.0, 20.0, 200.0, 74.0});
    open();

    ASSERT_NE(list(), nullptr);
    EXPECT_DOUBLE_EQ(list()->box().w, 280.0);
}

TEST_F(ProfileChooserTest, ClickingARowOpensThatProfile) {
    make();
    open();
    clickRow(2);

    EXPECT_EQ(config().activeProfileId, "profile-2");
    EXPECT_EQ(State::get().cfg.profileIndex, 2);
}

TEST_F(ProfileChooserTest, ClickingARowClosesTheList) {
    make();
    open();
    clickRow(1);

    EXPECT_EQ(list(), nullptr);
}

TEST_F(ProfileChooserTest, ClickingTheCurrentRowOnlyCloses) {
    make();
    open();
    clickRow(0);

    EXPECT_EQ(config().activeProfileId, "profile-0");
    EXPECT_EQ(list(), nullptr);
}

TEST_F(ProfileChooserTest, TheAdderAsksForAName) {
    make();
    open();

    ASSERT_NE(list(), nullptr);

    const BLRect &box = list()->box();

    parts::click(ui(), box.x + 60.0, box.y + box.h - (ADDER / 2.0));

    ASSERT_EQ(rig().prompts.size(), 1U);
    EXPECT_EQ(rig().prompts.front().title, "New profile");
    EXPECT_EQ(rig().prompts.front().value, "New profile");
    EXPECT_EQ(list(), nullptr);
}

TEST_F(ProfileChooserTest, AcceptingTheNameAddsTheProfile) {
    make();
    open();

    ASSERT_NE(list(), nullptr);

    const BLRect &box = list()->box();

    parts::click(ui(), box.x + 60.0, box.y + box.h - (ADDER / 2.0));

    ASSERT_EQ(rig().prompts.size(), 1U);

    rig().prompts.front().accepted("Sunlust");

    EXPECT_EQ(config().profiles.size(), 4U);
    EXPECT_EQ(config().activeProfile().name, "Sunlust");
}

TEST_F(ProfileChooserTest, ClickingTheChooserAgainClosesTheList) {
    make();
    open();
    open();

    EXPECT_EQ(list(), nullptr);
}

TEST_F(ProfileChooserTest, ClickingAwayClosesTheList) {
    make();
    open();
    parts::click(ui(), 760.0, 680.0);

    EXPECT_EQ(list(), nullptr);
}

TEST_F(ProfileChooserTest, SpaceOnTheFocusedChooserDropsTheList) {
    make();
    ui().focus(_chooser);
    ui().key(parts::key(ttk::Code::Space));

    EXPECT_NE(list(), nullptr);
}

TEST_F(ProfileChooserTest, TheListIsNoTabStop) {
    make();
    open();

    ASSERT_NE(list(), nullptr);
    EXPECT_FALSE(list()->takes_focus());
}

TEST_F(ProfileChooserTest, OpeningScrollsTheCurrentProfileIntoView) {
    Config what = tests::Fixtures::config(12, 1);

    what.activeProfileId = "profile-10";
    make(std::move(what));
    open();

    ASSERT_NE(list(), nullptr);

    const double offset = scroll().offset();
    const double view = scroll().box().h;

    EXPECT_GE(offset, (11 * ROW) - view);
    EXPECT_LE(offset, 10 * ROW);
}

TEST_F(ProfileChooserTest, TheWheelScrollsALongList) {
    make(tests::Fixtures::config(12, 1));
    open();

    const BLPoint at = row(2);

    ui().wheel(-3.0, at.x, at.y);
    screen().run(30);

    EXPECT_GT(scroll().offset(), 0.0);
}

TEST_F(ProfileChooserTest, AScrolledListOpensTheRowUnderThePointer) {
    make(tests::Fixtures::config(12, 1));
    open();
    scroll().scroll_to(2 * ROW);
    screen().frame();
    clickRow(0);

    EXPECT_EQ(config().activeProfileId, "profile-2");
}

TEST_F(ProfileChooserTest, TheCurrentProfileIsWashedInTheList) {
    make();
    open();
    screen().full();

    const BLRect &view = scroll().box();
    const int x = static_cast<int>(view.x) + 3;
    const ttk::Theme::Palette &palette = ttk::Theme::palette();

    EXPECT_TRUE(
            parts::leans(screen().at(x, static_cast<int>(view.y + (ROW / 2.0))), palette.accentSoft, palette.raised));
    EXPECT_TRUE(parts::leans(
            screen().at(x, static_cast<int>(view.y + ROW + (ROW / 2.0))), palette.raised, palette.accentSoft));
}

TEST_F(ProfileChooserTest, HoverWashesTheChooser) {
    make();

    const std::vector<std::uint32_t> resting = parts::pixels_in(screen(), BLRectI{24, 50, 4, 4});

    ui().motion(400.0, 50.0);
    screen().frame();

    EXPECT_GT(parts::changed(parts::pixels_in(screen(), BLRectI{24, 50, 4, 4}), resting), 0);

    ui().motion(780.0, 680.0);
    screen().frame();

    EXPECT_EQ(parts::pixels_in(screen(), BLRectI{24, 50, 4, 4}), resting);
}

TEST_F(ProfileChooserTest, OpeningAndClosingRepaintsLikeAFullFrame) {
    make();
    open();
    screen().run(10);
    parts::click(ui(), 760.0, 680.0);
    screen().run(10);

    const parts::Ink off = parts::mismatch(screen());

    EXPECT_EQ(off.pixels, 0U) << off;
}

TEST_F(ProfileChooserTest, HoveringDownTheListRepaintsLikeAFullFrame) {
    make();
    open();

    for (int index = 0; index < 3; ++index) {
        const BLPoint at = row(index);

        ui().motion(at.x, at.y);
        screen().frame();
    }

    const BLRect &box = list()->box();

    ui().motion(box.x + 60.0, box.y + box.h - (ADDER / 2.0));
    screen().frame();

    const parts::Ink off = parts::mismatch(screen());

    EXPECT_EQ(off.pixels, 0U) << off;
}

TEST_F(ProfileChooserTest, ThePillIsHiddenWithNoRunKnown) {
    make();

    EXPECT_FALSE(parts::find<ttk::StatusIndicator>(_chooser)->visible());
}

TEST_F(ProfileChooserTest, AStatusShowsThePillAtTheRightEdge) {
    make();
    _chooser->setStatus(Status::Running, "");
    screen().frame();

    const ttk::StatusIndicator &pill = *parts::find<ttk::StatusIndicator>(_chooser);

    EXPECT_TRUE(pill.visible());
    EXPECT_NEAR(pill.box().x + pill.box().w, SPOT.x + SPOT.w - 10.0, 0.5);
}

TEST_F(ProfileChooserTest, ANewStatusRepaintsLikeAFullFrame) {
    make();
    _chooser->setStatus(Status::Launching, "");
    screen().frame();
    _chooser->setStatus(Status::Failed, "No such file.");
    screen().frame();

    const parts::Ink off = parts::mismatch(screen());

    EXPECT_EQ(off.pixels, 0U) << off;
}

TEST_F(ProfileChooserTest, TheTitleScreenFillsTheThumbOnceItArrives) {
    make(tests::Fixtures::grounded(2, 0));

    const BLRectI thumb{static_cast<int>(SPOT.x) + 20, static_cast<int>(SPOT.y) + 10, 90, 50};
    const int bare = parts::count_near(screen(), thumb, Cards::artMiddle, 4);

    rig().ready();
    _chooser->setSaid("Port 0 · Test Doom", true);
    screen().full();

    EXPECT_GT(bare, 4000);
    EXPECT_LT(parts::count_near(screen(), thumb, Cards::artMiddle, 4), bare / 2);
}

// rowAt() counts from the scroller's top without stopping at its bottom, so the pad under the last row in view
TEST_F(ProfileChooserTest, ThePadUnderTheRowsInViewOpensNoHiddenProfile) {
    make(tests::Fixtures::config(12, 1));
    open();

    ASSERT_NE(list(), nullptr);

    const BLRect &box = list()->box();
    const BLRect &view = scroll().box();
    const double pad = (view.y + view.h + box.y + box.h - ADDER) / 2.0;

    ASSERT_GT(box.y + box.h - ADDER, view.y + view.h);

    parts::click(ui(), view.x + 120.0, pad);

    EXPECT_NE(config().activeProfileId, "profile-8");
}

}
