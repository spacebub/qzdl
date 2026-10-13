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
#include <vector>

#include <gtest/gtest.h>

#include "ttk/util/Format.h"

#include "Model.h"
#include "core/config/Config.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/ProfilePanels.h"
#include "gui/state/State.h"

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
    EXPECT_TRUE(cfg().netFlags);
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

TEST_F(Multiplayer, TheChocolateFamilyHostsJoinsListsAndDuplicates) {
    on("Chocolate");

    EXPECT_TRUE(cfg().netHosts);
    EXPECT_TRUE(cfg().netJoins);
    EXPECT_TRUE(cfg().netListing);
    EXPECT_TRUE(cfg().netDup);
    EXPECT_FALSE(cfg().netPlayers);
    EXPECT_FALSE(cfg().netFragLimit);
    EXPECT_FALSE(cfg().netFlags);
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

TEST_F(Multiplayer, TheFlagsAreWrittenAndPushed) {
    on("GZDoom");

    panels().setDmflags("1024");
    panels().setDmflags2("2048");

    EXPECT_EQ(mp().dmflags, "1024");
    EXPECT_EQ(mp().dmflags2, "2048");
    EXPECT_EQ(cfg().dmflags, "1024");
    EXPECT_EQ(cfg().dmflags2, "2048");
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
    panels().setDmflags("1");
    panels().setDmflags2("2");
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

    EXPECT_TRUE(config().activeProfile().dialogOpen);
    EXPECT_TRUE(cfg().multiplayerOpen);

    settle();

    EXPECT_TRUE(saved().activeProfile().dialogOpen);
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
    EXPECT_TRUE(cfg().replayComplevels.empty());
}

TEST_F(Replay, PrBoomOffersItsComplevelsAfterThePortsOwn) {
    on("PrBoom");

    ASSERT_EQ(cfg().replayComplevels.size(), 19U);
    EXPECT_EQ(cfg().replayComplevels.front(), "The port's own");
    EXPECT_EQ(cfg().replayComplevels[3], "Doom v1.9");
    EXPECT_EQ(cfg().replayComplevels.back(), "PrBoom, current");
    EXPECT_EQ(cfg().replayComplevelNumbers.front(), "");
    EXPECT_EQ(cfg().replayComplevelNumbers[3], "2");
}

TEST_F(Replay, DsdaAddsMbf21) {
    on("DSDA");

    ASSERT_EQ(cfg().replayComplevels.size(), 20U);
    EXPECT_EQ(cfg().replayComplevels.back(), "MBF21");
    EXPECT_EQ(cfg().replayComplevelNumbers.back(), "21");
}

TEST_F(Replay, WoofOffersItsOwnShortList) {
    on("Woof");

    EXPECT_EQ(cfg().replayComplevelNumbers, (std::vector<std::string>{"", "2", "3", "4", "9", "11", "21", "24"}));
    EXPECT_EQ(cfg().replayComplevels.back(), "id24");
}

TEST_F(Replay, AComplevelPickedIsWrittenAsItsNumber) {
    on("Woof");

    panels().setReplayComplevel(4);

    EXPECT_EQ(demo().compatibility, 9);
    EXPECT_EQ(cfg().replayComplevel, 4);
}

TEST_F(Replay, ThePortsOwnComplevelIsMinusOne) {
    on("Woof");

    panels().setReplayComplevel(4);
    panels().setReplayComplevel(0);

    EXPECT_EQ(demo().compatibility, -1);
    EXPECT_EQ(cfg().replayComplevel, 0);
}

TEST_F(Replay, AComplevelPastTheListIsThePortsOwn) {
    on("Woof");

    panels().setReplayComplevel(40);

    EXPECT_EQ(demo().compatibility, -1);
}

TEST_F(Replay, AComplevelThePortDoesNotOfferShowsAsThePortsOwn) {
    Config made = model::world(1);

    made.profiles.front().port = "Woof";
    made.profiles.front().replay.compatibility = 5;
    install(made);

    EXPECT_EQ(cfg().replayComplevel, 0);
}

TEST_F(Replay, TheComplevelIsSaved) {
    on("PrBoom");

    panels().setReplayComplevel(3);
    settle();

    EXPECT_EQ(saved().activeProfile().replay.compatibility, 2);
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

    EXPECT_TRUE(config().activeProfile().replayOpen);
    EXPECT_TRUE(cfg().replayOpen);
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

TEST_F(Saves, DsdaCannotLoadASave) {
    on("DSDA", true);

    EXPECT_FALSE(cfg().saveLoads);
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
    written({"old.zds", "prbmsav3.dsg"});
    on("GZDoom", true);

    bridge().profile().setPort("PrBoom");

    EXPECT_EQ(cfg().saveFiles, (std::vector<std::string>{"prbmsav3.dsg"}));
}

TEST_F(Saves, ASlotPortLabelsEachSaveWithItsSlot) {
    written({"prbmsav3.dsg", "quick.dsg"});
    on("PrBoom", true);

    EXPECT_EQ(cfg().saveFiles, (std::vector<std::string>{"quick.dsg", "prbmsav3.dsg"}));
    EXPECT_EQ(cfg().saveSlotLabels, (std::vector<std::string>{"", "Slot 3"}));
}

TEST_F(Saves, ANamePortHasNoSlotLabels) {
    written({"save7.zds"});
    on("GZDoom", true);

    EXPECT_EQ(cfg().saveSlotLabels, (std::vector<std::string>{""}));
}

TEST_F(Saves, WithoutAConfigOfItsOwnThereIsNoSaveFolder) {
    written({"save1.zds"});
    on("GZDoom", false);

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

TEST_F(Saves, ASlotPortCannotLoadASaveWithNoNumber) {
    written({"quick.dsg"});
    on("PrBoom", true);

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

    EXPECT_TRUE(config().activeProfile().saveOpen);
    EXPECT_TRUE(cfg().saveOpen);
}

}
