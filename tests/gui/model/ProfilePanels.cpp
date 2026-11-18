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
#include <filesystem>
#include <span>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Paths.h"
#include "ttk/util/Format.h"

#include "Model.h"
#include "core/config/Config.h"
#include "core/config/Options.h"
#include "core/launch/Dialect.h"
#include "core/launch/Flags.h"
#include "core/ports/Detect.h"
#include "core/ports/Zip.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/ProfilePanels.h"
#include "gui/state/State.h"
#include "support/FlatpakApp.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

class ProfilePanelsTest : public model::Bridged {
protected:
    ProfilePanels &panels() const { return bridge().panels(); }

    static MultiplayerSettings &mp() { return config().activeProfile().multiplayer; }

    static ReplaySettings &demo() { return config().activeProfile().replay; }

    static SaveSettings &kept() { return config().activeProfile().save; }

    // Alpha alone, on `engine`.
    void on(const std::string &engine, const bool profileConfigs = false) const {
        Config made = model::world(1);

        made.profiles.front().engine = engine;
        made.general.profileConfigs = profileConfigs;
        install(made);
    }

    // Alpha alone on `engine`, which is a release of its port.
    void onRelease(const std::string &engine, const std::string &version) const {
        Config made = model::world(1);

        made.profiles.front().engine = engine;
        made.general.profileConfigs = true;
        std::ranges::find(made.engines, engine, &Engine::name)->version = version;
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

TEST_F(Multiplayer, TheZdoomFamilyDoesEverythingButHostListed) {
    on("GZDoom");

    EXPECT_TRUE(cfg().features.has(Dialect::Feature::hosts));
    EXPECT_TRUE(cfg().features.has(Dialect::Feature::joins));
    EXPECT_TRUE(cfg().features.has(Dialect::Feature::hostPlayers));
    EXPECT_TRUE(cfg().features.has(Dialect::Feature::fragLimit));
    EXPECT_TRUE(cfg().features.has(Dialect::Feature::savegame));
    EXPECT_TRUE(cfg().features.has(Dialect::Feature::extratic));
    EXPECT_TRUE(cfg().features.has(Dialect::Feature::netmode));
    EXPECT_TRUE(cfg().features.has(Dialect::Feature::dup));
    EXPECT_FALSE(cfg().features.has(Dialect::Feature::hostListed));
}

TEST_F(Multiplayer, ARoleChangeRefreshesTheReplayReason) {
    Config made = model::world(1);

    made.engines.push_back(model::engine("Zandronum", "/ports/zandronum"));
    made.profiles.front().engine = "Zandronum";
    made.profiles.front().replay = {.mode = ReplayMode::Record, .file = "run1"};
    made.general.profileConfigs = true;
    install(made);

    ASSERT_EQ(cfg().replayLeftOut, Arguments::ReplayLeftOut::none);
    EXPECT_FALSE(cfg().serverHosted);

    panels().setNetRole(NetRole::Host);

    EXPECT_EQ(cfg().replayLeftOut, Arguments::ReplayLeftOut::server);
    EXPECT_TRUE(cfg().serverHosted);

    panels().setNetRole(NetRole::Alone);

    EXPECT_EQ(cfg().replayLeftOut, Arguments::ReplayLeftOut::none);
    EXPECT_FALSE(cfg().serverHosted);
}

TEST_F(Multiplayer, UzdoomHasNoNetmode) {
    on("UZDoom");

    EXPECT_TRUE(cfg().features.has(Dialect::Feature::hosts));
    EXPECT_FALSE(cfg().features.has(Dialect::Feature::netmode));
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

TEST_F(Multiplayer, TheSaveExtensionFollowsThePort) {
    on("GZDoom");

    EXPECT_EQ(cfg().netSaveExt, ".zds");

    on("Woof");

    EXPECT_EQ(cfg().netSaveExt, ".dsg");
}

TEST_F(Multiplayer, TheChocolateFamilyHostsJoinsListsAndDuplicates) {
    on("Chocolate");

    EXPECT_TRUE(cfg().features.has(Dialect::Feature::hosts));
    EXPECT_TRUE(cfg().features.has(Dialect::Feature::joins));
    EXPECT_TRUE(cfg().features.has(Dialect::Feature::hostListed));
    EXPECT_TRUE(cfg().features.has(Dialect::Feature::dup));
    EXPECT_FALSE(cfg().features.has(Dialect::Feature::hostPlayers));
    EXPECT_FALSE(cfg().features.has(Dialect::Feature::fragLimit));
    EXPECT_FALSE(cfg().features.has(Dialect::Feature::savegame));
    EXPECT_FALSE(cfg().features.has(Dialect::Feature::extratic));
    EXPECT_FALSE(cfg().features.has(Dialect::Feature::netmode));
}

TEST_F(Multiplayer, WoofPlaysTheChocolateWay) {
    on("Woof");

    EXPECT_TRUE(cfg().features.has(Dialect::Feature::hosts));
    EXPECT_TRUE(cfg().features.has(Dialect::Feature::hostListed));
    EXPECT_FALSE(cfg().features.has(Dialect::Feature::hostPlayers));
}

TEST_F(Multiplayer, PrBoomOnlyJoins) {
    on("PrBoom");

    EXPECT_TRUE(cfg().features.has(Dialect::Feature::joins));
    EXPECT_FALSE(cfg().features.has(Dialect::Feature::hosts));
    EXPECT_FALSE(cfg().features.has(Dialect::Feature::hostListed));
    EXPECT_FALSE(cfg().features.has(Dialect::Feature::dup));
}

TEST_F(Multiplayer, DsdaHasNoNetgame) {
    on("DSDA");

    EXPECT_FALSE(cfg().features.has(Dialect::Feature::hosts));
    EXPECT_FALSE(cfg().features.has(Dialect::Feature::joins));
    EXPECT_FALSE(cfg().features.has(Dialect::Feature::dup));
}

TEST_F(Multiplayer, NothingUnderDosboxPlaysOverTheNet) {
    on("Vanilla");

    EXPECT_FALSE(cfg().features.has(Dialect::Feature::hosts));
    EXPECT_FALSE(cfg().features.has(Dialect::Feature::joins));
}

TEST_F(Multiplayer, NoPortIsTakenForTheZdoomFamily) {
    on("");

    EXPECT_TRUE(cfg().features.has(Dialect::Feature::hosts));
    EXPECT_TRUE(cfg().features.has(Dialect::Feature::hostPlayers));
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

    EXPECT_TRUE(cfg().features.has(Dialect::Feature::replays));
    EXPECT_TRUE(cfg().features.has(Dialect::Feature::timedemo));
    EXPECT_TRUE(cfg().features.has(Dialect::Feature::fastdemo));
    EXPECT_TRUE(cfg().replayHasComplevel);
    EXPECT_TRUE(cfg().features.has(Dialect::Feature::longtics));
    EXPECT_TRUE(cfg().features.has(Dialect::Feature::soloNet));
}

TEST_F(Replay, TheZdoomFamilyRecordsWithoutTheBoomExtras) {
    on("GZDoom");

    EXPECT_TRUE(cfg().features.has(Dialect::Feature::replays));
    EXPECT_TRUE(cfg().features.has(Dialect::Feature::timedemo));
    EXPECT_FALSE(cfg().features.has(Dialect::Feature::fastdemo));
    EXPECT_FALSE(cfg().replayHasComplevel);
    EXPECT_FALSE(cfg().features.has(Dialect::Feature::longtics));
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
    EXPECT_TRUE(cfg().features.has(Dialect::Feature::continues));
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
    void onNew(const std::string &name, const std::string &file) const {
        Config made = model::world(1);

        made.engines.push_back(model::engine(name, "/ports/" + file));
        made.profiles.front().engine = name;
        install(made);
    }
};

TEST_F(Gameplay, TheListIsWhatCoreOffersForThePort) {
    for (const char *name : {"PrBoom", "DSDA", "Woof", "Chocolate"}) {
        on(name);

        const Dialect::Traits traits = Dialect::of(config());
        const std::span<const int> levels = Dialect::complevels(traits.complevel);

        EXPECT_EQ(cfg().compatibility, Dialect::compatibility(traits)) << name;
        ASSERT_EQ(cfg().compatibilityNames.size(), levels.size()) << name;
        ASSERT_EQ(cfg().compatibilityBadges.size(), levels.size()) << name;

        for (size_t at = 0; at < levels.size(); ++at) {
            EXPECT_EQ(cfg().compatibilityNames[at], Dialect::levelName(levels[at])) << name << " " << at;
            EXPECT_EQ(cfg().compatibilityBadges[at], Dialect::complevelWord(traits.complevel, levels[at]))
                    << name << " " << at;
        }
    }
}

TEST_F(Gameplay, HelionListsWhatItTakes) {
    onNew("Helion", "helion");

    const std::span<const int> levels = Dialect::complevels(Dialect::Complevels::helion);

    EXPECT_EQ(cfg().compatibility, Dialect::Compatibility::complevels);
    ASSERT_EQ(cfg().compatibilityNames.size(), levels.size());
    EXPECT_EQ(cfg().compatibilityBadges.back(), Dialect::complevelWord(Dialect::Complevels::helion, levels.back()));
}

TEST_F(Gameplay, AComplevelPickedIsWrittenAsItsNumber) {
    on("Woof");

    panels().setCompatibility(4);

    EXPECT_EQ(active().complevel, 9);
    EXPECT_EQ(cfg().compatibilityIndex, 4);
    EXPECT_FALSE(cfg().complevelName.empty());
    EXPECT_EQ(cfg().complevelName, Dialect::levelLabel(Dialect::Complevels::woof, 9));
}

TEST_F(Gameplay, ThePortsOwnComplevelIsMinusOne) {
    on("Woof");

    panels().setCompatibility(4);
    panels().setCompatibility(0);

    EXPECT_EQ(active().complevel, -1);
    EXPECT_EQ(cfg().compatibilityIndex, 0);
    EXPECT_TRUE(cfg().complevelName.empty());
}

TEST_F(Gameplay, AComplevelPastTheListIsThePortsOwn) {
    on("Woof");

    panels().setCompatibility(40);

    EXPECT_EQ(active().complevel, -1);
}

TEST_F(Gameplay, AComplevelThePortDoesNotOfferShowsAsThePortsOwn) {
    Config made = model::world(1);

    made.profiles.front().engine = "Woof";
    made.profiles.front().complevel = 5;
    install(made);

    EXPECT_EQ(cfg().compatibilityIndex, 0);
    EXPECT_TRUE(cfg().complevelName.empty());
}

TEST_F(Gameplay, TheComplevelIsSaved) {
    on("PrBoom");

    panels().setCompatibility(3);
    settle();

    EXPECT_EQ(saved().activeProfile().complevel, 2);
}

TEST_F(Gameplay, GzdoomOffersItsCompatibilityModes) {
    on("GZDoom");

    const std::span<const Flags::Mode> modes = Flags::modes(Dialect::of(config()).line);

    EXPECT_EQ(cfg().compatibility, Dialect::Compatibility::modes);
    ASSERT_EQ(cfg().compatibilityNames.size(), modes.size() + 1);
    EXPECT_EQ(cfg().compatibilityNames.front(), Dialect::levelName(-1));
    EXPECT_EQ(cfg().compatibilityNames[1], modes.front().label);
    EXPECT_EQ(cfg().compatibilityNames.back(), modes.back().label);
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

    EXPECT_TRUE(cfg().features.has(Flags::feature(Flags::Word::dmflags3)));
    EXPECT_TRUE(cfg().features.has(Flags::feature(Flags::Word::compatflags2)));
    EXPECT_FALSE(cfg().features.has(Flags::feature(Flags::Word::zadmflags)));
    EXPECT_TRUE(cfg().features.has(Dialect::Feature::keepsCompatflags));

    on("PrBoom");

    for (const Flags::Word each : Flags::ALL) {
        EXPECT_FALSE(cfg().features.has(Flags::feature(each)));
    }

    EXPECT_FALSE(cfg().features.has(Dialect::Feature::keepsCompatflags));
}

TEST_F(Gameplay, ZandronumShowsItsOwnWordsAndKeepsNone) {
    onNew("Zandronum", "zandronum");

    EXPECT_TRUE(cfg().features.has(Flags::feature(Flags::Word::zadmflags)));
    EXPECT_TRUE(cfg().features.has(Flags::feature(Flags::Word::lmsWeapons)));
    EXPECT_TRUE(cfg().features.has(Flags::feature(Flags::Word::zacompatflags)));
    EXPECT_FALSE(cfg().features.has(Flags::feature(Flags::Word::dmflags3)));
    EXPECT_FALSE(cfg().features.has(Dialect::Feature::keepsCompatflags));
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

TEST_F(Gameplay, RecordingWithoutAComplevelIsFlaggedUntilOneIsPicked) {
    on("DSDA");

    panels().setReplayMode(ReplayMode::Record);

    EXPECT_FALSE(cfg().complevelNeeded);

    panels().setReplayFile("run");

    EXPECT_TRUE(cfg().complevelNeeded);

    panels().setCompatibility(19);

    EXPECT_FALSE(cfg().complevelNeeded);
    EXPECT_EQ(cfg().complevelName, Dialect::levelLabel(Dialect::Complevels::dsda, 21));
}

TEST_F(Gameplay, AnUnrecordableLevelIsFlaggedByName) {
    on("Woof");

    panels().setReplayMode(ReplayMode::Record);
    panels().setReplayFile("run");
    panels().setCompatibility(7);

    EXPECT_EQ(active().complevel, Dialect::ID24);
    EXPECT_TRUE(cfg().complevelNeeded);
    EXPECT_EQ(cfg().complevelName, Dialect::levelLabel(Dialect::Complevels::woof, Dialect::ID24));
}

TEST_F(Gameplay, RecordingWithoutASkillIsFlaggedUntilOneIsPicked) {
    on("DSDA");

    panels().setReplayMode(ReplayMode::Record);

    EXPECT_FALSE(cfg().skillNeeded);

    panels().setReplayFile("run");

    EXPECT_TRUE(cfg().skillNeeded);

    active().skill = 4;
    panels().setReplayFile("run");

    EXPECT_FALSE(cfg().skillNeeded);
}

TEST_F(Gameplay, TheRangeOfANumberFollowsThePort) {
    on("DSDA");

    const State::OptionField &turbo = cfg().optionFields[Options::indexOf(Dialect::Feature::turbo)];

    EXPECT_TRUE(cfg().features.has(Dialect::Feature::turbo));
    EXPECT_EQ(turbo.least, 10);
    EXPECT_EQ(turbo.most, 255);

    panels().setNumber(Dialect::Feature::turbo, "300");

    EXPECT_TRUE(cfg().optionFields[Options::indexOf(Dialect::Feature::turbo)].bad);

    on("Woof");

    const State::OptionField &woof = cfg().optionFields[Options::indexOf(Dialect::Feature::turbo)];

    EXPECT_EQ(woof.least, 10);
    EXPECT_EQ(woof.most, 400);
    EXPECT_FALSE(cfg().optionFields[Options::indexOf(Dialect::Feature::dogs)].bad);
}

TEST_F(Gameplay, ARuleTheReleaseHasNoSwitchForIsNotOffered) {
    for (const char *version : {"0.25.0", "0.22.3", "0.21.1"}) {
        onRelease("DSDA", version);

        EXPECT_FALSE(cfg().features.has(Dialect::Feature::chainEpisodes)) << version;
        EXPECT_FALSE(cfg().features.has(Dialect::Feature::gameSpeed)) << version;
        EXPECT_TRUE(cfg().features.has(Dialect::Feature::turbo)) << version;
    }

    for (const char *version : {"0.26.0", "0.30.0", ""}) {
        onRelease("DSDA", version);

        EXPECT_TRUE(cfg().features.has(Dialect::Feature::chainEpisodes)) << version;
        EXPECT_TRUE(cfg().features.has(Dialect::Feature::gameSpeed)) << version;
    }
}

TEST_F(Gameplay, ARuleLeftOutOnAnOldReleaseComesBackWithItsValue) {
    onRelease("DSDA", "0.25.0");
    panels().setOption(Dialect::Feature::chainEpisodes, true);
    panels().setNumber(Dialect::Feature::gameSpeed, "100");

    EXPECT_FALSE(cfg().features.has(Dialect::Feature::gameSpeed));

    std::ranges::find(config().engines, "DSDA", &Engine::name)->version = "0.30.0";
    panels().pushGameplay();

    const State::OptionField &chain = cfg().optionFields[Options::indexOf(Dialect::Feature::chainEpisodes)];
    const State::OptionField &speed = cfg().optionFields[Options::indexOf(Dialect::Feature::gameSpeed)];

    EXPECT_TRUE(cfg().features.has(Dialect::Feature::chainEpisodes));
    EXPECT_TRUE(chain.on);
    EXPECT_TRUE(cfg().features.has(Dialect::Feature::gameSpeed));
    EXPECT_EQ(speed.text, "100");
}

TEST_F(Gameplay, PistolStartIsOfferedFrom0_21_2) {
    onRelease("DSDA", "0.21.2");

    EXPECT_TRUE(cfg().features.has(Dialect::Feature::pistolStart));

    onRelease("DSDA", "0.21.1");

    EXPECT_FALSE(cfg().features.has(Dialect::Feature::pistolStart));
}

TEST_F(Gameplay, ChocolatePicksAGameVersionByItsPlace) {
    on("Chocolate");

    EXPECT_EQ(cfg().compatibility, Dialect::Compatibility::versions);

    panels().setCompatibility(6);

    EXPECT_EQ(active().complevel, Dialect::complevels(Dialect::Complevels::gameVersion)[6]);
    EXPECT_EQ(cfg().complevelName, Dialect::levelLabel(Dialect::Complevels::gameVersion, active().complevel));
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

TEST_F(Saves, DsdaBefore0_30_0LoadsNoSaveAndListsNone) {
    onRelease("DSDA", "0.30.0");
    written({"dsda-doom-savegame3.dsg"});
    panels().refreshSaves();

    EXPECT_TRUE(cfg().saveLoads);
    EXPECT_EQ(cfg().saveFiles.size(), 1U);

    onRelease("DSDA", "0.28.0");
    written({"dsda-doom-savegame3.dsg"});
    panels().refreshSaves();

    EXPECT_FALSE(cfg().saveLoads);
    EXPECT_FALSE(cfg().saveSlots);
    EXPECT_TRUE(cfg().saveFiles.empty());
    EXPECT_EQ(cfg().saveTrouble, "");
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

    bridge().profile().setEngine("PrBoom");

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

TEST_F(Saves, AnOlderReleaseWithoutAnAutosaveStopsListingIt) {
    written({"woofsav3.dsg", "autosave.dsg"});
    on("Woof", true);

    ASSERT_EQ(cfg().saveFiles.size(), 2U);

    bridge().lists().updateEngine(4, {.name = "Woof", .file = "/ports/woof", .version = "10.2.0"});

    EXPECT_EQ(cfg().saveFiles, (std::vector<std::string>{"woofsav3.dsg"}));
}

#ifdef __linux__
TEST_F(Saves, ARefreshedFlatpakVersionListsWhatTheNewReleaseLoads) {
    const std::string app = "io.github.fabiangreffrath.woof";
    const fs::path root = tests::Sandbox::scratch("saves-flatpak") / "flatpak";
    const tests::FlatpakApp deployed = tests::deployFlatpak(root, app);

    tests::nameRelease(deployed, "10.2.0");
    written({"woofsav3.dsg", "autosave.dsg"});

    Config made = model::world(1);

    made.engines.push_back({.name = "Woof Flatpak", .file = deployed.exported.string(), .version = "10.2.0"});
    made.profiles.front().engine = "Woof Flatpak";
    made.general.profileConfigs = true;
    install(made);

    ASSERT_EQ(cfg().saveFiles, (std::vector<std::string>{"woofsav3.dsg"}));

    tests::nameRelease(tests::deployFlatpak(root, app, "stable", "4567cdef"), "15.0.0");
    panels().pushSave();

    ASSERT_EQ(cfg().saveFiles, (std::vector<std::string>{"woofsav3.dsg"}));

    bridge().lists().storeVersions(Detect::installedVersions(config().engines));

    EXPECT_EQ(cfg().saveFiles, (std::vector<std::string>{"autosave.dsg", "woofsav3.dsg"}));
}
#endif

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

    made.engines.front().file = "/ports/mystery-engine";
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
    made.profiles.front().engine = "Woof";
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

    bridge().profile().setEngine("Woof");

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
