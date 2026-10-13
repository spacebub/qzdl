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
#include "ttk/notices/Notice.h"
#include "ttk/system/Process.h"
#include "ttk/toolkit/Root.h"
#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/StatusIndicator.h"

#include "Mount.h"
#include "core/config/Session.h"
#include "gui/components/ProfileHead.h"
#include "gui/model/ConfigBridge.h"
#include "gui/state/State.h"
#include "support/Rig.h"

namespace {

constexpr BLRect SPOT{0.0, 20.0, 1180.0, 74.0};

constexpr ttk::Process::Id NOBODY = 987654321;

enum Row : std::uint8_t {
    Rename,
    Duplicate,
    Reset,
    CopyConfig,
    Import,
    SaveAs,
    Delete,
};

class ProfileHeadTest : public parts::Screened {
protected:
    ProfileHeadTest() {
        wide = 1280;
        tall = 480;
    }

    components::ProfileHead *make(Config what = parts::onPort("gzdoom")) {
        parts::install(rig(), std::move(what));

        _head = parts::place(screen(), std::make_unique<components::ProfileHead>(&rig().reach()), SPOT);

        _head->sync();
        screen().frame();

        return _head;
    }

    void sync() {
        rig().shell().advance(0.0);
        _head->sync();
        screen().frame();
    }

    ttk::Button &launch() { return *parts::find<ttk::Button>(_head); }

    ttk::GlyphButton &terminal() { return *parts::all<ttk::GlyphButton>(_head)[0]; }

    ttk::GlyphButton &cog() { return *parts::all<ttk::GlyphButton>(_head)[1]; }

    ttk::Widget *popups() { return screen().ui().layer(ttk::Root::POPUPS); }

    // Rows are 32 tall and rules 9, under a pad of 5.
    void pick(const Row row) {
        parts::click(screen().ui(), cog());

        ASSERT_EQ(popups()->children().size(), 1U);

        const BLRect &menu = popups()->children().front()->box();
        constexpr double tops[] = {0.0, 32.0, 64.0, 105.0, 146.0, 178.0, 219.0};

        parts::click(screen().ui(), menu.x + 60.0, menu.y + 5.0 + tops[row] + 16.0);
    }

    static Profile &active() { return Session::get().config().activeProfile(); }

    std::string key() { return State::get().cfg.profileKey; }

    components::ProfileHead *_head = nullptr;
};

TEST_F(ProfileHeadTest, IsSeventyFourTall) {
    components::ProfileHead *head = make();

    EXPECT_DOUBLE_EQ(head->wanted_height(screen().type(), 1180.0), 74.0);
}

TEST_F(ProfileHeadTest, TheButtonsSitAtTheRightEndOfTheRow) {
    components::ProfileHead *head = make();

    EXPECT_NEAR(launch().box().x + launch().box().w, SPOT.w - ttk::Theme::bleed, 0.5);
    EXPECT_LT(cog().box().x, launch().box().x);
    EXPECT_LT(head->children().front()->box().x + head->children().front()->box().w, cog().box().x);
}

TEST_F(ProfileHeadTest, TheChooserTakesTheRestOfTheRow) {
    components::ProfileHead *head = make();
    const BLRect &chooser = head->children().front()->box();

    EXPECT_DOUBLE_EQ(chooser.x, ttk::Theme::bleed);
    EXPECT_DOUBLE_EQ(chooser.h, 74.0);
    EXPECT_GT(chooser.w, 600.0);
}

TEST_F(ProfileHeadTest, ANarrowerRowNarrowsOnlyTheChooser) {
    components::ProfileHead *head = make();
    const double wide = head->children().front()->box().w;
    const double button = launch().box().w;

    parts::place(screen(), std::make_unique<components::ProfileHead>(&rig().reach()), BLRect{0.0, 20.0, 800.0, 74.0});

    components::ProfileHead *narrow = parts::find<components::ProfileHead>(screen().ui().content());

    narrow->sync();
    screen().frame();

    EXPECT_NEAR(narrow->children().front()->box().w, wide - 380.0, 0.5);
    EXPECT_DOUBLE_EQ(parts::find<ttk::Button>(narrow)->box().w, button);
}

TEST_F(ProfileHeadTest, LaunchIsOnWithAPort) {
    make();

    EXPECT_TRUE(launch().enabled());
}

TEST_F(ProfileHeadTest, LaunchIsOffWithoutAPort) {
    Config what = parts::onPort("gzdoom");

    what.profiles.front().port.clear();
    make(std::move(what));

    EXPECT_FALSE(launch().enabled());
}

TEST_F(ProfileHeadTest, LaunchingAPortThatIsNotThereLandsANotice) {
    make();

    const size_t was = rig().notify().messages().size();

    parts::click(screen().ui(), launch());

    ASSERT_EQ(rig().notify().messages().size(), was + 1);
    EXPECT_EQ(rig().notify().messages().back().severity, ttk::Severity::Error);
    EXPECT_EQ(rig().notify().messages().back().title, "Nothing was launched");
}

TEST_F(ProfileHeadTest, ARefusedLaunchIsMarkedFailed) {
    make();
    parts::click(screen().ui(), launch());

    EXPECT_EQ(rig().runs().stateOf(key()), State::RunState::Failed);
}

TEST_F(ProfileHeadTest, ReturnOnTheFocusedLaunchButtonLaunches) {
    make();
    screen().ui().focus(&launch());
    screen().ui().key(parts::key(ttk::Code::Return));

    EXPECT_EQ(rig().runs().stateOf(key()), State::RunState::Failed);
}

TEST_F(ProfileHeadTest, TheTerminalButtonIsHiddenWhileNothingIsCaptured) {
    make();

    EXPECT_FALSE(terminal().visible());
}

TEST_F(ProfileHeadTest, TheTerminalButtonShowsForACapturingPort) {
    Config what = parts::onPort("gzdoom");

    what.profiles.front().captureOutput = true;
    make(std::move(what));

    EXPECT_TRUE(terminal().visible());
}

TEST_F(ProfileHeadTest, ADosPortHasNoTerminalButton) {
    Config what = parts::onPort("doom2", 3, true);

    what.profiles.front().captureOutput = true;
    make(std::move(what));

    EXPECT_FALSE(terminal().visible());
}

TEST_F(ProfileHeadTest, ClosingOnLaunchHasNoTerminalButton) {
    Config what = parts::onPort("gzdoom");

    what.profiles.front().captureOutput = true;
    what.general.autoClose = true;
    make(std::move(what));

    EXPECT_FALSE(terminal().visible());
}

TEST_F(ProfileHeadTest, TheTerminalButtonWaitsForSomethingLogged) {
    Config what = parts::onPort("gzdoom");

    what.profiles.front().captureOutput = true;
    make(std::move(what));

    EXPECT_FALSE(terminal().enabled());
    EXPECT_EQ(terminal().hint, "Nothing has been launched from this profile yet");

    rig().runs().refused(key(), "Profile", "No such file.");
    sync();

    EXPECT_TRUE(terminal().enabled());
    EXPECT_EQ(terminal().hint, "Show what this profile printed");
}

TEST_F(ProfileHeadTest, TheTerminalButtonShowsThisProfilesLog) {
    Config what = parts::onPort("gzdoom");

    what.profiles.front().captureOutput = true;
    make(std::move(what));
    rig().runs().refused(key(), "Profile", "No such file.");
    sync();
    parts::click(screen().ui(), terminal());

    EXPECT_EQ(State::get().runs.showing, key());
}

TEST_F(ProfileHeadTest, TheStatusPillShowsOnceARunIsKnown) {
    components::ProfileHead *head = make();
    ttk::StatusIndicator *pill = parts::find<ttk::StatusIndicator>(head);

    ASSERT_NE(pill, nullptr);
    EXPECT_FALSE(pill->visible());

    rig().runs().refused(key(), "Profile", "No such file.");
    sync();

    EXPECT_TRUE(pill->visible());
    EXPECT_EQ(pill->status(), ttk::StatusIndicator::Status::Failed);
}

TEST_F(ProfileHeadTest, TheStatusPillSitsInsideTheChoosersRightEdge) {
    components::ProfileHead *head = make();

    rig().runs().refused(key(), "Profile", "No such file.");
    sync();

    const BLRect &chooser = head->children().front()->box();
    const BLRect &pill = parts::find<ttk::StatusIndicator>(head)->box();

    EXPECT_NEAR(pill.x + pill.w, chooser.x + chooser.w - 10.0, 0.5);
    EXPECT_NEAR(pill.y + (pill.h / 2.0), chooser.y + (chooser.h / 2.0), 0.5);
}

TEST_F(ProfileHeadTest, ClickingTheStatusPillShowsTheLog) {
    components::ProfileHead *head = make();

    rig().runs().refused(key(), "Profile", "No such file.");
    sync();
    parts::click(screen().ui(), *parts::find<ttk::StatusIndicator>(head));

    EXPECT_EQ(State::get().runs.showing, key());
}

TEST_F(ProfileHeadTest, TheCogDropsTheMenuUnderItself) {
    make();
    parts::click(screen().ui(), cog());

    ASSERT_EQ(popups()->children().size(), 1U);

    const BLRect &menu = popups()->children().front()->box();

    EXPECT_DOUBLE_EQ(menu.x + menu.w, cog().box().x + cog().box().w);
    EXPECT_DOUBLE_EQ(menu.y, cog().box().y + cog().box().h + 4.0);
}

TEST_F(ProfileHeadTest, ClickingAwayClosesTheMenu) {
    make();
    parts::click(screen().ui(), cog());
    parts::click(screen().ui(), 300.0, 400.0);

    EXPECT_TRUE(popups()->children().empty());
}

TEST_F(ProfileHeadTest, ClosingTheMenuRepaintsLikeAFullFrame) {
    make();
    screen().full();
    parts::click(screen().ui(), cog());
    screen().run(20);
    parts::click(screen().ui(), 300.0, 400.0);
    screen().run(20);

    const parts::Ink off = parts::mismatch(screen());

    EXPECT_EQ(off.pixels, 0U) << off;
}

TEST_F(ProfileHeadTest, RenameAsksStartingFromTheCurrentName) {
    make();

    const std::string name = active().name;

    pick(Rename);

    ASSERT_EQ(rig().prompts.size(), 1U);
    EXPECT_EQ(rig().prompts.front().title, "Rename profile");
    EXPECT_EQ(rig().prompts.front().value, name);
    EXPECT_TRUE(popups()->children().empty());
}

TEST_F(ProfileHeadTest, AcceptingTheRenameRenamesTheProfile) {
    make();
    pick(Rename);

    ASSERT_EQ(rig().prompts.size(), 1U);

    rig().prompts.front().accepted("Ultimate");

    EXPECT_EQ(active().name, "Ultimate");
}

TEST_F(ProfileHeadTest, DuplicateAddsAProfileAndOpensIt) {
    make();

    const std::string was = active().id;

    pick(Duplicate);

    EXPECT_EQ(Session::get().config().profiles.size(), 4U);
    EXPECT_NE(active().id, was);
}

TEST_F(ProfileHeadTest, ResetAsksFirst) {
    make();
    pick(Reset);

    ASSERT_EQ(rig().asks.size(), 1U);
    EXPECT_TRUE(rig().asks.front().danger);
    EXPECT_FALSE(active().port.empty());
}

TEST_F(ProfileHeadTest, AcceptingTheResetEmptiesTheProfile) {
    make();
    pick(Reset);

    ASSERT_EQ(rig().asks.size(), 1U);

    rig().asks.front().accepted();

    EXPECT_TRUE(active().port.empty());
    EXPECT_TRUE(active().files.empty());
    EXPECT_EQ(Session::get().config().profiles.size(), 3U);
}

TEST_F(ProfileHeadTest, CopyPortConfigOpensItsDialog) {
    make();
    pick(CopyConfig);

    EXPECT_EQ(rig().copyConfigShown, 1);
}

TEST_F(ProfileHeadTest, CopyPortConfigIsOffWithoutAPort) {
    Config what = parts::onPort("gzdoom");

    what.profiles.front().port.clear();
    make(std::move(what));
    pick(CopyConfig);

    EXPECT_EQ(rig().copyConfigShown, 0);
}

TEST_F(ProfileHeadTest, ImportOpensTheFilePicker) {
    make();
    pick(Import);

    EXPECT_TRUE(rig().files().state().open);
    EXPECT_EQ(rig().files().state().title, "Load a .zdl launch config");
    EXPECT_FALSE(rig().files().state().saving);
}

TEST_F(ProfileHeadTest, SaveAsOpensASavePicker) {
    make();
    pick(SaveAs);

    EXPECT_TRUE(rig().files().state().open);
    EXPECT_EQ(rig().files().state().title, "Save this profile as a .zdl");
    EXPECT_TRUE(rig().files().state().saving);
}

TEST_F(ProfileHeadTest, DeleteAsksFirst) {
    make();
    pick(Delete);

    ASSERT_EQ(rig().asks.size(), 1U);
    EXPECT_EQ(rig().asks.front().title, "Delete \"" + active().name + "\"?");
    EXPECT_EQ(Session::get().config().profiles.size(), 3U);
}

TEST_F(ProfileHeadTest, AcceptingTheDeleteRemovesItAndGoesToTheLibrary) {
    make();
    pick(Delete);

    ASSERT_EQ(rig().asks.size(), 1U);

    rig().asks.front().accepted();

    EXPECT_EQ(Session::get().config().profiles.size(), 2U);
    EXPECT_EQ(rig().visited, (std::vector<State::Page>{State::Page::Library}));
}

TEST_F(ProfileHeadTest, RenameAndDeleteAreOffWhileTheProfileRuns) {
    make();
    rig().runs().began(key(), "Profile", "gzdoom", NOBODY, ttk::Process::NOTHING);
    pick(Rename);

    EXPECT_TRUE(rig().prompts.empty());

    screen().ui().dismiss();
    pick(Delete);

    EXPECT_TRUE(rig().asks.empty());
}

TEST_F(ProfileHeadTest, APortlessProfileSaysSoInTheWarningTone) {
    Config what = parts::onPort("gzdoom");

    what.profiles.front().port.clear();
    make(std::move(what));
    screen().full();

    const BLRect &chooser = parts::find<components::ProfileHead>(screen().ui().content())->children().front()->box();
    const BLRectI said{static_cast<int>(chooser.x) + 140, static_cast<int>(chooser.y) + 44, 300, 18};
    const ttk::Theme::Palette &palette = ttk::Theme::palette();
    const std::uint32_t inked = parts::peak(screen(), said, palette.background.value);

    EXPECT_TRUE(parts::leans(inked, palette.warning, palette.faint));
}

TEST_F(ProfileHeadTest, AReadyProfileIsSummedUpFaint) {
    make();
    screen().full();

    const BLRect &chooser = parts::find<components::ProfileHead>(screen().ui().content())->children().front()->box();
    const BLRectI said{static_cast<int>(chooser.x) + 140, static_cast<int>(chooser.y) + 44, 300, 18};
    const ttk::Theme::Palette &palette = ttk::Theme::palette();
    const std::uint32_t inked = parts::peak(screen(), said, palette.background.value);

    EXPECT_TRUE(parts::leans(inked, palette.faint, palette.warning));
}

TEST_F(ProfileHeadTest, TabWalksTheChooserTheCogAndLaunch) {
    components::ProfileHead *head = make();

    screen().ui().key(parts::key(ttk::Code::Tab));

    EXPECT_EQ(screen().ui().focused(), head->children().front().get());

    screen().ui().key(parts::key(ttk::Code::Tab));

    EXPECT_EQ(screen().ui().focused(), &cog());

    screen().ui().key(parts::key(ttk::Code::Tab));

    EXPECT_EQ(screen().ui().focused(), &launch());
}

TEST_F(ProfileHeadTest, AProfileThatRanTabsThroughItsPillAndTheTerminal) {
    Config what = parts::onPort("gzdoom");

    what.profiles.front().captureOutput = true;

    components::ProfileHead *head = make(std::move(what));

    rig().runs().refused(key(), "Profile", "No such file.");
    sync();
    screen().ui().key(parts::key(ttk::Code::Tab));
    screen().ui().key(parts::key(ttk::Code::Tab));

    EXPECT_EQ(screen().ui().focused(), parts::find<ttk::StatusIndicator>(head));

    screen().ui().key(parts::key(ttk::Code::Tab));

    EXPECT_EQ(screen().ui().focused(), &terminal());
}

TEST_F(ProfileHeadTest, RestingOnAStatusOffersWhatItSays) {
    components::ProfileHead *head = make();

    rig().runs().refused(key(), "Profile", "No such file.");
    sync();

    const BLPoint at = parts::middle(parts::find<ttk::StatusIndicator>(head)->box());

    screen().ui().motion(at.x, at.y);

    ASSERT_NE(screen().ui().hovered(), nullptr);
    EXPECT_NE(screen().ui().hovered()->hint.find("Click to see what it printed."), std::string::npos);
}

}
