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
#include "gui/components/HostField.h"
#include "gui/components/NetPanel.h"
#include "gui/state/State.h"
#include "support/Rig.h"

namespace {

using parts::key;
namespace Code = ttk::Code;

constexpr BLRect SPOT{20.0, 20.0, 1000.0, 820.0};

Config alone(const std::string &file = "gzdoom") {
    Config made = parts::onEngine(file);

    made.profiles.front().multiplayer = MultiplayerSettings{};

    return made;
}

Config hosting(const std::string &file = "gzdoom", const int players = 4) {
    Config made = parts::onEngine(file);
    Profile &first = made.profiles.front();

    first.multiplayer = MultiplayerSettings{};
    first.multiplayer.gameType = GameType::Coop;
    first.multiplayer.players = players;
    first.multiplayer.port = "5029";
    first.set(Switch::dialogOpen, true);

    return made;
}

Config joining(const std::string &file = "gzdoom", const std::string &host = {}) {
    Config made = parts::onEngine(file);
    Profile &first = made.profiles.front();

    first.multiplayer = MultiplayerSettings{};
    first.multiplayer.gameType = GameType::Coop;
    first.multiplayer.host = host;
    first.multiplayer.port = "5029";
    first.set(Switch::dialogOpen, true);

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

    components::HostField &address() { return *parts::find<components::HostField>(_panel); }

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

// The panel syncs on every change to the profile, and one that changes nothing here paints nothing.
TEST_F(NetPanelTest, ASyncThatChangesNothingPaintsNothing) {
    make(alone("gzdoom"));

    ASSERT_TRUE(_panel->visible());

    _panel->sync();

    EXPECT_EQ(screen().frame(), 0U);
}

TEST_F(NetPanelTest, IsHiddenForAPortWithNoNetgame) {
    make(alone("dsda-doom"));

    EXPECT_FALSE(_panel->visible());
}

TEST_F(NetPanelTest, IsHiddenForADosPort) {
    Config what = alone("doom2");

    what.engines.front().dosbox = true;
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

    EXPECT_TRUE(active().on(Switch::dialogOpen));
    EXPECT_TRUE(_panel->body()->visible());
}

TEST_F(NetPanelTest, ClickingAnOpenHeadingFoldsIt) {
    make(hosting());
    clickHeading();

    EXPECT_FALSE(active().on(Switch::dialogOpen));
    EXPECT_FALSE(_panel->body()->visible());
}

TEST_F(NetPanelTest, ClickingTheRoleSwitchDoesNotFoldThePanel) {
    make(hosting());
    parts::click(ui(), role());
    sync();

    EXPECT_TRUE(active().on(Switch::dialogOpen));
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

    what.profiles.front().set(Switch::dialogOpen, true);
    make(std::move(what));

    EXPECT_TRUE(note().visible());
    EXPECT_TRUE(note().text().starts_with("This profile starts a game for one."));
}

TEST_F(NetPanelTest, PickingHostOpensThePanelAsHost) {
    make(alone());
    press(role(), Code::Right);

    EXPECT_NE(settings().gameType, GameType::None);
    EXPECT_GT(settings().players, 0);
    EXPECT_TRUE(active().on(Switch::dialogOpen));
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
    EXPECT_FALSE(active().on(Switch::dialogOpen));
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
    EXPECT_FALSE(parts::visibleToTheRoot(&address()));
    EXPECT_FALSE(note().visible());
}

TEST_F(NetPanelTest, JoiningShowsOnlyWhereToJoin) {
    make(joining("gzdoom", "10.0.0.7"));

    EXPECT_TRUE(parts::visibleToTheRoot(&address()));
    EXPECT_TRUE(parts::visibleToTheRoot(address().port()));
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
    type(*address().host(), "192.168.0.2");

    EXPECT_EQ(settings().host, "192.168.0.2");
}

TEST_F(NetPanelTest, TypingTheJoinPortWritesIt) {
    make(joining("gzdoom", "10.0.0.7"));
    type(*address().port(), "10666");

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

    EXPECT_EQ(settings().fragLimit, "20");
    EXPECT_EQ(settings().timeLimit, "15");
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
    EXPECT_TRUE(parts::visibleToTheRoot(&address()));
}

TEST_F(NetPanelTest, AChocolatePortShowsTheListedToggle) {
    make(hosting("chocolate-doom"));

    EXPECT_TRUE(parts::visibleToTheRoot(&listed()));
    EXPECT_FALSE(parts::visibleToTheRoot(&field("Frag limit")));
    EXPECT_FALSE(parts::visibleToTheRoot(&stepper("Players")));
}

TEST_F(NetPanelTest, AZDoomPortShowsNoListedToggle) {
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
            _panel,
            "The Saves panel names a save too. A port loads only one, and the save from the Saves panel wins.")));
}

TEST_F(NetPanelTest, ASaveOnlyThisPanelNamesIsNotFlagged) {
    Config what = hosting();

    what.profiles.front().multiplayer.savegame = "/saves/coop.zds";
    make(std::move(what));

    EXPECT_FALSE(parts::visibleToTheRoot(parts::labelled(
            _panel,
            "The Saves panel names a save too. A port loads only one, and the save from the Saves panel wins.")));
    EXPECT_EQ(field("Start from a save").text(), "/saves/coop.zds");
}

TEST_F(NetPanelTest, BrowsingForASaveOpensThePicker) {
    make(hosting());

    ttk::Field &save = field("Start from a save");
    auto *browse = parts::hinted<ttk::GlyphButton>(&save, "Browse");

    ASSERT_NE(browse, nullptr);

    parts::click(ui(), *browse);

    EXPECT_TRUE(rig().files().state().open);
    EXPECT_EQ(rig().files().state().title, "Select a save game");
}

TEST_F(NetPanelTest, UzdoomOffersItsNetgameSaveFolderInPlaceOfThePicker) {
    make(hosting("uzdoom"));

    ttk::Field &save = field("Start from a save");

    EXPECT_NE(parts::hinted<ttk::GlyphButton>(&save, "Open the netgame save folder"), nullptr);
    EXPECT_EQ(parts::hinted<ttk::GlyphButton>(&save, "Browse"), nullptr);
    EXPECT_TRUE(parts::visibleToTheRoot(
            parts::labelled(_panel,
                            "Everyone who joins starts in this save. The port reads netgame saves only from the "
                            "netgame save folder, which has one subfolder per game. Type the name of a save from the "
                            "subfolder for this game.")));
}

TEST_F(NetPanelTest, UzdoomFlagsTheProfileSaveAsLeftOut) {
    Config what = hosting("uzdoom");

    what.profiles.front().save = SaveSettings{.enabled = true, .file = "solo.zds"};
    make(std::move(what));

    EXPECT_TRUE(parts::visibleToTheRoot(
            parts::labelled(_panel,
                            "The save from the Saves panel is left out, since the port reads netgame saves only from "
                            "the netgame save folder.")));
}

TEST_F(NetPanelTest, UzdoomHostingForOneKeepsThePicker) {
    make(hosting("uzdoom", 1));

    EXPECT_NE(parts::hinted<ttk::GlyphButton>(&field("Start from a save"), "Browse"), nullptr);
}

TEST_F(NetPanelTest, UzdoomFlagsNothingWithoutAProfileSave) {
    make(hosting("uzdoom"));

    EXPECT_FALSE(parts::visibleToTheRoot(
            parts::labelled(_panel,
                            "The save from the Saves panel is left out, since the port reads netgame saves only from "
                            "the netgame save folder.")));
}

TEST_F(NetPanelTest, UzdoomJoiningSaysTheProfileSaveIsLeftOut) {
    Config what = joining("uzdoom", "10.0.0.9");

    what.profiles.front().save = SaveSettings{.enabled = true, .file = "solo.zds"};
    make(std::move(what));

    EXPECT_EQ(note().text(),
              "The save from the Saves panel is left out, since the port reads netgame saves only from the "
              "netgame save folder.");
}

TEST_F(NetPanelTest, GzdoomSaysTheNameIsLookedUpInTheSaveFolder) {
    make(hosting());

    EXPECT_TRUE(parts::visibleToTheRoot(parts::labelled(
            _panel, "Everyone who joins starts in this save. The port looks the name up in the save folder.")));
}

TEST_F(NetPanelTest, HostingOnZandronumSaysItStartsAServer) {
    make(hosting("zandronum"));

    EXPECT_TRUE(note().visible());
    EXPECT_EQ(note().text(),
              "This port starts a server when it hosts, and plays no game itself. To play, join it from another "
              "profile.");
    EXPECT_NE(parts::hinted<ttk::Stepper>(_panel, "The most players the server lets in"), nullptr);
}

TEST_F(NetPanelTest, HostingOnGzdoomCountsThisMachineAmongThePlayers) {
    make(hosting());

    EXPECT_FALSE(note().visible());
    EXPECT_NE(parts::hinted<ttk::Stepper>(_panel, "How many the game is opened for, this machine included"), nullptr);
}

TEST_F(NetPanelTest, AZandronumServerStartsFromNoSaveAndHasNoPlayer) {
    for (const char *file : {"zandronum", "skulltag"}) {
        make(hosting(file));

        EXPECT_FALSE(parts::visibleToTheRoot(&field("Start from a save"))) << file;
        EXPECT_FALSE(parts::visibleToTheRoot(&field("Player name"))) << file;
    }

    make(joining("zandronum", "10.0.0.7"));

    EXPECT_TRUE(parts::visibleToTheRoot(&field("Player name")));
}

TEST_F(NetPanelTest, AServerHasNoTicsToTune) {
    for (const char *file : {"zandronum", "skulltag"}) {
        make(hosting(file));
        State::get().nav.tuning = true;
        sync();

        EXPECT_FALSE(parts::visibleToTheRoot(&stepper("Duplicate tics"))) << file;
    }
}

TEST_F(NetPanelTest, ACountAboveThePortLimitShowsAsTheLimit) {
    make(hosting("gzdoom", 40));

    EXPECT_EQ(State::get().cfg.players, 8);
    EXPECT_EQ(stepper("Players").value(), 8);
}

TEST_F(NetPanelTest, AServerStepperStopsAtThePlayerLimitOfThePort) {
    for (const auto &[file, most] : {std::pair{"zandronum", 64}, std::pair{"skulltag", 32}}) {
        make(hosting(file, most - 1));
        parts::click(ui(), *parts::all<ttk::GlyphButton>(&stepper("Players"))[1]);
        sync();
        parts::click(ui(), *parts::all<ttk::GlyphButton>(&stepper("Players"))[1]);

        EXPECT_EQ(settings().players, most) << file;
    }
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

namespace {

ttk::Select &select(ttk::Widget *panel, const std::string &caption) {
    return *parts::captioned<ttk::Select>(panel, caption);
}

}

TEST_F(NetPanelTest, TeamDeathmatchIsOfferedOnlyWithTeams) {
    make(hosting("gzdoom"));

    for (int step = 0; step < 4; step++) {
        press(gameType(), Code::Right);
    }

    EXPECT_EQ(settings().gameType, GameType::TeamDeathmatch);

    make(hosting("chocolate-doom"));

    for (int step = 0; step < 4; step++) {
        press(gameType(), Code::Right);
    }

    EXPECT_EQ(settings().gameType, GameType::AltDeathmatch);
}

TEST_F(NetPanelTest, TheTeamShowsForTeamDeathmatchAndForJoining) {
    make(hosting("gzdoom"));

    EXPECT_FALSE(parts::visibleToTheRoot(&select(_panel, "Team")));

    settings().gameType = GameType::TeamDeathmatch;
    ProfilePanels::pushMultiplayer();
    sync();

    EXPECT_TRUE(parts::visibleToTheRoot(&select(_panel, "Team")));

    make(joining("gzdoom", "10.0.0.7"));

    EXPECT_TRUE(parts::visibleToTheRoot(&select(_panel, "Team")));

    make(joining("zandronum", "10.0.0.7"));

    EXPECT_FALSE(parts::visibleToTheRoot(&select(_panel, "Team")));
}

TEST_F(NetPanelTest, ThePlayerRowShowsOnlyForTheZdoomFamily) {
    make(joining("gzdoom", "10.0.0.7"));

    EXPECT_TRUE(parts::visibleToTheRoot(&field("Player name")));
    EXPECT_TRUE(note().text().contains("name, color and team"));

    make(joining("chocolate-doom", "10.0.0.7"));

    EXPECT_FALSE(parts::visibleToTheRoot(&field("Player name")));
}

TEST_F(NetPanelTest, APresetColorIsWrittenAsHex) {
    make(joining("gzdoom", "10.0.0.7"));
    press(select(_panel, "Color"), Code::Down);
    press(select(_panel, "Color"), Code::Down);

    EXPECT_EQ(settings().playerColor, "#ff0000");
    EXPECT_FALSE(parts::visibleToTheRoot(&field("Hex")));
}

TEST_F(NetPanelTest, CustomOpensTheHexField) {
    Config what = joining("gzdoom", "10.0.0.7");

    what.profiles.front().multiplayer.playerColor = "#123456";
    make(std::move(what));

    EXPECT_EQ(select(_panel, "Color").current(), 8);
    EXPECT_TRUE(parts::visibleToTheRoot(&field("Hex")));
    EXPECT_EQ(field("Hex").text(), "#123456");
}

TEST_F(NetPanelTest, PickingATeamWritesIt) {
    make(joining("gzdoom", "10.0.0.7"));
    press(select(_panel, "Team"), Code::Down);
    press(select(_panel, "Team"), Code::Down);

    EXPECT_EQ(settings().team, 1);
}

TEST_F(NetPanelTest, ThePlayerRowLinesUpAlongItsFoot) {
    Config what = joining("gzdoom", "10.0.0.7");

    what.profiles.front().multiplayer.playerColor = "#123456";
    make(std::move(what));

    const auto foot = [](const ttk::Widget &widget) { return widget.box().y + widget.box().h; };
    const double name = foot(field("Player name"));

    EXPECT_DOUBLE_EQ(foot(select(_panel, "Color")), name);
    EXPECT_DOUBLE_EQ(foot(field("Hex")), name);
    EXPECT_DOUBLE_EQ(foot(select(_panel, "Team")), name);
    EXPECT_DOUBLE_EQ(select(_panel, "Color").box().y, field("Player name").box().y) << "the captions do not line up";
}

TEST_F(NetPanelTest, ThePillGrowsAtOnceForALongerGameType) {
    make(hosting("gzdoom"));

    // Nothing else in the panel changes with it, so nothing else lays the heading out.
    rig().config().panels().setGameType(GameType::Deathmatch);

    // As the window does it: no layout forced from outside.
    rig().shell().advance(0.0);
    _panel->sync();
    screen().frame();

    EXPECT_DOUBLE_EQ(_panel->pill()->box().w, _panel->pill()->natural_width(screen().type()));
}

TEST_F(NetPanelTest, AColonInTheAddressMovesThePortOver) {
    make(joining());
    type(*address().host(), "10.0.0.7:10666");
    sync();

    EXPECT_EQ(settings().host, "10.0.0.7");
    EXPECT_EQ(settings().port, "10666");
    EXPECT_EQ(address().host()->text(), "10.0.0.7");
    EXPECT_EQ(address().port()->text(), "10666");
    EXPECT_EQ(ui().focused(), address().port());
}

TEST_F(NetPanelTest, AnIpv6AddressKeepsItsColons) {
    make(joining());
    type(*address().host(), "fe80::1");
    sync();

    EXPECT_EQ(settings().host, "fe80::1");
    EXPECT_EQ(address().host()->text(), "fe80::1");
}

TEST_F(NetPanelTest, AColonOnItsOwnKeepsThePort) {
    make(joining("gzdoom", "10.0.0.7"));
    type(*address().host(), "10.0.0.7:");
    sync();

    EXPECT_EQ(settings().host, "10.0.0.7");
    EXPECT_EQ(settings().port, "5029");
    EXPECT_EQ(ui().focused(), address().port());
}

TEST_F(NetPanelTest, AnEmptiedHexFieldStaysOpen) {
    Config what = joining("gzdoom", "10.0.0.7");

    what.profiles.front().multiplayer.playerColor = "#123456";
    make(std::move(what));
    type(*field("Hex").input(), "");
    sync();

    EXPECT_EQ(settings().playerColor, "");
    EXPECT_TRUE(parts::visibleToTheRoot(&field("Hex")));
}

TEST_F(NetPanelTest, TeamDeathmatchWithoutTeamsShowsAsDeathmatch) {
    Config what = hosting("woof");

    what.profiles.front().multiplayer.gameType = GameType::TeamDeathmatch;
    make(std::move(what));

    EXPECT_EQ(gameType().current(), static_cast<int>(GameType::Deathmatch));
}

TEST_F(NetPanelTest, WoofOffersDeathmatchThree) {
    make(hosting("woof"));
    press(gameType(), Code::End);

    EXPECT_EQ(settings().gameType, GameType::Deathmatch3);
    EXPECT_EQ(gameType().current(), static_cast<int>(GameType::Deathmatch3));
}

TEST_F(NetPanelTest, StrifeShowsAStoredCoopAsDeathmatch) {
    make(hosting("chocolate-strife"));

    EXPECT_EQ(gameType().current(), static_cast<int>(GameType::Deathmatch));
    EXPECT_EQ(settings().gameType, GameType::Coop);

    press(gameType(), Code::End);

    EXPECT_EQ(settings().gameType, GameType::AltDeathmatch);

    press(gameType(), Code::Home);

    EXPECT_EQ(settings().gameType, GameType::Deathmatch) << "Strife offers no co-op";

    press(gameType(), Code::Right);

    EXPECT_EQ(settings().gameType, GameType::AltDeathmatch);
}

TEST_F(NetPanelTest, HereticShowsAStoredAltDeathmatchAsDeathmatch) {
    Config what = hosting("crispy-heretic");

    what.profiles.front().multiplayer.gameType = GameType::AltDeathmatch;
    make(std::move(what));

    EXPECT_EQ(gameType().current(), static_cast<int>(GameType::Deathmatch));
    EXPECT_EQ(settings().gameType, GameType::AltDeathmatch);

    press(gameType(), Code::Home);

    EXPECT_EQ(settings().gameType, GameType::Coop);

    press(gameType(), Code::End);

    EXPECT_EQ(settings().gameType, GameType::Deathmatch) << "Heretic offers no alt deathmatch";
}

TEST_F(NetPanelTest, ChocolateOffersNoDeathmatchThreeAndShowsItAsDeathmatch) {
    Config what = hosting("chocolate-doom");

    what.profiles.front().multiplayer.gameType = GameType::Deathmatch3;
    make(std::move(what));

    EXPECT_EQ(gameType().current(), static_cast<int>(GameType::Deathmatch));

    press(gameType(), Code::End);

    EXPECT_EQ(settings().gameType, GameType::AltDeathmatch);
}
