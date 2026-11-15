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

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Paths.h"
#include "ttk/util/Format.h"

#include "Model.h"
#include "core/config/Config.h"
#include "core/launch/Flags.h"
#include "core/launch/Rules.h"
#include "core/ports/Zip.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/ProfilePanels.h"
#include "gui/state/State.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

class ProfilePanelsTest : public model::Bridged {
protected:
    ProfilePanels &panels() const { return bridge().panels(); }

    static MultiplayerSettings &mp() { return config().activeProfile().multiplayer; }

    static ReplaySettings &demo() { return config().activeProfile().replay; }

    static SaveSettings &kept() { return config().activeProfile().save; }

    // Alpha alone, on `port`.
    void on(const std::string &port, const bool profileConfigs = false) const {
        Config made = model::world(1);

        made.profiles.front().port = port;
        made.general.profileConfigs = profileConfigs;
        install(made);
    }

    static fs::path replays() { return model::ownFolder("Alpha") / "replays"; }

    static fs::path saves() { return model::ownFolder("Alpha") / "saves"; }
};

class Multiplayer : public ProfilePanelsTest {};

TEST_F(Multiplayer, NoGameTypeIsPlayingAlone) {
    EXPECT_EQ(ProfilePanels::netRoleOf(MultiplayerSettings{}), NetRole::Alone);
}

TEST_F(Multiplayer, AGameWithPlayersIsHosted) {
    MultiplayerSettings set;

    set.gameType = GameType::Deathmatch;
    set.players = 3;

    EXPECT_EQ(ProfilePanels::netRoleOf(set), NetRole::Host);
}

TEST_F(Multiplayer, AGameWithNoPlayersIsJoined) {
    MultiplayerSettings set;

    set.gameType = GameType::Coop;

    EXPECT_EQ(ProfilePanels::netRoleOf(set), NetRole::Join);
}

TEST_F(Multiplayer, TheZdoomFamilyDoesEverythingButListing) {
    on("GZDoom");

    EXPECT_TRUE(cfg().netHosts);
    EXPECT_TRUE(cfg().netJoins);
    EXPECT_TRUE(cfg().netPlayers);
    EXPECT_TRUE(cfg().netFragLimit);
    EXPECT_TRUE(cfg().netSavegame);
    EXPECT_TRUE(cfg().netExtratic);
    EXPECT_TRUE(cfg().hasNetmode);
    EXPECT_TRUE(cfg().netDup);
    EXPECT_FALSE(cfg().netListing);
}

TEST_F(Multiplayer, UzdoomHasNoNetmode) {
    on("UZDoom");

    EXPECT_TRUE(cfg().netHosts);
    EXPECT_FALSE(cfg().hasNetmode);
}

TEST_F(Multiplayer, UzdoomHostingForSeveralShowsTheNetgameSaveFolder) {
    on("UZDoom");
    panels().setNetRole(NetRole::Host);
    panels().setPlayers(4);

    EXPECT_FALSE(cfg().netSaveFolder.empty());
    EXPECT_TRUE(cfg().netSaveByName);
}

TEST_F(Multiplayer, UzdoomHostingForOneUsesTheOrdinarySaveFolder) {
    on("UZDoom");
    panels().setNetRole(NetRole::Host);
    panels().setPlayers(1);

    EXPECT_TRUE(cfg().netSaveFolder.empty());
}

TEST_F(Multiplayer, GzdoomHasNoNetgameSaveFolder) {
    on("GZDoom");
    panels().setNetRole(NetRole::Host);
    panels().setPlayers(4);

    EXPECT_TRUE(cfg().netSaveFolder.empty());
}

TEST_F(Multiplayer, TheChocolateFamilyHostsJoinsListsAndDuplicates) {
    on("Chocolate");

    EXPECT_TRUE(cfg().netHosts);
    EXPECT_TRUE(cfg().netJoins);
    EXPECT_TRUE(cfg().netListing);
    EXPECT_TRUE(cfg().netDup);
    EXPECT_FALSE(cfg().netPlayers);
    EXPECT_FALSE(cfg().netFragLimit);
    EXPECT_FALSE(cfg().netSavegame);
    EXPECT_FALSE(cfg().netExtratic);
    EXPECT_FALSE(cfg().hasNetmode);
}

TEST_F(Multiplayer, WoofPlaysTheChocolateWay) {
    on("Woof");

    EXPECT_TRUE(cfg().netHosts);
    EXPECT_TRUE(cfg().netListing);
    EXPECT_FALSE(cfg().netPlayers);
}

TEST_F(Multiplayer, PrBoomOnlyJoins) {
    on("PrBoom");

    EXPECT_TRUE(cfg().netJoins);
    EXPECT_FALSE(cfg().netHosts);
    EXPECT_FALSE(cfg().netListing);
    EXPECT_FALSE(cfg().netDup);
}

TEST_F(Multiplayer, DsdaHasNoNetgame) {
    on("DSDA");

    EXPECT_FALSE(cfg().netHosts);
    EXPECT_FALSE(cfg().netJoins);
    EXPECT_FALSE(cfg().netDup);
}

TEST_F(Multiplayer, NothingUnderDosboxPlaysOverTheNet) {
    on("Vanilla");

    EXPECT_FALSE(cfg().netHosts);
    EXPECT_FALSE(cfg().netJoins);
}

TEST_F(Multiplayer, NoPortIsTakenForTheZdoomFamily) {
    on("");

    EXPECT_TRUE(cfg().netHosts);
    EXPECT_TRUE(cfg().netPlayers);
}

TEST_F(Multiplayer, DefaultsAreNotSet) {
    on("GZDoom");

    EXPECT_FALSE(cfg().multiplayerSet);
    EXPECT_EQ(cfg().netRole, NetRole::Alone);
}

TEST_F(Multiplayer, HostingFromAloneStartsACoopForTwo) {
    on("GZDoom");

    panels().setNetRole(NetRole::Host);

    EXPECT_EQ(mp().gameType, GameType::Coop);
    EXPECT_EQ(mp().players, 2);
    EXPECT_EQ(cfg().netRole, NetRole::Host);
    EXPECT_EQ(cfg().gameType, GameType::Coop);
    EXPECT_EQ(cfg().players, 2);
    EXPECT_TRUE(cfg().multiplayerSet);
}

TEST_F(Multiplayer, HostingKeepsAPlayerCountAboveTwo) {
    on("GZDoom");

    mp().players = 6;
    panels().setNetRole(NetRole::Host);

    EXPECT_EQ(mp().players, 6);
}

TEST_F(Multiplayer, HostingKeepsTheGameTypeChosen) {
    on("GZDoom");

    mp().gameType = GameType::Deathmatch;
    panels().setNetRole(NetRole::Host);

    EXPECT_EQ(mp().gameType, GameType::Deathmatch);
}

TEST_F(Multiplayer, JoiningClearsThePlayerCount) {
    on("GZDoom");

    panels().setNetRole(NetRole::Host);
    panels().setNetRole(NetRole::Join);

    EXPECT_EQ(mp().players, 0);
    EXPECT_EQ(mp().gameType, GameType::Coop);
    EXPECT_EQ(cfg().netRole, NetRole::Join);
}

TEST_F(Multiplayer, PlayingAloneClearsTheGameType) {
    on("GZDoom");

    panels().setNetRole(NetRole::Host);
    panels().setNetRole(NetRole::Alone);

    EXPECT_EQ(mp().gameType, GameType::None);
    EXPECT_EQ(cfg().netRole, NetRole::Alone);
}

TEST_F(Multiplayer, TheSameRoleAgainChangesNothing) {
    on("GZDoom");

    const int before = cfg().rev;

    panels().setNetRole(NetRole::Alone);

    EXPECT_EQ(cfg().rev, before);
}

TEST_F(Multiplayer, TheRoleIsSaved) {
    on("GZDoom");

    panels().setNetRole(NetRole::Host);
    settle();

    EXPECT_EQ(saved().activeProfile().multiplayer.players, 2);
    EXPECT_EQ(saved().activeProfile().multiplayer.gameType, GameType::Coop);
}

TEST_F(Multiplayer, TheRoleShowsOnTheCard) {
    on("GZDoom");

    panels().setNetRole(NetRole::Host);

    EXPECT_EQ(cfg().profileCards.front().netRole, NetRole::Host);
    EXPECT_TRUE(cfg().profileCards.front().netSupported);
}

TEST_F(Multiplayer, HostingReachesTheCommandLine) {
    on("GZDoom");

    panels().setNetRole(NetRole::Host);
    preview();

    EXPECT_TRUE(cfg().commandLine.contains("-host 2")) << cfg().commandLine;
}

TEST_F(Multiplayer, TheGameTypeIsWrittenPushedAndCarded) {
    on("GZDoom");

    panels().setGameType(GameType::AltDeathmatch);

    EXPECT_EQ(mp().gameType, GameType::AltDeathmatch);
    EXPECT_EQ(cfg().gameType, GameType::AltDeathmatch);
    EXPECT_EQ(cfg().profileCards.front().netRole, NetRole::Join);
}

TEST_F(Multiplayer, ThePlayersAreWrittenPushedAndCarded) {
    on("GZDoom");

    panels().setGameType(GameType::Coop);
    panels().setPlayers(4);

    EXPECT_EQ(mp().players, 4);
    EXPECT_EQ(cfg().players, 4);
    EXPECT_EQ(cfg().profileCards.front().netRole, NetRole::Host);
}

TEST_F(Multiplayer, TheHostIsWrittenAndPushed) {
    on("GZDoom");

    panels().setHost("10.0.0.7");

    EXPECT_EQ(mp().host, "10.0.0.7");
    EXPECT_EQ(cfg().host, "10.0.0.7");
}

TEST_F(Multiplayer, JoiningAHostReachesTheCommandLine) {
    on("GZDoom");

    panels().setGameType(GameType::Coop);
    panels().setHost("10.0.0.7");
    preview();

    EXPECT_TRUE(cfg().commandLine.contains("10.0.0.7")) << cfg().commandLine;
}

TEST_F(Multiplayer, ThePortIsWrittenAndPushed) {
    on("GZDoom");

    panels().setNetPort("5030");

    EXPECT_EQ(mp().port, "5030");
    EXPECT_EQ(cfg().netPort, "5030");
}

TEST_F(Multiplayer, TheFragLimitIsWrittenAndPushed) {
    on("GZDoom");

    panels().setFragLimit("20");

    EXPECT_EQ(mp().fragLimit, "20");
    EXPECT_EQ(cfg().fragLimit, "20");
}

TEST_F(Multiplayer, TheTimeLimitIsWrittenAndPushed) {
    on("GZDoom");

    panels().setTimeLimit("10");

    EXPECT_EQ(mp().timeLimit, "10");
    EXPECT_EQ(cfg().timeLimit, "10");
}

TEST_F(Multiplayer, ExtraticIsWrittenAndPushed) {
    on("GZDoom");

    panels().setExtratic(true);

    EXPECT_TRUE(mp().extratic);
    EXPECT_TRUE(cfg().extratic);
}

TEST_F(Multiplayer, TheNetmodeIsWrittenAndPushed) {
    on("GZDoom");

    panels().setNetmode(1);

    EXPECT_EQ(mp().netmode, 1);
    EXPECT_EQ(cfg().netmode, 1);
}

TEST_F(Multiplayer, DupIsWrittenAndPushed) {
    on("GZDoom");

    panels().setDup(2);

    EXPECT_EQ(mp().dup, 2);
    EXPECT_EQ(cfg().dup, 2);
}

TEST_F(Multiplayer, ListingIsWrittenAndPushed) {
    on("Chocolate");

    panels().setListed(true);

    EXPECT_TRUE(mp().listed);
    EXPECT_TRUE(cfg().listed);
}

TEST_F(Multiplayer, TheSavegameIsWrittenAndPushed) {
    on("GZDoom");

    panels().setSavegame("net.zds");

    EXPECT_EQ(mp().savegame, "net.zds");
    EXPECT_EQ(cfg().savegame, "net.zds");
}

TEST_F(Multiplayer, EveryFieldIsSaved) {
    on("GZDoom");

    panels().setHost("10.0.0.7");
    panels().setNetPort("5030");
    panels().setFragLimit("20");
    panels().setTimeLimit("10");
    panels().setExtratic(true);
    panels().setNetmode(1);
    panels().setDup(2);
    panels().setListed(true);
    panels().setSavegame("net.zds");
    settle();

    const MultiplayerSettings got = saved().activeProfile().multiplayer;

    EXPECT_EQ(got, mp());
}

TEST_F(Multiplayer, AnyFieldSetMarksThePanelSet) {
    on("GZDoom");

    panels().setFragLimit("20");

    EXPECT_TRUE(cfg().multiplayerSet);
}

TEST_F(Multiplayer, ClearingPutsTheDefaultsBack) {
    on("GZDoom");

    panels().setNetRole(NetRole::Host);
    panels().setHost("10.0.0.7");
    panels().clearMultiplayer();

    EXPECT_EQ(mp(), MultiplayerSettings());
    EXPECT_FALSE(cfg().multiplayerSet);
    EXPECT_EQ(cfg().profileCards.front().netRole, NetRole::Alone);
}

TEST_F(Multiplayer, OpeningThePanelIsRemembered) {
    on("GZDoom");

    panels().setMultiplayerOpen(true);

    EXPECT_TRUE(config().activeProfile().on(Switch::dialogOpen));
    EXPECT_TRUE(cfg().multiplayerOpen);

    settle();

    EXPECT_TRUE(saved().activeProfile().on(Switch::dialogOpen));
}

class Replay : public ProfilePanelsTest {
protected:
    // Two demos and a file that is not one, two.lmp the newer.
    static void recorded() {
        model::writeAged(replays() / "one.lmp", 60);
        model::writeAged(replays() / "two.lmp", 10);
        model::writeAged(replays() / "notes.txt", 5);
    }
};

TEST_F(Replay, ThePortSaysWhatItCanDo) {
    on("PrBoom");

    EXPECT_TRUE(cfg().replayRecords);
    EXPECT_TRUE(cfg().replayTimed);
    EXPECT_TRUE(cfg().replayFast);
    EXPECT_TRUE(cfg().replayHasComplevel);
    EXPECT_TRUE(cfg().replayHasLongtics);
    EXPECT_TRUE(cfg().replayHasSoloNet);
}

TEST_F(Replay, TheZdoomFamilyRecordsWithoutTheBoomExtras) {
    on("GZDoom");

    EXPECT_TRUE(cfg().replayRecords);
    EXPECT_TRUE(cfg().replayTimed);
    EXPECT_FALSE(cfg().replayFast);
    EXPECT_FALSE(cfg().replayHasComplevel);
    EXPECT_FALSE(cfg().replayHasLongtics);
}

TEST_F(Replay, TheDemosAreListedNewestFirst) {
    recorded();
    on("GZDoom");

    EXPECT_EQ(cfg().replayFiles, (std::vector<std::string>{"two.lmp", "one.lmp"}));
    EXPECT_EQ(cfg().replayFolder, ttk::Format::from_path(replays()));
}

TEST_F(Replay, ADemoRecordedSinceIsListedOnARefresh) {
    on("GZDoom");

    EXPECT_TRUE(cfg().replayFiles.empty());

    recorded();
    panels().refreshReplays();

    EXPECT_EQ(cfg().replayFiles.size(), 2U);
}

TEST_F(Replay, TheListingIsNotReadAgainUnasked) {
    on("GZDoom");

    recorded();
    panels().setReplayLongtics(true);

    EXPECT_TRUE(cfg().replayFiles.empty());
}

TEST_F(Replay, PlayingPicksTheNewestDemoWhenNoneIsChosen) {
    recorded();
    on("GZDoom");

    panels().setReplayMode(ReplayMode::Play);

    EXPECT_EQ(demo().file, "two.lmp");
    EXPECT_EQ(cfg().replayFile, "two.lmp");
    EXPECT_EQ(cfg().replayIndex, 0);
}

TEST_F(Replay, ContinueTakesThePlayedDemoAsItsSource) {
    recorded();
    on("Woof");

    panels().setReplayMode(ReplayMode::Play);
    panels().setReplayMode(ReplayMode::Continue);

    EXPECT_EQ(demo().source, "two.lmp");
    EXPECT_EQ(demo().file, "");
    EXPECT_TRUE(cfg().replayContinues);
    EXPECT_EQ(cfg().replaySourceIndex, 0);

    panels().setReplayMode(ReplayMode::Play);

    EXPECT_EQ(demo().file, "two.lmp");
}

TEST_F(Replay, ContinuePicksTheNewestDemoWhenNoneIsChosen) {
    recorded();
    on("Woof");

    panels().setReplayFile("next");
    panels().setReplayMode(ReplayMode::Continue);

    EXPECT_EQ(demo().source, "two.lmp");
    EXPECT_EQ(demo().file, "next");
}

TEST_F(Replay, PlayingKeepsTheDemoChosen) {
    recorded();
    on("GZDoom");

    panels().setReplayFile("one.lmp");
    panels().setReplayMode(ReplayMode::Play);

    EXPECT_EQ(demo().file, "one.lmp");
    EXPECT_EQ(cfg().replayIndex, 1);
}

TEST_F(Replay, PlayingReadsTheFolderAgain) {
    on("GZDoom");

    recorded();
    panels().setReplayMode(ReplayMode::Play);

    EXPECT_EQ(cfg().replayFiles.size(), 2U);
}

TEST_F(Replay, RecordingPicksNoDemo) {
    recorded();
    on("GZDoom");

    panels().setReplayMode(ReplayMode::Record);

    EXPECT_EQ(demo().file, "");
    EXPECT_EQ(cfg().replayMode, ReplayMode::Record);
}

TEST_F(Replay, TheSameModeAgainChangesNothing) {
    on("GZDoom");

    const int before = cfg().rev;

    panels().setReplayMode(ReplayMode::Off);

    EXPECT_EQ(cfg().rev, before);
}

TEST_F(Replay, TheModeShowsOnTheCard) {
    on("GZDoom");

    panels().setReplayMode(ReplayMode::Record);

    EXPECT_EQ(cfg().profileCards.front().replayMode, ReplayMode::Record);
}

TEST_F(Replay, TheModeIsSaved) {
    on("GZDoom");

    panels().setReplayMode(ReplayMode::Record);
    settle();

    EXPECT_EQ(saved().activeProfile().replay.mode, ReplayMode::Record);
}

TEST_F(Replay, ADemoInTheFolderIsFoundInTheList) {
    recorded();
    on("GZDoom");

    panels().setReplayFile("one.lmp");

    EXPECT_EQ(cfg().replayIndex, 1);
    EXPECT_EQ(cfg().replayPath, ttk::Format::from_path(replays() / "one.lmp"));
}

TEST_F(Replay, ADemoNamedWithoutItsExtensionIsStillFound) {
    recorded();
    on("GZDoom");

    panels().setReplayFile("one");

    EXPECT_EQ(cfg().replayIndex, 1);
}

TEST_F(Replay, ADemoOutsideTheFolderIsNotInTheList) {
    recorded();
    on("GZDoom");

    panels().setReplayFile((folder() / "one.lmp").string());

    EXPECT_EQ(cfg().replayIndex, -1);
}

TEST_F(Replay, PickingFromTheListChoosesThatDemo) {
    recorded();
    on("GZDoom");

    panels().setReplayIndex(1);

    EXPECT_EQ(demo().file, "one.lmp");
}

TEST_F(Replay, PickingPastTheListChoosesNone) {
    recorded();
    on("GZDoom");

    panels().setReplayIndex(1);
    panels().setReplayIndex(2);

    EXPECT_EQ(demo().file, "");
}

TEST_F(Replay, RecordingOverADemoThatIsThereSaysTheNameIsTaken) {
    recorded();
    on("GZDoom");

    panels().setReplayMode(ReplayMode::Record);
    panels().setReplayFile("two");

    EXPECT_TRUE(cfg().replayNameTaken);
}

TEST_F(Replay, RecordingUnderANewNameIsNotTaken) {
    recorded();
    on("GZDoom");

    panels().setReplayMode(ReplayMode::Record);
    panels().setReplayFile("three");

    EXPECT_FALSE(cfg().replayNameTaken);
}

TEST_F(Replay, PlayingADemoThatIsThereIsNeverTaken) {
    recorded();
    on("GZDoom");

    panels().setReplayMode(ReplayMode::Play);

    EXPECT_FALSE(cfg().replayNameTaken);
}

TEST_F(Replay, PlayingADemoThatIsGoneIsTrouble) {
    on("GZDoom");

    panels().setReplayFile("gone");
    panels().setReplayMode(ReplayMode::Play);

    EXPECT_TRUE(cfg().replayTrouble.starts_with("There is no demo at ")) << cfg().replayTrouble;
}

TEST_F(Replay, PlaybackIsWrittenPushedAndSaved) {
    on("PrBoom");

    panels().setReplayPlayback(Playback::Fast);

    EXPECT_EQ(demo().playback, Playback::Fast);
    EXPECT_EQ(cfg().replayPlayback, Playback::Fast);

    settle();

    EXPECT_EQ(saved().activeProfile().replay.playback, Playback::Fast);
}

TEST_F(Replay, LongticsAreWrittenPushedAndSaved) {
    on("PrBoom");

    panels().setReplayLongtics(true);

    EXPECT_TRUE(demo().longtics);
    EXPECT_TRUE(cfg().replayLongtics);

    settle();

    EXPECT_TRUE(saved().activeProfile().replay.longtics);
}

TEST_F(Replay, SoloNetIsWrittenPushedAndSaved) {
    on("PrBoom");

    panels().setReplaySoloNet(true);

    EXPECT_TRUE(demo().soloNet);
    EXPECT_TRUE(cfg().replaySoloNet);

    settle();

    EXPECT_TRUE(saved().activeProfile().replay.soloNet);
}

TEST_F(Replay, RecordingWithLongticsReachesTheCommandLine) {
    on("PrBoom");

    panels().setReplayMode(ReplayMode::Record);
    panels().setReplayFile("run");
    panels().setReplayLongtics(true);
    preview();

    EXPECT_TRUE(cfg().commandLine.contains("-longtics")) << cfg().commandLine;
    EXPECT_TRUE(cfg().commandLine.contains("-record")) << cfg().commandLine;
}

TEST_F(Replay, AnySettingMarksThePanelSet) {
    on("GZDoom");

    EXPECT_FALSE(cfg().replaySet);

    panels().setReplayLongtics(true);

    EXPECT_TRUE(cfg().replaySet);
}

TEST_F(Replay, ClearingPutsTheDefaultsBack) {
    recorded();
    on("GZDoom");

    panels().setReplayMode(ReplayMode::Play);
    panels().clearReplay();

    EXPECT_EQ(demo(), ReplaySettings());
    EXPECT_FALSE(cfg().replaySet);
    EXPECT_EQ(cfg().profileCards.front().replayMode, ReplayMode::Off);
}

TEST_F(Replay, OpeningThePanelIsRemembered) {
    on("GZDoom");

    panels().setReplayOpen(true);

    EXPECT_TRUE(config().activeProfile().on(Switch::replayOpen));
    EXPECT_TRUE(cfg().replayOpen);
}

class Gameplay : public ProfilePanelsTest {
protected:
    static Profile &active() { return config().activeProfile(); }

    static const State::FlagField &field(const Flags::Word word) { return cfg().flagFields[static_cast<size_t>(word)]; }

    // Alpha alone on Helion or Zandronum, which the shared world does not carry.
    void onNew(const std::string &name, const std::string &program) const {
        Config made = model::world(1);

        made.ports.push_back(model::port(name, "/ports/" + program));
        made.profiles.front().port = name;
        install(made);
    }
};

TEST_F(Gameplay, PrBoomOffersItsComplevelsAfterThePortsOwn) {
    on("PrBoom");

    EXPECT_EQ(cfg().compatibility, State::Compatibility::complevels);
    ASSERT_EQ(cfg().compatibilityNames.size(), 19U);
    EXPECT_EQ(cfg().compatibilityNames.front(), "The port's own");
    EXPECT_EQ(cfg().compatibilityNames[3], "Doom v1.9");
    EXPECT_EQ(cfg().compatibilityNames.back(), "PrBoom, current");
    EXPECT_EQ(cfg().compatibilityBadges.front(), "");
    EXPECT_EQ(cfg().compatibilityBadges[3], "2");
}

TEST_F(Gameplay, DsdaAddsMbf21) {
    on("DSDA");

    ASSERT_EQ(cfg().compatibilityNames.size(), 20U);
    EXPECT_EQ(cfg().compatibilityNames.back(), "MBF21");
    EXPECT_EQ(cfg().compatibilityBadges.back(), "21");
}

TEST_F(Gameplay, HelionShowsTheNameItTakes) {
    onNew("Helion", "helion");

    EXPECT_EQ(cfg().compatibilityNames,
              (std::vector<std::string>{"The port's own", "Doom v1.9", "Boom v2.02", "MBF", "MBF21"}));
    EXPECT_EQ(cfg().compatibilityBadges, (std::vector<std::string>{"", "vanilla", "boom", "mbf", "mbf21"}));
}

TEST_F(Gameplay, WoofOffersItsOwnShortList) {
    on("Woof");

    EXPECT_EQ(cfg().compatibilityBadges, (std::vector<std::string>{"", "2", "3", "4", "9", "11", "21", "24"}));
    EXPECT_EQ(cfg().compatibilityNames.back(), "id24");
}

TEST_F(Gameplay, AComplevelPickedIsWrittenAsItsNumber) {
    on("Woof");

    panels().setCompatibility(4);

    EXPECT_EQ(active().complevel, 9);
    EXPECT_EQ(cfg().compatibilityIndex, 4);
    EXPECT_EQ(cfg().complevelName, "Boom v2.02 · 9");
}

TEST_F(Gameplay, ThePortsOwnComplevelIsMinusOne) {
    on("Woof");

    panels().setCompatibility(4);
    panels().setCompatibility(0);

    EXPECT_EQ(active().complevel, -1);
    EXPECT_EQ(cfg().compatibilityIndex, 0);
    EXPECT_EQ(cfg().complevelName, "");
}

TEST_F(Gameplay, AComplevelPastTheListIsThePortsOwn) {
    on("Woof");

    panels().setCompatibility(40);

    EXPECT_EQ(active().complevel, -1);
}

TEST_F(Gameplay, AComplevelThePortDoesNotOfferShowsAsThePortsOwn) {
    Config made = model::world(1);

    made.profiles.front().port = "Woof";
    made.profiles.front().complevel = 5;
    install(made);

    EXPECT_EQ(cfg().compatibilityIndex, 0);
    EXPECT_EQ(cfg().complevelName, "");
}

TEST_F(Gameplay, TheComplevelIsSaved) {
    on("PrBoom");

    panels().setCompatibility(3);
    settle();

    EXPECT_EQ(saved().activeProfile().complevel, 2);
}

TEST_F(Gameplay, GzdoomOffersItsCompatibilityModes) {
    on("GZDoom");

    EXPECT_EQ(cfg().compatibility, State::Compatibility::modes);
    ASSERT_EQ(cfg().compatibilityNames.size(), 11U);
    EXPECT_EQ(cfg().compatibilityNames.front(), "The port's own");
    EXPECT_EQ(cfg().compatibilityNames[1], "Default");
    EXPECT_EQ(cfg().compatibilityNames.back(), "ZDoom 2.0.63");
    EXPECT_TRUE(cfg().compatibilityBadges.empty());
}

TEST_F(Gameplay, AModeFillsBothCompatibilityWords) {
    on("GZDoom");

    // Doom, after the port's own and Default.
    panels().setCompatibility(2);

    EXPECT_EQ(active().flag(Flags::Word::compatflags), "2820160259");
    EXPECT_EQ(active().flag(Flags::Word::compatflags2), "16642");
    EXPECT_EQ(cfg().compatibilityIndex, 2);
    EXPECT_EQ(field(Flags::Word::compatflags).text, "2820160259");
}

TEST_F(Gameplay, UzdoomModesSetItsOwnSecondWord) {
    on("UZDoom");

    panels().setCompatibility(2);

    EXPECT_EQ(active().flag(Flags::Word::compatflags2), "1048834");
}

TEST_F(Gameplay, WordsThatMatchNoModeAreCustom) {
    on("GZDoom");

    panels().setCompatibility(2);
    panels().setFlag(Flags::Word::compatflags, "1");

    EXPECT_EQ(cfg().compatibilityIndex, -1);

    panels().setFlag(Flags::Word::compatflags, "2820160259");

    EXPECT_EQ(cfg().compatibilityIndex, 2);

    panels().setFlag(Flags::Word::compatflags2, "");

    EXPECT_EQ(cfg().compatibilityIndex, -1) << "an empty word beside a typed one";
}

TEST_F(Gameplay, ThePortsOwnModeEmptiesBothWords) {
    on("GZDoom");

    panels().setCompatibility(3);
    panels().setCompatibility(0);

    EXPECT_EQ(active().flags, Profile().flags);
    EXPECT_EQ(cfg().compatibilityIndex, 0);
}

TEST_F(Gameplay, OnlyTheWordsThePortReadsAreShown) {
    on("GZDoom");

    EXPECT_TRUE(field(Flags::Word::dmflags3).shown);
    EXPECT_TRUE(field(Flags::Word::compatflags2).shown);
    EXPECT_FALSE(field(Flags::Word::zadmflags).shown);
    EXPECT_TRUE(cfg().compatibilityKept);

    on("PrBoom");

    for (const State::FlagField &each : cfg().flagFields) {
        EXPECT_FALSE(each.shown);
    }

    EXPECT_FALSE(cfg().compatibilityKept);
}

TEST_F(Gameplay, ZandronumShowsItsOwnWordsAndKeepsNone) {
    onNew("Zandronum", "zandronum");

    EXPECT_TRUE(field(Flags::Word::zadmflags).shown);
    EXPECT_TRUE(field(Flags::Word::lmsWeapons).shown);
    EXPECT_TRUE(field(Flags::Word::zacompatflags).shown);
    EXPECT_FALSE(field(Flags::Word::dmflags3).shown);
    EXPECT_FALSE(cfg().compatibilityKept);
    EXPECT_EQ(cfg().compatibilityNames.size(), 8U);
}

TEST_F(Gameplay, AWordThatIsNotANumberIsMarked) {
    on("GZDoom");

    panels().setFlag(Flags::Word::dmflags, "12a");

    EXPECT_TRUE(field(Flags::Word::dmflags).bad);
    EXPECT_EQ(field(Flags::Word::dmflags).text, "12a");

    panels().setFlag(Flags::Word::dmflags, "12");

    EXPECT_FALSE(field(Flags::Word::dmflags).bad);
}

TEST_F(Gameplay, SeveralWordsAreSetAtOnceAndSaved) {
    on("GZDoom");

    panels().setFlags({{Flags::Word::dmflags, "4096"}, {Flags::Word::dmflags2, "64"}, {Flags::Word::dmflags3, ""}});
    settle();

    EXPECT_EQ(saved().activeProfile().flag(Flags::Word::dmflags), "4096");
    EXPECT_EQ(saved().activeProfile().flag(Flags::Word::dmflags2), "64");
    EXPECT_EQ(field(Flags::Word::dmflags2).text, "64");
    EXPECT_EQ(field(Flags::Word::dmflags3).text, "");
}

TEST_F(Gameplay, RecordingOnDsdaWithoutAComplevelIsFlagged) {
    on("DSDA");

    panels().setReplayMode(ReplayMode::Record);

    EXPECT_FALSE(cfg().complevelNeeded) << "nothing is recorded without a name";

    panels().setReplayFile("run");

    EXPECT_TRUE(cfg().complevelNeeded);

    panels().setCompatibility(19);

    EXPECT_FALSE(cfg().complevelNeeded);
    EXPECT_EQ(cfg().complevelName, "MBF21 · 21");
}

TEST_F(Gameplay, WoofDoesNotRecordAtId24) {
    on("Woof");

    panels().setReplayMode(ReplayMode::Record);
    panels().setReplayFile("run");
    panels().setCompatibility(7);

    EXPECT_EQ(active().complevel, 24);
    EXPECT_TRUE(cfg().complevelNeeded);
    EXPECT_EQ(cfg().complevelName, "id24 · 24");

    panels().setCompatibility(6);

    EXPECT_FALSE(cfg().complevelNeeded);
}

TEST_F(Gameplay, PrBoomRecordsWithoutAComplevel) {
    on("PrBoom");

    panels().setReplayMode(ReplayMode::Record);

    EXPECT_FALSE(cfg().complevelNeeded);
}

TEST_F(Gameplay, TheRangeOfANumberFollowsThePort) {
    on("DSDA");

    const State::RuleField &turbo = cfg().ruleFields[Rules::index(Rules::Rule::turbo)];

    EXPECT_TRUE(turbo.shown);
    EXPECT_EQ(turbo.least, 10);
    EXPECT_EQ(turbo.most, 255);

    panels().setRule(Rules::Rule::turbo, "300");

    EXPECT_TRUE(cfg().ruleFields[Rules::index(Rules::Rule::turbo)].bad);

    on("Woof");

    const State::RuleField &woof = cfg().ruleFields[Rules::index(Rules::Rule::turbo)];

    EXPECT_EQ(woof.least, 10);
    EXPECT_EQ(woof.most, 400);
    EXPECT_FALSE(cfg().ruleFields[Rules::index(Rules::Rule::dogs)].bad);
}

TEST_F(Gameplay, ChocolateNamesItsGameVersions) {
    on("Chocolate");

    EXPECT_EQ(cfg().compatibility, State::Compatibility::versions);
    EXPECT_EQ(cfg().compatibilityNames.size(), 12U);
    EXPECT_EQ(cfg().compatibilityNames[6], "Doom 1.9");
    EXPECT_EQ(cfg().compatibilityBadges[6], "1.9");

    panels().setCompatibility(6);

    EXPECT_EQ(cfg().compatibility, State::Compatibility::versions);
    EXPECT_EQ(cfg().complevelName, "Doom 1.9 · 1.9");
}

TEST_F(Gameplay, OpeningThePanelIsRemembered) {
    on("GZDoom");

    panels().setGameplayOpen(true);

    EXPECT_TRUE(cfg().gameplayOpen);

    settle();

    EXPECT_TRUE(saved().activeProfile().on(Switch::gameplayOpen));
}

class Saves : public ProfilePanelsTest {
protected:
    static void written(const std::vector<std::string> &names) {
        int age = 100;

        for (const std::string &name : names) {
            model::writeAged(saves() / name, age);

            age -= 10;
        }
    }
};

TEST_F(Saves, TheZdoomFamilyLoadsByName) {
    on("GZDoom", true);

    EXPECT_TRUE(cfg().saveLoads);
    EXPECT_FALSE(cfg().saveSlots);
}

TEST_F(Saves, TheBoomFamilyLoadsBySlot) {
    on("PrBoom", true);

    EXPECT_TRUE(cfg().saveLoads);
    EXPECT_TRUE(cfg().saveSlots);
}

TEST_F(Saves, DsdaLoadsBySlot) {
    on("DSDA", true);

    EXPECT_TRUE(cfg().saveLoads);
    EXPECT_TRUE(cfg().saveSlots);
}

TEST_F(Saves, APortWithNoSaveFolderCannotLoadOne) {
    on("Vanilla", true);

    EXPECT_FALSE(cfg().saveLoads);
}

TEST_F(Saves, TheSavesAreListedNewestFirstByThePortsExtension) {
    written({"old.zds", "other.dsg", "new.zds"});
    on("GZDoom", true);

    EXPECT_EQ(cfg().saveFiles, (std::vector<std::string>{"new.zds", "old.zds"}));
    EXPECT_EQ(cfg().saveFolder, ttk::Format::from_path(saves()));
}

TEST_F(Saves, AnotherPortListsItsOwnSavesWithoutBeingAsked) {
    written({"old.zds", "prboom-plus-savegame3.dsg"});
    on("GZDoom", true);

    bridge().profile().setPort("PrBoom");

    EXPECT_EQ(cfg().saveFiles, (std::vector<std::string>{"prboom-plus-savegame3.dsg"}));
}

TEST_F(Saves, ASlotPortListsOnlyWhatItsSlotsLoadEachWithItsSlot) {
    written({"prboom-plus-savegame3.dsg", "quick.dsg", "prbmsav4.dsg"});
    on("PrBoom", true);

    EXPECT_EQ(cfg().saveFiles, (std::vector<std::string>{"prboom-plus-savegame3.dsg"}));
    EXPECT_EQ(cfg().saveSlotLabels, (std::vector<std::string>{"Slot 3"}));
}

TEST_F(Saves, AWoofPortLabelsItsAutosaveTheWayThePortDoes) {
    written({"woofsav3.dsg", "autosave.dsg"});
    on("Woof", true);

    EXPECT_EQ(cfg().saveFiles, (std::vector<std::string>{"autosave.dsg", "woofsav3.dsg"}));
    EXPECT_EQ(cfg().saveSlotLabels, (std::vector<std::string>{"Auto Save", "Slot 3"}));
}

TEST_F(Saves, ANamePortHasNoSlotLabels) {
    written({"save7.zds"});
    on("GZDoom", true);

    EXPECT_EQ(cfg().saveSlotLabels, (std::vector<std::string>{""}));
}

TEST_F(Saves, WithoutAConfigOfItsOwnTheSavesAreThePorts) {
    const fs::path root = ttk::Paths::home_directory() / ".config" / "gzdoom" / "savegames";
    std::error_code code;

    fs::remove_all(root, code);
    written({"save1.zds"});
    model::write(root / "doom.id.doom2.commercial" / "save00.zds",
                 ports::zip({{.name = "info.json", .bytes = R"({"Game WAD": "DOOM2.WAD"})"}}));
    model::write(root / "doom.id.doom2.plutonia" / "save00.zds",
                 ports::zip({{.name = "info.json", .bytes = R"({"Game WAD": "PLUTONIA.WAD"})"}}));

    on("GZDoom", false);

    EXPECT_TRUE(cfg().savesFromPort);
    EXPECT_EQ(cfg().saveFolder, ttk::Format::from_path(root));
    EXPECT_EQ(cfg().saveFiles, (std::vector<std::string>{"doom.id.doom2.commercial/save00.zds"}));

    bridge().profile().setIwad("Plutonia");

    EXPECT_EQ(cfg().saveFiles, (std::vector<std::string>{"doom.id.doom2.plutonia/save00.zds"}));

    panels().setSaveIndex(0);

    EXPECT_EQ(cfg().saveIndex, 0);
    EXPECT_EQ(config().activeProfile().save.file, "doom.id.doom2.plutonia/save00.zds");

    fs::remove_all(root, code);
}

TEST_F(Saves, APortZdl4CannotPlaceHasNoSaveFolder) {
    Config made = model::world(1);

    made.ports.front().file = "/ports/mystery-engine";
    made.general.profileConfigs = false;
    install(made);

    EXPECT_TRUE(cfg().saveFiles.empty());
    EXPECT_EQ(cfg().saveFolder, "");
}

TEST_F(Saves, LoadingPicksTheNewestSaveWhenNoneIsChosen) {
    written({"old.zds", "new.zds"});
    on("GZDoom", true);

    panels().setSaveEnabled(true);

    EXPECT_EQ(kept().file, "new.zds");
    EXPECT_TRUE(cfg().saveEnabled);
    EXPECT_EQ(cfg().saveIndex, 0);
}

TEST_F(Saves, LoadingKeepsTheSaveChosen) {
    written({"old.zds", "new.zds"});
    on("GZDoom", true);

    kept().file = "old.zds";
    panels().setSaveEnabled(true);

    EXPECT_EQ(kept().file, "old.zds");
    EXPECT_EQ(cfg().saveIndex, 1);
}

TEST_F(Saves, LoadingReadsTheFolderAgain) {
    on("GZDoom", true);

    written({"late.zds"});
    panels().setSaveEnabled(true);

    EXPECT_EQ(cfg().saveFiles, (std::vector<std::string>{"late.zds"}));
}

TEST_F(Saves, TheSameChoiceAgainChangesNothing) {
    on("GZDoom", true);

    const int before = cfg().rev;

    panels().setSaveEnabled(false);

    EXPECT_EQ(cfg().rev, before);
}

TEST_F(Saves, PickingFromTheListChoosesThatSave) {
    written({"old.zds", "new.zds"});
    on("GZDoom", true);

    panels().setSaveIndex(1);

    EXPECT_EQ(kept().file, "old.zds");
    EXPECT_EQ(cfg().saveIndex, 1);
    EXPECT_EQ(cfg().savePath, ttk::Format::from_path(saves() / "old.zds"));
}

TEST_F(Saves, PickingPastTheListChoosesNone) {
    written({"old.zds"});
    on("GZDoom", true);

    panels().setSaveIndex(3);

    EXPECT_EQ(kept().file, "");
    EXPECT_EQ(cfg().saveIndex, -1);
}

TEST_F(Saves, ASaveRemovedSinceIsDroppedOnARefresh) {
    written({"old.zds", "new.zds"});
    on("GZDoom", true);

    fs::remove(saves() / "old.zds");
    panels().refreshSaves();

    EXPECT_EQ(cfg().saveFiles, (std::vector<std::string>{"new.zds"}));
}

TEST_F(Saves, LoadingASaveThatIsGoneIsTrouble) {
    written({"old.zds"});
    on("GZDoom", true);

    panels().setSaveEnabled(true);
    fs::remove(saves() / "old.zds");
    panels().refreshSaves();

    EXPECT_TRUE(cfg().saveTrouble.starts_with("There is no save at ")) << cfg().saveTrouble;
}

TEST_F(Saves, MovingToThePortsSettingsPicksFromThePortsSaves) {
    const fs::path root = ttk::Paths::home_directory() / ".config" / "gzdoom" / "savegames";
    std::error_code code;

    fs::remove_all(root, code);
    written({"mine.zds"});
    model::write(root / "doom.id.doom2.commercial" / "save00.zds",
                 ports::zip({{.name = "info.json", .bytes = R"({"Game WAD": "DOOM2.WAD"})"}}));
    on("GZDoom", true);
    panels().setSaveEnabled(true);

    ASSERT_EQ(config().activeProfile().save.file, "mine.zds");

    bridge().profile().setSharedConfig(true);

    EXPECT_EQ(config().activeProfile().save.file, "doom.id.doom2.commercial/save00.zds");
    EXPECT_EQ(cfg().saveTrouble, "");
    EXPECT_EQ(cfg().saveIndex, 0);

    bridge().profile().setSharedConfig(false);

    EXPECT_EQ(config().activeProfile().save.file, "mine.zds");
    EXPECT_EQ(cfg().saveTrouble, "");

    fs::remove_all(root, code);
}

TEST_F(Saves, MovingWithLoadingOffDropsThePick) {
    written({"mine.zds"});
    on("GZDoom", true);
    panels().setSaveIndex(0);
    panels().setSaveEnabled(false);

    ASSERT_EQ(config().activeProfile().save.file, "mine.zds");

    bridge().profile().setSharedConfig(true);

    EXPECT_EQ(config().activeProfile().save.file, "");
    EXPECT_EQ(cfg().saveTrouble, "");
}

TEST_F(Saves, AFullPathPickStaysWhenTheListMoves) {
    written({"mine.zds"});
    on("GZDoom", true);
    panels().setSaveEnabled(true);

    const std::string full = (saves() / "mine.zds").string();

    config().activeProfile().save.file = full;
    bridge().profile().setSharedConfig(true);

    EXPECT_EQ(config().activeProfile().save.file, full);
}

TEST_F(Saves, AnAddOnWithMapsMovesWoofSavesAtOnce) {
    Config made = model::world(1);
    const fs::path games = tests::Sandbox::scratch("woof-panel");

    // One map: its marker and the THINGS lump that makes it one.
    const auto wad = [](const char *kind) {
        return std::string(kind) + std::string("\x02\0\0\0\x0c\0\0\0", 8) + std::string(8, '\0')
               + std::string("MAP01\0\0\0", 8) + std::string(8, '\0') + std::string("THINGS\0\0", 8);
    };

    model::write(games / "DOOM2.WAD", wad("IWAD"));
    model::write(games / "maps.wad", wad("PWAD"));

    made.iwads.front().file = (games / "DOOM2.WAD").string();
    made.profiles.front().port = "Woof";
    made.general.profileConfigs = false;
    install(made);

    const std::string before = cfg().saveFolder;

    ASSERT_TRUE(before.ends_with("/savegames/DOOM2.WAD")) << before;

    bridge().lists().addFiles({(games / "maps.wad").string()});

    EXPECT_NE(cfg().saveFolder, before);
    EXPECT_TRUE(cfg().saveFolder.ends_with("/savegames/maps.wad")) << cfg().saveFolder;
}

TEST_F(Saves, AnotherPortRepicksFromItsOwnSaves) {
    written({"mine.zds"});
    on("GZDoom", false);
    config().activeProfile().save = {.enabled = true, .file = "gone/from-gzdoom.zds"};
    panels().refreshSaves();

    bridge().profile().setPort("Woof");

    EXPECT_EQ(config().activeProfile().save.file, "");
    EXPECT_EQ(cfg().saveTrouble, "");
}

TEST_F(Saves, AnotherProfileKeepsAPickThatIsGone) {
    Config made = model::world(2);

    made.general.profileConfigs = true;
    made.profiles[1].save = {.enabled = true, .file = "gone.zds"};
    install(made);

    bridge().profile().setProfileIndex(1);

    EXPECT_EQ(config().activeProfile().save.file, "gone.zds");
    EXPECT_TRUE(cfg().saveTrouble.starts_with("There is no save at ")) << cfg().saveTrouble;
}

TEST_F(Saves, ASlotPortCannotLoadASaveWithNoNumber) {
    written({"quick.dsg"});
    on("PrBoom", true);

    config().activeProfile().save.file = "quick.dsg";
    panels().setSaveEnabled(true);

    EXPECT_TRUE(cfg().saveTrouble.contains("no number in quick.dsg")) << cfg().saveTrouble;
}

TEST_F(Saves, LoadingASaveReachesTheCommandLine) {
    written({"game.zds"});
    on("GZDoom", true);

    panels().setSaveEnabled(true);
    preview();

    EXPECT_TRUE(cfg().commandLine.contains("-loadgame")) << cfg().commandLine;
}

TEST_F(Saves, TheChoiceIsSaved) {
    written({"game.zds"});
    on("GZDoom", true);

    panels().setSaveEnabled(true);
    settle();

    EXPECT_TRUE(saved().activeProfile().save.enabled);
    EXPECT_EQ(saved().activeProfile().save.file, "game.zds");
}

TEST_F(Saves, OpeningThePanelIsRemembered) {
    on("GZDoom", true);

    panels().setSaveOpen(true);

    EXPECT_TRUE(config().activeProfile().on(Switch::saveOpen));
    EXPECT_TRUE(cfg().saveOpen);
}

}
