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
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/draw/Theme.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/MultistateSwitch.h"
#include "ttk/toolkit/controls/Stepper.h"
#include "ttk/toolkit/controls/Toggle.h"
#include "ttk/toolkit/layout/Collapsible.h"

#include "Mount.h"
#include "core/config/Session.h"
#include "gui/components/NetPanel.h"
#include "gui/state/State.h"
#include "support/Rig.h"

namespace {

using parts::key;
namespace Code = ttk::Code;

constexpr BLRect SPOT{20.0, 20.0, 1000.0, 820.0};

Config alone(const std::string &program = "gzdoom") {
    Config made = parts::onPort(program);

    made.profiles.front().multiplayer = MultiplayerSettings{};

    return made;
}

Config hosting(const std::string &program = "gzdoom", const int players = 4) {
    Config made = parts::onPort(program);
    Profile &first = made.profiles.front();

    first.multiplayer = MultiplayerSettings{};
    first.multiplayer.gameType = GameType::Coop;
    first.multiplayer.players = players;
    first.multiplayer.port = "5029";
    first.dialogOpen = true;

    return made;
}

Config joining(const std::string &program = "gzdoom", const std::string &host = {}) {
    Config made = parts::onPort(program);
    Profile &first = made.profiles.front();

    first.multiplayer = MultiplayerSettings{};
    first.multiplayer.gameType = GameType::Coop;
    first.multiplayer.host = host;
    first.multiplayer.port = "5029";
    first.dialogOpen = true;

    return made;
}

class NetPanelTest : public parts::Screened {
protected:
    components::NetPanel *make(Config what) {
        parts::install(rig(), std::move(what));

        _panel = parts::place(screen(), std::make_unique<components::NetPanel>(&rig().reach()), SPOT);

        sync();

        return _panel;
    }

    // Runs the fold to its end, so what is in the body takes the pointer.
    void sync() {
        rig().shell().advance(0.0);
        _panel->sync();
        screen().ui().relayout();
        screen().run(30);
    }

    static MultiplayerSettings &settings() { return Session::get().config().activeProfile().multiplayer; }

    static Profile &active() { return Session::get().config().activeProfile(); }

    ttk::Label &said() { return *static_cast<ttk::Label *>(_panel->pill()->parent()->children()[2].get()); }

    ttk::Label &note() { return *static_cast<ttk::Label *>(_panel->body()->children()[1].get()); }

    ttk::MultistateSwitch &role() {
        return *static_cast<ttk::MultistateSwitch *>(_panel->tools()->children()[0].get());
    }

    ttk::GlyphButton &reset() { return *static_cast<ttk::GlyphButton *>(_panel->tools()->children()[1].get()); }

    ttk::MultistateSwitch &gameType() { return *parts::all<ttk::MultistateSwitch>(_panel)[1]; }

    ttk::MultistateSwitch &netmode() { return *parts::all<ttk::MultistateSwitch>(_panel)[2]; }

    ttk::MultistateSwitch &extratic() { return *parts::all<ttk::MultistateSwitch>(_panel)[3]; }

    ttk::Field &field(const std::string &caption) { return *parts::captioned<ttk::Field>(_panel, caption); }

    ttk::Stepper &stepper(const std::string &caption) { return *parts::captioned<ttk::Stepper>(_panel, caption); }

    ttk::Toggle &listed() { return *parts::find<ttk::Toggle>(_panel); }

    ttk::DisclosureHeading &connection() { return *parts::find<ttk::DisclosureHeading>(_panel); }

    void press(ttk::Widget &target, const int code) {
        ui().focus(&target);
        ui().key(key(code));
        sync();
    }

    void clickHeading() {
        parts::click(ui(), SPOT.x + 40.0, SPOT.y + 16.0 + (ttk::Theme::control / 2.0));
        sync();
    }

    components::NetPanel *_panel = nullptr;
};

TEST_F(NetPanelTest, IsHiddenForAPortWithNoNetgame) {
    make(alone("dsda-doom"));

    EXPECT_FALSE(_panel->visible());
}

TEST_F(NetPanelTest, IsHiddenForADosPort) {
    Config what = alone("doom2");

    what.ports.front().dosbox = true;
    make(std::move(what));

    EXPECT_FALSE(_panel->visible());
}

TEST_F(NetPanelTest, IsShownForAZDoomPort) {
    make(alone());

    EXPECT_TRUE(_panel->visible());
}

TEST_F(NetPanelTest, FoldedItIsOnlyItsHeading) {
    make(alone());

    EXPECT_DOUBLE_EQ(_panel->wanted_height(screen().type(), SPOT.w), ttk::Theme::control + 32.0);
    EXPECT_FALSE(_panel->body()->visible());
}

TEST_F(NetPanelTest, OpenItGrowsToHoldItsBody) {
    make(hosting());

    EXPECT_GT(_panel->wanted_height(screen().type(), SPOT.w), ttk::Theme::control + 200.0);
    EXPECT_TRUE(_panel->body()->visible());
}

TEST_F(NetPanelTest, ClickingTheHeadingOpensIt) {
    make(alone());
    clickHeading();

    EXPECT_TRUE(active().dialogOpen);
    EXPECT_TRUE(_panel->body()->visible());
}

TEST_F(NetPanelTest, ClickingAnOpenHeadingFoldsIt) {
    make(hosting());
    clickHeading();

    EXPECT_FALSE(active().dialogOpen);
    EXPECT_FALSE(_panel->body()->visible());
}

TEST_F(NetPanelTest, ClickingTheRoleSwitchDoesNotFoldThePanel) {
    make(hosting());
    parts::click(ui(), role());
    sync();

    EXPECT_TRUE(active().dialogOpen);
}

TEST_F(NetPanelTest, FoldingRepaintsLikeAFullFrame) {
    make(hosting());
    screen().full();
    parts::click(ui(), SPOT.x + 40.0, SPOT.y + 37.0);
    rig().shell().advance(0.0);
    _panel->sync();
    screen().run(5);

    const parts::Ink off = parts::mismatch(screen());

    EXPECT_EQ(off.pixels, 0U) << off;
}

TEST_F(NetPanelTest, ASinglePlayerProfileSaysSo) {
    make(alone());

    EXPECT_EQ(said().text(), "Off · this profile launches a single player game");
    EXPECT_FALSE(_panel->pill()->visible());
    EXPECT_EQ(role().current(), static_cast<int>(NetRole::Alone));
}

TEST_F(NetPanelTest, ASinglePlayerProfileExplainsTheSides) {
    Config what = alone();

    what.profiles.front().dialogOpen = true;
    make(std::move(what));

    EXPECT_TRUE(note().visible());
    EXPECT_TRUE(note().text().starts_with("This profile starts a game for one."));
}

TEST_F(NetPanelTest, PickingHostOpensThePanelAsHost) {
    make(alone());
    press(role(), Code::Right);

    EXPECT_NE(settings().gameType, GameType::None);
    EXPECT_GT(settings().players, 0);
    EXPECT_TRUE(active().dialogOpen);
}

TEST_F(NetPanelTest, PickingJoinJoins) {
    make(alone());
    press(role(), Code::End);

    EXPECT_NE(settings().gameType, GameType::None);
    EXPECT_EQ(settings().players, 0);
}

TEST_F(NetPanelTest, PickingOffFoldsThePanel) {
    make(hosting());
    press(role(), Code::Home);

    EXPECT_EQ(settings().gameType, GameType::None);
    EXPECT_FALSE(active().dialogOpen);
}

TEST_F(NetPanelTest, HostingSaysForHowManyOnWhichPort) {
    make(hosting("gzdoom", 4));

    EXPECT_EQ(said().text(), "for 4 players, on port 5029");
    EXPECT_TRUE(_panel->pill()->visible());
}

TEST_F(NetPanelTest, HostingShowsTheGameAndItsRules) {
    make(hosting());

    EXPECT_TRUE(parts::visibleToTheRoot(&gameType()));
    EXPECT_TRUE(parts::visibleToTheRoot(&field("Frag limit")));
    EXPECT_TRUE(parts::visibleToTheRoot(&field("dmflags")));
    EXPECT_FALSE(parts::visibleToTheRoot(&field("Address of the game")));
    EXPECT_FALSE(note().visible());
}

TEST_F(NetPanelTest, JoiningShowsOnlyWhereToJoin) {
    make(joining("gzdoom", "10.0.0.7"));

    EXPECT_TRUE(parts::visibleToTheRoot(&field("Address of the game")));
    EXPECT_TRUE(parts::visibleToTheRoot(&field("Port")));
    EXPECT_FALSE(parts::visibleToTheRoot(&gameType()));
    EXPECT_FALSE(parts::visibleToTheRoot(&field("Frag limit")));
}

TEST_F(NetPanelTest, JoiningSaysTheAddressAndPort) {
    make(joining("gzdoom", "10.0.0.7"));

    EXPECT_EQ(said().text(), "10.0.0.7:5029");
    EXPECT_TRUE(note().text().starts_with("How the game is played is the host's"));
}

TEST_F(NetPanelTest, JoiningWithoutAnAddressWarns) {
    make(joining());

    EXPECT_EQ(said().text(), "no address yet");
    EXPECT_TRUE(note().visible());
    EXPECT_TRUE(note().text().starts_with("Without an address there is nothing to join"));
}

TEST_F(NetPanelTest, TypingTheAddressWritesIt) {
    make(joining());
    type(field("Address of the game"), "192.168.0.2");

    EXPECT_EQ(settings().host, "192.168.0.2");
}

TEST_F(NetPanelTest, TypingTheJoinPortWritesIt) {
    make(joining("gzdoom", "10.0.0.7"));
    type(field("Port"), "10666");

    EXPECT_EQ(settings().port, "10666");
}

TEST_F(NetPanelTest, TypingTheListenPortWritesIt) {
    Config what = hosting();

    what.profiles.front().multiplayer.port.clear();
    make(std::move(what));
    type(field("Listen on port"), "10666");

    EXPECT_EQ(settings().port, "10666");
}

TEST_F(NetPanelTest, TypingTheRulesWritesThem) {
    make(hosting());
    type(field("Frag limit"), "20");
    type(field("Time limit"), "15");
    type(field("dmflags"), "1024");
    type(field("dmflags2"), "512");

    EXPECT_EQ(settings().fragLimit, "20");
    EXPECT_EQ(settings().timeLimit, "15");
    EXPECT_EQ(settings().dmflags, "1024");
    EXPECT_EQ(settings().dmflags2, "512");
}

TEST_F(NetPanelTest, TheFieldsShowWhatIsSet) {
    Config what = hosting();

    what.profiles.front().multiplayer.fragLimit = "50";
    make(std::move(what));

    EXPECT_EQ(field("Frag limit").text(), "50");
    EXPECT_EQ(field("Listen on port").text(), "5029");
}

TEST_F(NetPanelTest, ThePlayersStepperCountsUp) {
    make(hosting("gzdoom", 4));

    ttk::Stepper &players = stepper("Players");

    EXPECT_EQ(players.value(), 4);

    parts::click(ui(), *parts::all<ttk::GlyphButton>(&players)[1]);

    EXPECT_EQ(settings().players, 5);
}

TEST_F(NetPanelTest, ThePlayersStepperCountsDown) {
    make(hosting("gzdoom", 4));
    parts::click(ui(), *parts::all<ttk::GlyphButton>(&stepper("Players"))[0]);

    EXPECT_EQ(settings().players, 3);
}

TEST_F(NetPanelTest, TheGameTypeSwitchPicksDeathmatch) {
    make(hosting());
    press(gameType(), Code::Right);

    EXPECT_EQ(settings().gameType, GameType::Deathmatch);
}

TEST_F(NetPanelTest, AJoinOnlyPortCannotHost) {
    make(hosting("prboom-plus"));

    EXPECT_EQ(said().text(), "not a side this port takes");
    EXPECT_TRUE(note().text().starts_with("This port opens no game of its own"));
    EXPECT_FALSE(parts::visibleToTheRoot(&gameType()));
    EXPECT_FALSE(parts::visibleToTheRoot(&connection()));
}

TEST_F(NetPanelTest, AJoinOnlyPortJoins) {
    make(joining("prboom-plus", "10.0.0.9"));

    EXPECT_EQ(said().text(), "10.0.0.9:5029");
    EXPECT_TRUE(parts::visibleToTheRoot(&field("Address of the game")));
}

TEST_F(NetPanelTest, AChocolatePortHasAPublicListing) {
    make(hosting("chocolate-doom"));

    EXPECT_TRUE(parts::visibleToTheRoot(&listed()));
    EXPECT_FALSE(parts::visibleToTheRoot(&field("Frag limit")));
    EXPECT_FALSE(parts::visibleToTheRoot(&stepper("Players")));
}

TEST_F(NetPanelTest, AZDoomPortHasNoPublicListing) {
    make(hosting());

    EXPECT_FALSE(parts::visibleToTheRoot(&listed()));
}

TEST_F(NetPanelTest, ClickingPublicListsTheGame) {
    make(hosting("chocolate-doom"));
    parts::flip(ui(), listed());

    EXPECT_TRUE(settings().listed);
}

TEST_F(NetPanelTest, ResetIsOffWithNothingSet) {
    make(alone());

    EXPECT_FALSE(reset().enabled());
    EXPECT_EQ(reset().hint, "Nothing here has been set");
}

TEST_F(NetPanelTest, ResetAsksFirst) {
    make(hosting());

    EXPECT_TRUE(reset().enabled());

    parts::click(ui(), reset());

    ASSERT_EQ(rig().asks.size(), 1U);
    EXPECT_EQ(rig().asks.front().title, "Reset the multiplayer settings?");
    EXPECT_TRUE(rig().asks.front().danger);
    EXPECT_EQ(settings().players, 4);
}

TEST_F(NetPanelTest, AcceptingTheResetClearsEverySetting) {
    make(hosting());
    parts::click(ui(), reset());

    ASSERT_EQ(rig().asks.size(), 1U);

    rig().asks.front().accepted();

    EXPECT_EQ(settings(), MultiplayerSettings{});
}

TEST_F(NetPanelTest, TheConnectionIsLeftToThePortUntilTuned) {
    make(hosting());

    EXPECT_TRUE(parts::visibleToTheRoot(&connection()));
    EXPECT_NE(parts::labelled(&connection(), "left to the port"), nullptr);
}

TEST_F(NetPanelTest, TheConnectionSaysWhatIsTuned) {
    Config what = hosting();

    what.profiles.front().multiplayer.netmode = 1;
    what.profiles.front().multiplayer.dup = 2;
    what.profiles.front().multiplayer.extratic = true;
    make(std::move(what));

    EXPECT_NE(parts::labelled(&connection(), "client/server · 2× tics · extra tic"), nullptr);
}

TEST_F(NetPanelTest, ClickingTheConnectionUnfoldsIt) {
    make(hosting());

    EXPECT_FALSE(State::get().nav.tuning);

    parts::click(ui(), connection().box().x + 30.0, connection().box().y + 9.0);
    sync();

    EXPECT_TRUE(State::get().nav.tuning);
    EXPECT_TRUE(parts::visibleToTheRoot(&netmode()));
}

TEST_F(NetPanelTest, PickingClientServerWritesTheNetMode) {
    make(hosting());
    State::get().nav.tuning = true;
    sync();
    press(netmode(), Code::End);

    EXPECT_EQ(settings().netmode, 1);
}

TEST_F(NetPanelTest, TurningOnTheExtraTicWritesIt) {
    make(hosting());
    State::get().nav.tuning = true;
    sync();
    press(extratic(), Code::Right);

    EXPECT_TRUE(settings().extratic);
}

TEST_F(NetPanelTest, UZDoomHasNoNetMode) {
    make(hosting("uzdoom"));
    State::get().nav.tuning = true;
    sync();

    EXPECT_FALSE(parts::visibleToTheRoot(&netmode()));
    EXPECT_TRUE(parts::visibleToTheRoot(&stepper("Duplicate tics")));
}

TEST_F(NetPanelTest, AJoinOnlyPortHasNothingToTune) {
    make(joining("prboom-plus", "10.0.0.9"));

    EXPECT_FALSE(parts::visibleToTheRoot(&connection()));
}

TEST_F(NetPanelTest, ASaveTheSavesPanelAlsoNamesIsFlagged) {
    Config what = hosting();

    what.profiles.front().multiplayer.savegame = "/saves/coop.zds";
    what.profiles.front().save = SaveSettings{.enabled = true, .file = "solo.zds"};
    make(std::move(what));

    EXPECT_TRUE(parts::visibleToTheRoot(parts::labelled(
            _panel, "The Saves panel names one too, and a port loads one save: that is the one it gets.")));
}

TEST_F(NetPanelTest, ASaveOnlyThisPanelNamesIsNotFlagged) {
    Config what = hosting();

    what.profiles.front().multiplayer.savegame = "/saves/coop.zds";
    make(std::move(what));

    EXPECT_FALSE(parts::visibleToTheRoot(parts::labelled(
            _panel, "The Saves panel names one too, and a port loads one save: that is the one it gets.")));
    EXPECT_EQ(field("Start from a save").text(), "/saves/coop.zds");
}

TEST_F(NetPanelTest, BrowsingForASaveOpensThePicker) {
    make(hosting());

    ttk::Field &save = field("Start from a save");
    ttk::GlyphButton *browse = parts::hinted<ttk::GlyphButton>(&save, "Browse");

    ASSERT_NE(browse, nullptr);

    parts::click(ui(), *browse);

    EXPECT_TRUE(rig().files().state().open);
    EXPECT_EQ(rig().files().state().title, "Select a save game");
}

TEST_F(NetPanelTest, TypingASaveWritesIt) {
    make(hosting());
    type(field("Start from a save"), "/saves/coop.zds");

    EXPECT_EQ(settings().savegame, "/saves/coop.zds");
}

TEST_F(NetPanelTest, TheDuplicateTicsStepperWritesThem) {
    make(hosting());
    State::get().nav.tuning = true;
    sync();
    parts::click(ui(), *parts::all<ttk::GlyphButton>(&stepper("Duplicate tics"))[1]);

    EXPECT_EQ(settings().dup, 1);
}

TEST_F(NetPanelTest, ThePlayersStepperStopsAtEight) {
    make(hosting("gzdoom", 8));
    parts::click(ui(), *parts::all<ttk::GlyphButton>(&stepper("Players"))[1]);

    EXPECT_EQ(settings().players, 8);
}

}
