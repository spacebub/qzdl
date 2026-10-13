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
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/notices/Notice.h"
#include "ttk/util/Format.h"

#include "Model.h"
#include "core/config/Config.h"
#include "core/config/Schema.h"
#include "core/launch/Storage.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/ProfileBridge.h"
#include "gui/state/State.h"
#include "support/Corpus.h"
#include "support/Fixtures.h"

namespace {

namespace fs = std::filesystem;

class ProfileBridgeTest : public model::Bridged {
protected:
    ProfileBridge &profile() const { return bridge().profile(); }

    static std::vector<std::string> profileNames() {
        std::vector<std::string> out;

        for (const Profile &each : config().profiles) {
            out.push_back(each.name);
        }

        return out;
    }

    static std::vector<std::string> cardNames() {
        std::vector<std::string> out;

        for (const State::ProfileCard &card : cfg().profileCards) {
            out.push_back(card.name);
        }

        return out;
    }

    static fs::path portConfig(const std::string &name) {
        return model::ownFolder(name) / (ttk::Text::lower(name) + ".cfg");
    }

    static State::ProfileCard plainCard() {
        State::ProfileCard card;

        card.ready = true;

        return card;
    }

    static std::vector<std::string> badgeTexts(const State::ProfileCard &card) {
        std::vector<std::string> out;

        for (const State::BadgeSpec &badge : ProfileBridge::badgesOf(card)) {
            out.push_back(badge.text);
        }

        return out;
    }
};

TEST_F(ProfileBridgeTest, PushCopiesTheActiveProfile) {
    Config made = model::world(2);
    Profile &bravo = made.profiles[1];

    bravo.skill = 3;
    bravo.monsters = 2;
    bravo.warp = "MAP05";
    bravo.extra = "-nomusic";
    bravo.port = "Chocolate";
    bravo.iwad = "Plutonia";
    made.activeProfileId = bravo.id;
    install(made);

    EXPECT_EQ(cfg().profileIndex, 1);
    EXPECT_EQ(cfg().profileName, "Bravo");
    EXPECT_EQ(cfg().profileKey, "profile:id-bravo");
    EXPECT_EQ(cfg().iwad, "Plutonia");
    EXPECT_EQ(cfg().port, "Chocolate");
    EXPECT_EQ(cfg().skill, 3);
    EXPECT_EQ(cfg().monsters, 2);
    EXPECT_EQ(cfg().warp, "MAP05");
    EXPECT_EQ(cfg().extra, "-nomusic");
}

TEST_F(ProfileBridgeTest, NoProfilesIsIndexMinusOne) {
    install(model::world(0));

    EXPECT_EQ(cfg().profileIndex, -1);
    EXPECT_TRUE(cfg().profileCards.empty());
}

TEST_F(ProfileBridgeTest, TheProfileFolderIsTheOneItsConfigNames) {
    install(model::world(1));

    EXPECT_EQ(cfg().profileDirectory, ttk::Format::from_path(model::ownFolder("Alpha")));
}

TEST_F(ProfileBridgeTest, ThereIsACardPerProfileInOrder) {
    install(model::world(3));

    EXPECT_EQ(cardNames(), (std::vector<std::string>{"Alpha", "Bravo", "Charlie"}));
    EXPECT_EQ(cfg().profileCards[2].index, 2);
    EXPECT_EQ(cfg().profileCards[2].id, "id-charlie");
    EXPECT_EQ(cfg().profileCards[2].key, "profile:id-charlie");
}

TEST_F(ProfileBridgeTest, AProfileWithNoNameIsShownUnnamed) {
    Config made = model::world(1);

    made.profiles.front().name.clear();
    install(made);

    EXPECT_EQ(cfg().profileCards.front().name, "(unnamed)");
}

TEST_F(ProfileBridgeTest, ACardCountsTheFilesAndTheOnesLoaded) {
    Config made = model::world(1);

    made.profiles.front().files = {{.file = "/a.wad", .enabled = true},
                                   {.file = "/b.wad", .enabled = false},
                                   {.file = "/c.wad", .enabled = true}};
    install(made);

    EXPECT_EQ(cfg().profileCards.front().files, 3);
    EXPECT_EQ(cfg().profileCards.front().loaded, 2);
}

TEST_F(ProfileBridgeTest, ACardIsReadyWithAPort) {
    Config made = model::world(2);

    made.profiles[1].port.clear();
    install(made);

    EXPECT_TRUE(cfg().profileCards[0].ready);
    EXPECT_FALSE(cfg().profileCards[1].ready);
}

TEST_F(ProfileBridgeTest, ACardWithACustomCommandIsReadyWithoutAPort) {
    Config made = model::world(1);

    made.profiles.front().port.clear();
    made.profiles.front().customCommand = true;
    install(made);

    EXPECT_TRUE(cfg().profileCards.front().ready);
}

TEST_F(ProfileBridgeTest, ACardOnADosPortSaysSo) {
    Config made = model::world(2);

    made.profiles[1].port = "Vanilla";
    install(made);

    EXPECT_FALSE(cfg().profileCards[0].dosPort);
    EXPECT_TRUE(cfg().profileCards[1].dosPort);
}

TEST_F(ProfileBridgeTest, TheArtKeyIsTheGameAndTheFilesLoaded) {
    Config made = model::world(1);

    made.profiles.front().files = {{.file = "/a.wad", .enabled = true}, {.file = "/b.wad", .enabled = false}};
    install(made);

    EXPECT_EQ(cfg().profileCards.front().artKey, "/games/doom2.wad\n/a.wad");
    EXPECT_EQ(ProfileBridge::artKey(), "/games/doom2.wad\n/a.wad");
}

TEST_F(ProfileBridgeTest, AHostCardIsSupportedOnlyByAPortThatHosts) {
    Config made = model::world(3);

    for (Profile &each : made.profiles) {
        each.multiplayer.gameType = GameType::Coop;
        each.multiplayer.players = 2;
    }

    made.profiles[1].port = "Chocolate";
    made.profiles[2].port = "PrBoom";
    install(made);

    EXPECT_EQ(cfg().profileCards[0].netRole, NetRole::Host);
    EXPECT_TRUE(cfg().profileCards[0].netSupported);
    EXPECT_TRUE(cfg().profileCards[1].netSupported);
    EXPECT_FALSE(cfg().profileCards[2].netSupported);
}

TEST_F(ProfileBridgeTest, AJoinCardIsSupportedByAPortThatOnlyJoins) {
    Config made = model::world(1);

    made.profiles.front().port = "PrBoom";
    made.profiles.front().multiplayer.gameType = GameType::Coop;
    install(made);

    EXPECT_EQ(cfg().profileCards.front().netRole, NetRole::Join);
    EXPECT_TRUE(cfg().profileCards.front().netSupported);
}

TEST_F(ProfileBridgeTest, ASoloCardIsNeverNetSupported) {
    install(model::world(1));

    EXPECT_EQ(cfg().profileCards.front().netRole, NetRole::Alone);
    EXPECT_FALSE(cfg().profileCards.front().netSupported);
}

TEST_F(ProfileBridgeTest, AReadyCardWithNoFilesHasNoBadges) {
    EXPECT_TRUE(ProfileBridge::badgesOf(plainCard()).empty());
}

TEST_F(ProfileBridgeTest, ACardWithNoPortIsBadgedAsAWarning) {
    State::ProfileCard card = plainCard();

    card.ready = false;
    card.files = 3;
    card.loaded = 3;

    const std::vector<State::BadgeSpec> badges = ProfileBridge::badgesOf(card);

    ASSERT_EQ(badges.size(), 1U);
    EXPECT_EQ(badges.front(), (State::BadgeSpec{.text = "No port", .kind = State::BadgeKind::Warning, .dot = true}));
}

TEST_F(ProfileBridgeTest, AllFilesLoadedAreCounted) {
    State::ProfileCard card = plainCard();

    card.files = 3;
    card.loaded = 3;

    EXPECT_EQ(badgeTexts(card), (std::vector<std::string>{"3 files"}));

    card.files = 1;
    card.loaded = 1;

    EXPECT_EQ(badgeTexts(card), (std::vector<std::string>{"1 file"}));
}

TEST_F(ProfileBridgeTest, SomeFilesLoadedAreCountedOfTheWhole) {
    State::ProfileCard card = plainCard();

    card.files = 4;
    card.loaded = 1;

    EXPECT_EQ(badgeTexts(card), (std::vector<std::string>{"1 of 4 loaded"}));
}

TEST_F(ProfileBridgeTest, TheDosBadgeComesFirstAndIsMuted) {
    State::ProfileCard card = plainCard();

    card.dosPort = true;
    card.files = 2;
    card.loaded = 2;

    const std::vector<State::BadgeSpec> badges = ProfileBridge::badgesOf(card);

    ASSERT_EQ(badges.size(), 2U);
    EXPECT_EQ(badges.front(), (State::BadgeSpec{.text = "DOS", .kind = State::BadgeKind::Muted, .dot = true}));
}

TEST_F(ProfileBridgeTest, ASupportedNetgameIsBadgedBySide) {
    State::ProfileCard card = plainCard();

    card.netSupported = true;
    card.netRole = NetRole::Host;

    EXPECT_EQ(badgeTexts(card), (std::vector<std::string>{"Hosting"}));

    card.netRole = NetRole::Join;

    EXPECT_EQ(badgeTexts(card), (std::vector<std::string>{"Multiplayer"}));
}

TEST_F(ProfileBridgeTest, AnUnsupportedNetgameIsNotBadged) {
    State::ProfileCard card = plainCard();

    card.netRole = NetRole::Host;

    EXPECT_TRUE(ProfileBridge::badgesOf(card).empty());
}

TEST_F(ProfileBridgeTest, AReplayIsBadgedByWhatItDoes) {
    State::ProfileCard card = plainCard();

    card.replayMode = ReplayMode::Record;

    EXPECT_EQ(badgeTexts(card), (std::vector<std::string>{"Recording"}));

    card.replayMode = ReplayMode::Play;

    EXPECT_EQ(badgeTexts(card), (std::vector<std::string>{"Replay"}));
}

TEST_F(ProfileBridgeTest, EveryBadgeComesInOrder) {
    State::ProfileCard card = plainCard();

    card.dosPort = true;
    card.files = 2;
    card.loaded = 1;
    card.netRole = NetRole::Join;
    card.netSupported = true;
    card.replayMode = ReplayMode::Play;

    EXPECT_EQ(badgeTexts(card), (std::vector<std::string>{"DOS", "1 of 2 loaded", "Multiplayer", "Replay"}));
}

TEST_F(ProfileBridgeTest, TheZdlNameIsTheProfileNameMadeSafe) {
    Config made = model::world(1);

    made.profiles.front().name = "Sigil: \"Part\" 1/2?";
    install(made);

    EXPECT_EQ(ProfileBridge::zdlFileName(), "Sigil- -Part- 1-2-.zdl");
}

TEST_F(ProfileBridgeTest, TheZdlNameDropsTrailingDotsAndSpaces) {
    Config made = model::world(1);

    made.profiles.front().name = "  Final. . ";
    install(made);

    EXPECT_EQ(ProfileBridge::zdlFileName(), "Final.zdl");
}

TEST_F(ProfileBridgeTest, AnEmptyZdlNameFallsBackToTheStem) {
    Config made = model::world(1);

    made.profiles.front().name = "...";
    install(made);

    EXPECT_EQ(ProfileBridge::zdlFileName(), std::string(ConfigFile::PROFILE_STEM) + ConfigFile::ZDL_EXT);
}

TEST_F(ProfileBridgeTest, AProfileWithAPortIsLaunchable) {
    install(model::world(1));

    EXPECT_TRUE(ProfileBridge::launchable());
}

TEST_F(ProfileBridgeTest, AProfileWithoutAPortIsNotLaunchable) {
    Config made = model::world(1);

    made.profiles.front().port.clear();
    install(made);

    EXPECT_FALSE(ProfileBridge::launchable());
}

TEST_F(ProfileBridgeTest, ACustomCommandIsLaunchableUnlessItHasTrouble) {
    install(model::world(1));

    profile().setCommandOverride(true);
    preview();

    EXPECT_TRUE(ProfileBridge::launchable());

    profile().setCommand("{nope}");
    preview();

    EXPECT_FALSE(ProfileBridge::launchable());
}

TEST_F(ProfileBridgeTest, SelectingAProfileMakesItActive) {
    install(model::world(3));

    profile().setProfileIndex(2);

    EXPECT_EQ(config().activeProfileId, "id-charlie");
    EXPECT_EQ(cfg().profileIndex, 2);
    EXPECT_EQ(cfg().profileName, "Charlie");
}

TEST_F(ProfileBridgeTest, SelectingAProfileIsSaved) {
    install(model::world(3));

    profile().setProfileIndex(1);
    settle();

    EXPECT_EQ(saved().activeProfileId, "id-bravo");
}

TEST_F(ProfileBridgeTest, SelectingTheActiveProfileSavesNothing) {
    install(model::world(3));

    profile().setProfileIndex(0);
    settle();

    EXPECT_TRUE(saved().profiles.empty());
}

TEST_F(ProfileBridgeTest, SelectingAProfileThatIsNotThereDoesNothing) {
    install(model::world(3));

    profile().setProfileIndex(3);
    profile().setProfileIndex(-1);

    EXPECT_EQ(config().activeProfileId, "id-alpha");
}

TEST_F(ProfileBridgeTest, SettingTheGameWritesAndPushesIt) {
    install(model::world(1));

    profile().setIwad("Plutonia");

    EXPECT_EQ(config().activeProfile().iwad, "Plutonia");
    EXPECT_EQ(cfg().iwad, "Plutonia");
    EXPECT_EQ(cfg().profileCards.front().iwad, "Plutonia");
    EXPECT_EQ(cfg().profileCards.front().artKey, "/games/plutonia.wad");
}

TEST_F(ProfileBridgeTest, SettingTheGameIsSaved) {
    install(model::world(1));

    profile().setIwad("Plutonia");
    settle();

    EXPECT_EQ(saved().activeProfile().iwad, "Plutonia");
}

TEST_F(ProfileBridgeTest, SettingTheSameGameMovesNothing) {
    install(model::world(1));

    const int before = cfg().rev;

    profile().setIwad("Doom II");

    EXPECT_EQ(cfg().rev, before);
}

TEST_F(ProfileBridgeTest, TheGameChangesTheLaunch) {
    install(model::world(1));

    profile().setIwad("Plutonia");
    preview();

    EXPECT_TRUE(cfg().commandLine.contains("/games/plutonia.wad")) << cfg().commandLine;
}

TEST_F(ProfileBridgeTest, TheMapsComeFromTheGame) {
    install(tests::Fixtures::grounded(1, 0));

    EXPECT_EQ(cfg().maps.size(), 32U);
}

TEST_F(ProfileBridgeTest, AGameNotOnDiskHasNoMaps) {
    install(tests::Fixtures::grounded(1, 0));

    profile().setIwad("Game 5");

    EXPECT_TRUE(cfg().maps.empty());
}

TEST_F(ProfileBridgeTest, SettingThePortWritesAndPushesIt) {
    install(model::world(1));

    profile().setPort("Chocolate");

    EXPECT_EQ(config().activeProfile().port, "Chocolate");
    EXPECT_EQ(cfg().port, "Chocolate");
    EXPECT_EQ(cfg().profileCards.front().port, "Chocolate");
}

TEST_F(ProfileBridgeTest, SettingThePortTellsTheNetPanelWhatItSupports) {
    install(model::world(1));

    EXPECT_TRUE(cfg().netPlayers);

    profile().setPort("Chocolate");

    EXPECT_FALSE(cfg().netPlayers);
    EXPECT_TRUE(cfg().netListing);
}

TEST_F(ProfileBridgeTest, SettingThePortIsSaved) {
    install(model::world(1));

    profile().setPort("Woof");
    settle();

    EXPECT_EQ(saved().activeProfile().port, "Woof");
}

TEST_F(ProfileBridgeTest, ADosPortIsFlaggedForTheProfile) {
    install(model::world(1));

    profile().setPort("Vanilla");

    EXPECT_TRUE(cfg().dosPort);
    EXPECT_TRUE(cfg().profileCards.front().dosPort);
}

TEST_F(ProfileBridgeTest, SkillIsWrittenPushedAndLaunched) {
    install(model::world(1));

    profile().setSkill(4);
    preview();

    EXPECT_EQ(config().activeProfile().skill, 4);
    EXPECT_EQ(cfg().skill, 4);
    EXPECT_TRUE(cfg().commandLine.contains("-skill 4")) << cfg().commandLine;
}

TEST_F(ProfileBridgeTest, SkillIsSaved) {
    install(model::world(1));

    profile().setSkill(4);
    settle();

    EXPECT_EQ(saved().activeProfile().skill, 4);
}

TEST_F(ProfileBridgeTest, MonstersAreWrittenPushedAndLaunched) {
    install(model::world(1));

    profile().setMonsters(1);
    preview();

    EXPECT_EQ(config().activeProfile().monsters, 1);
    EXPECT_EQ(cfg().monsters, 1);
    EXPECT_TRUE(cfg().commandLine.contains("-nomonsters")) << cfg().commandLine;
}

TEST_F(ProfileBridgeTest, TheWarpIsWrittenPushedAndLaunched) {
    install(model::world(1));

    profile().setWarp("MAP07");
    preview();

    EXPECT_EQ(config().activeProfile().warp, "MAP07");
    EXPECT_EQ(cfg().warp, "MAP07");
    EXPECT_TRUE(cfg().commandLine.contains("MAP07")) << cfg().commandLine;
}

TEST_F(ProfileBridgeTest, TheWarpIsSaved) {
    install(model::world(1));

    profile().setWarp("MAP07");
    settle();

    EXPECT_EQ(saved().activeProfile().warp, "MAP07");
}

TEST_F(ProfileBridgeTest, ExtraArgumentsAreWrittenPushedAndLaunched) {
    install(model::world(1));

    profile().setExtra("-fast -record x");
    preview();

    EXPECT_EQ(config().activeProfile().extra, "-fast -record x");
    EXPECT_EQ(cfg().extra, "-fast -record x");
    EXPECT_TRUE(cfg().commandLine.contains("-fast")) << cfg().commandLine;
}

TEST_F(ProfileBridgeTest, ASharedConfigDropsTheProfilesOwn) {
    Config made = model::world(1);

    made.general.profileConfigs = true;
    install(made);

    profile().setSharedConfig(true);
    preview();

    EXPECT_TRUE(config().activeProfile().sharedConfig);
    EXPECT_TRUE(cfg().sharedConfig);
    EXPECT_FALSE(cfg().commandLine.contains("-config")) << cfg().commandLine;
}

TEST_F(ProfileBridgeTest, ASharedConfigIsSaved) {
    install(model::world(1));

    profile().setSharedConfig(true);
    settle();

    EXPECT_TRUE(saved().activeProfile().sharedConfig);
}

TEST_F(ProfileBridgeTest, TurningTheOverrideOnStartsFromTheGeneratedLine) {
    install(model::world(1));

    profile().setCommandOverride(true);

    EXPECT_TRUE(cfg().commandOverride);
    EXPECT_TRUE(cfg().command.starts_with("{source_port}")) << cfg().command;
    EXPECT_TRUE(cfg().command.contains("{game}")) << cfg().command;
}

TEST_F(ProfileBridgeTest, TurningTheOverrideOnKeepsACommandAlreadyTyped) {
    Config made = model::world(1);

    made.profiles.front().command = "/bin/true";
    install(made);

    profile().setCommandOverride(true);

    EXPECT_EQ(cfg().command, "/bin/true");
}

TEST_F(ProfileBridgeTest, TurningTheOverrideOffKeepsTheCommand) {
    install(model::world(1));

    profile().setCommandOverride(true);
    profile().setCommand("/bin/true");
    profile().setCommandOverride(false);

    EXPECT_FALSE(cfg().commandOverride);
    EXPECT_EQ(config().activeProfile().command, "/bin/true");
}

TEST_F(ProfileBridgeTest, TheOverrideMakesACardWithNoPortReady) {
    Config made = model::world(1);

    made.profiles.front().port.clear();
    install(made);

    profile().setCommandOverride(true);

    EXPECT_TRUE(cfg().profileCards.front().ready);
}

TEST_F(ProfileBridgeTest, ATypedCommandIsWhatLaunches) {
    install(model::world(1));

    profile().setCommandOverride(true);
    profile().setCommand("/bin/echo {game}");
    preview();

    EXPECT_EQ(cfg().commandLine, "/bin/echo /games/doom2.wad");
    EXPECT_TRUE(cfg().commandTrouble.empty()) << cfg().commandTrouble;
}

TEST_F(ProfileBridgeTest, AnUnknownPlaceholderIsTrouble) {
    install(model::world(1));

    profile().setCommandOverride(true);
    profile().setCommand("/bin/echo {nope}");
    preview();

    EXPECT_TRUE(cfg().commandTrouble.contains("{nope} is not one ZDL4 knows")) << cfg().commandTrouble;
}

TEST_F(ProfileBridgeTest, AnEmptyCommandIsTrouble) {
    install(model::world(1));

    profile().setCommandOverride(true);
    profile().setCommand("");
    preview();

    EXPECT_EQ(cfg().commandTrouble, "There is nothing here to run.");
}

TEST_F(ProfileBridgeTest, ACommandIsNoTroubleWithTheOverrideOff) {
    install(model::world(1));

    profile().setCommand("{nope}");
    preview();

    EXPECT_TRUE(cfg().commandTrouble.empty());
}

TEST_F(ProfileBridgeTest, TheCommandIsSaved) {
    install(model::world(1));

    profile().setCommandOverride(true);
    profile().setCommand("/bin/true");
    settle();

    EXPECT_TRUE(saved().activeProfile().customCommand);
    EXPECT_EQ(saved().activeProfile().command, "/bin/true");
}

TEST_F(ProfileBridgeTest, ANativePortSpendsNoDosCommands) {
    install(model::world(1));
    preview();

    EXPECT_EQ(cfg().dosCommands, 0);
}

TEST_F(ProfileBridgeTest, ADosLaunchCountsTheCommandsItSpends) {
    Config made = model::world(1);
    const fs::path dosbox = folder() / "dosbox";

    model::write(dosbox);

    made.general.dosbox = dosbox.string();
    made.profiles.front().port = "Vanilla";
    install(made);
    preview();

    EXPECT_GT(cfg().dosCommands, 0);
    EXPECT_LE(cfg().dosCommands, 11);
}

TEST_F(ProfileBridgeTest, ATypedDosboxCommandIsCountedAsWritten) {
    install(model::world(1));

    profile().setCommandOverride(true);
    profile().setCommand("dosbox -c one -c two -c three");
    preview();

    EXPECT_EQ(cfg().dosCommands, 3);
}

TEST_F(ProfileBridgeTest, DosFullscreenIsWrittenPushedAndSaved) {
    install(model::world(1));

    profile().setDosFullscreen(false);

    EXPECT_FALSE(config().activeProfile().dosFullscreen);
    EXPECT_FALSE(cfg().dosFullscreen);

    settle();

    EXPECT_FALSE(saved().activeProfile().dosFullscreen);
}

TEST_F(ProfileBridgeTest, DosExitIsWrittenPushedAndSaved) {
    install(model::world(1));

    profile().setDosExit(false);

    EXPECT_FALSE(config().activeProfile().dosExit);
    EXPECT_FALSE(cfg().dosExit);

    settle();

    EXPECT_FALSE(saved().activeProfile().dosExit);
}

TEST_F(ProfileBridgeTest, CapturingOutputIsWrittenPushedAndSaved) {
    install(model::world(1));

    profile().setCaptureOutput(true);

    EXPECT_TRUE(config().activeProfile().captureOutput);
    EXPECT_TRUE(cfg().captureOutput);

    settle();

    EXPECT_TRUE(saved().activeProfile().captureOutput);
}

TEST_F(ProfileBridgeTest, LevelstatIsOfferedOnlyByAPortThatWritesOne) {
    install(model::world(1));

    EXPECT_FALSE(cfg().hasLevelstat);

    profile().setPort("PrBoom");

    EXPECT_TRUE(cfg().hasLevelstat);
}

TEST_F(ProfileBridgeTest, LevelstatIsWrittenPushedAndLaunched) {
    Config made = model::world(1);

    made.profiles.front().port = "PrBoom";
    install(made);

    profile().setLevelstat(true);
    preview();

    EXPECT_TRUE(config().activeProfile().levelstat);
    EXPECT_TRUE(cfg().levelstat);
    EXPECT_TRUE(cfg().commandLine.contains("-levelstat")) << cfg().commandLine;
}

TEST_F(ProfileBridgeTest, MovingAProfileReordersTheCardsAndKeepsTheActiveOne) {
    install(model::world(3));

    profile().moveProfile(0, 2);

    EXPECT_EQ(profileNames(), (std::vector<std::string>{"Bravo", "Charlie", "Alpha"}));
    EXPECT_EQ(cardNames(), (std::vector<std::string>{"Bravo", "Charlie", "Alpha"}));
    EXPECT_EQ(config().activeProfileId, "id-alpha");
    EXPECT_EQ(cfg().profileIndex, 2);
}

TEST_F(ProfileBridgeTest, MovingAProfileIsSaved) {
    install(model::world(3));

    profile().moveProfile(2, 0);
    settle();

    EXPECT_EQ(saved().profiles.front().name, "Charlie");
}

TEST_F(ProfileBridgeTest, AnAddedProfileBecomesActive) {
    install(model::world(2));

    profile().addProfile("Zulu");

    EXPECT_EQ(profileNames().back(), "Zulu");
    EXPECT_EQ(cfg().profileName, "Zulu");
    EXPECT_EQ(cfg().profileIndex, 2);
    EXPECT_EQ(cfg().profileCards.size(), 3U);
}

TEST_F(ProfileBridgeTest, AnAddedProfileTakesANameNoOtherHas) {
    install(model::world(2));

    profile().addProfile("alpha");

    EXPECT_EQ(cfg().profileName, "alpha (2)");
}

TEST_F(ProfileBridgeTest, AnAddedProfileWithNoNameIsANewProfile) {
    install(model::world(0));

    profile().addProfile("  ");

    EXPECT_EQ(cfg().profileName, ProfileName::UNNAMED);
}

TEST_F(ProfileBridgeTest, AnAddedProfileGetsAConfigFileOfItsOwn) {
    install(model::world(1));

    profile().addProfile("Zulu Team");

    EXPECT_EQ(config().activeProfile().config, "zulu-team.cfg");
}

TEST_F(ProfileBridgeTest, AnAddedProfileIsSaved) {
    install(model::world(1));

    profile().addProfile("Zulu");
    settle();

    ASSERT_EQ(saved().profiles.size(), 2U);
    EXPECT_EQ(saved().activeProfile().name, "Zulu");
}

TEST_F(ProfileBridgeTest, ADuplicateCopiesTheSettingsUnderANewName) {
    Config made = model::world(1);

    made.profiles.front().skill = 4;
    made.profiles.front().files = {{.file = "/a.wad", .enabled = true}};
    install(made);

    profile().duplicateProfile();

    ASSERT_EQ(config().profiles.size(), 2U);
    EXPECT_EQ(cfg().profileName, "Alpha (2)");
    EXPECT_EQ(config().activeProfile().skill, 4);
    EXPECT_EQ(config().activeProfile().files.size(), 1U);
    EXPECT_NE(config().activeProfile().id, "id-alpha");
    EXPECT_NE(config().activeProfile().config, "alpha.cfg");
}

TEST_F(ProfileBridgeTest, DuplicatingNothingDoesNothing) {
    install(model::world(0));

    profile().duplicateProfile();

    EXPECT_TRUE(config().profiles.empty());
}

TEST_F(ProfileBridgeTest, RenamingWritesTheNameEverywhere) {
    install(model::world(2));

    profile().renameProfile("Zulu");

    EXPECT_EQ(config().activeProfile().name, "Zulu");
    EXPECT_EQ(cfg().profileName, "Zulu");
    EXPECT_EQ(cardNames().front(), "Zulu");
}

TEST_F(ProfileBridgeTest, RenamingToAnEmptyNameIsIgnored) {
    install(model::world(1));

    profile().renameProfile("");

    EXPECT_EQ(config().activeProfile().name, "Alpha");
}

TEST_F(ProfileBridgeTest, RenamingOntoAnotherProfilesNameNumbersIt) {
    install(model::world(2));

    profile().renameProfile("bravo");

    EXPECT_EQ(config().activeProfile().name, "bravo (2)");
}

TEST_F(ProfileBridgeTest, RenamingToTheSameNameInAnotherCaseIsNotNumbered) {
    install(model::world(1));

    profile().renameProfile("ALPHA");

    EXPECT_EQ(config().activeProfile().name, "ALPHA");
    EXPECT_EQ(config().activeProfile().config, "alpha.cfg");
}

TEST_F(ProfileBridgeTest, RenamingToTheSameNameWithSpacesAroundIsNotNumbered) {
    install(model::world(1));

    profile().renameProfile("Alpha ");

    EXPECT_EQ(config().activeProfile().name, "Alpha");
    EXPECT_EQ(config().activeProfile().config, "alpha.cfg");
}

TEST_F(ProfileBridgeTest, RenamingTakesTheConfigFileAlong) {
    install(model::world(1));

    profile().renameProfile("Zulu");

    EXPECT_EQ(config().activeProfile().config, "zulu.cfg");
}

TEST_F(ProfileBridgeTest, RenamingMovesTheFolderItOwns) {
    install(model::world(1));

    model::write(portConfig("Alpha"), "seta x 1");

    profile().renameProfile("Zulu");

    EXPECT_FALSE(fs::exists(model::ownFolder("Alpha")));
    EXPECT_TRUE(fs::exists(model::ownFolder("Zulu") / "zulu.cfg"));
    EXPECT_EQ(cfg().profileDirectory, ttk::Format::from_path(model::ownFolder("Zulu")));
}

TEST_F(ProfileBridgeTest, RenamingIsSaved) {
    install(model::world(1));

    profile().renameProfile("Zulu");
    settle();

    EXPECT_EQ(saved().activeProfile().name, "Zulu");
    EXPECT_EQ(saved().activeProfile().config, "zulu.cfg");
}

TEST_F(ProfileBridgeTest, ARunningProfileIsNotRenamed) {
    install(model::world(1));
    hold("id-alpha");

    profile().renameProfile("Zulu");

    EXPECT_EQ(config().activeProfile().name, "Alpha");

    const std::vector<ttk::Notice> warnings = said(ttk::Severity::Warning);

    ASSERT_EQ(warnings.size(), 1U);
    EXPECT_TRUE(warnings.front().body.starts_with("Alpha is still running.")) << warnings.front().body;
}

TEST_F(ProfileBridgeTest, RemovingTheFirstProfileMakesTheNextActive) {
    install(model::world(3));

    profile().removeProfile();

    EXPECT_EQ(profileNames(), (std::vector<std::string>{"Bravo", "Charlie"}));
    EXPECT_EQ(cfg().profileName, "Bravo");
    EXPECT_EQ(cfg().profileIndex, 0);
}

TEST_F(ProfileBridgeTest, RemovingTheLastProfileMakesTheOneBeforeActive) {
    install(model::world(3));

    profile().setProfileIndex(2);
    profile().removeProfile();

    EXPECT_EQ(cfg().profileName, "Bravo");
    EXPECT_EQ(cfg().profileIndex, 1);
}

TEST_F(ProfileBridgeTest, RemovingTheOnlyProfileLeavesNone) {
    install(model::world(1));

    profile().removeProfile();

    EXPECT_TRUE(config().profiles.empty());
    EXPECT_EQ(cfg().profileIndex, -1);
    EXPECT_TRUE(cfg().profileCards.empty());
}

TEST_F(ProfileBridgeTest, RemovingAProfileDeletesTheFolderItOwns) {
    install(model::world(2));

    model::write(portConfig("Alpha"));

    profile().removeProfile();

    EXPECT_FALSE(fs::exists(model::ownFolder("Alpha")));
}

TEST_F(ProfileBridgeTest, RemovingAProfileLeavesAFolderItSharesAlone) {
    Config made = model::world(2);

    made.profiles[1].config = "alpha.cfg";
    install(made);

    model::write(portConfig("Alpha"));

    profile().removeProfile();

    EXPECT_TRUE(fs::exists(portConfig("Alpha")));
}

TEST_F(ProfileBridgeTest, RemovingAProfileIsSaved) {
    install(model::world(2));

    profile().removeProfile();
    settle();

    ASSERT_EQ(saved().profiles.size(), 1U);
    EXPECT_EQ(saved().activeProfileId, "id-bravo");
}

TEST_F(ProfileBridgeTest, ARunningProfileIsNotRemoved) {
    install(model::world(2));
    hold("id-alpha");

    profile().removeProfile();

    EXPECT_EQ(config().profiles.size(), 2U);
    EXPECT_EQ(said(ttk::Severity::Warning).size(), 1U);
}

TEST_F(ProfileBridgeTest, TheRemovalNoteWarnsOfAFolderOwned) {
    install(model::world(1));

    EXPECT_TRUE(std::string(ProfileBridge::removalNote()).contains("so does its folder"));
}

TEST_F(ProfileBridgeTest, TheRemovalNoteKeepsAFolderNotOwned) {
    Config made = model::world(1);

    made.profiles.front().config = "/elsewhere/alpha.cfg";
    install(made);

    EXPECT_EQ(std::string(ProfileBridge::removalNote()),
              "The profile goes. Its folder and the files it loaded are left alone.");
}

TEST_F(ProfileBridgeTest, ClearingKeepsTheNameAndDropsTheRest) {
    Config made = model::world(1);

    made.profiles.front().skill = 4;
    made.profiles.front().files = {{.file = "/a.wad", .enabled = true}};
    install(made);

    profile().clearProfile();

    EXPECT_EQ(cfg().profileName, "Alpha");
    EXPECT_EQ(cfg().port, "");
    EXPECT_EQ(cfg().iwad, "");
    EXPECT_EQ(cfg().skill, 0);
    EXPECT_TRUE(cfg().files.empty());
    EXPECT_EQ(config().activeProfile().config, "alpha.cfg");
}

TEST_F(ProfileBridgeTest, ClearingIsSaved) {
    install(model::world(1));

    profile().clearProfile();
    settle();

    EXPECT_EQ(saved().activeProfile().port, "");
}

TEST_F(ProfileBridgeTest, AZdlSavedIsReadBackAsANewProfile) {
    Config made = model::world(1);

    made.profiles.front().warp = "MAP09";
    install(made);

    const fs::path file = folder() / "alpha.zdl";

    profile().saveZdl(file.string());
    profile().loadZdl(file.string());

    ASSERT_EQ(config().profiles.size(), 2U);
    EXPECT_EQ(cfg().profileName, "Alpha (2)");
    EXPECT_EQ(cfg().warp, "MAP09");
    EXPECT_EQ(cfg().profileIndex, 1);
}

TEST_F(ProfileBridgeTest, SavingAZdlSaysWhere) {
    install(model::world(1));

    const fs::path file = folder() / "alpha.zdl";

    profile().saveZdl(file.string());

    const std::vector<ttk::Notice> done = said(ttk::Severity::Success);

    ASSERT_EQ(done.size(), 1U);
    EXPECT_EQ(done.front().body, "Saved Alpha to " + file.string() + ".");
}

TEST_F(ProfileBridgeTest, LoadingAZdlSaysWhatWasAdded) {
    install(model::world(1));

    const fs::path file = folder() / "alpha.zdl";

    profile().saveZdl(file.string());
    profile().loadZdl(file.string());

    const std::vector<ttk::Notice> done = said(ttk::Severity::Success);

    ASSERT_EQ(done.size(), 2U);
    EXPECT_EQ(done.back().body, "Added Alpha (2) from " + file.string() + ".");
}

TEST_F(ProfileBridgeTest, AZdlThatCannotBeReadIsReported) {
    install(model::world(1));

    const fs::path file = folder() / "missing.zdl";

    profile().loadZdl(file.string());

    EXPECT_EQ(config().profiles.size(), 1U);

    const std::vector<ttk::Notice> errors = said(ttk::Severity::Error);

    ASSERT_EQ(errors.size(), 1U);
    EXPECT_EQ(errors.front().body, "Could not read " + file.string() + " as a .zdl file.");
}

TEST_F(ProfileBridgeTest, AZdlThatCannotBeWrittenIsReported) {
    install(model::world(1));

    profile().saveZdl(folder().string());

    const std::vector<ttk::Notice> errors = said(ttk::Severity::Error);

    ASSERT_EQ(errors.size(), 1U);
    EXPECT_EQ(errors.front().body, "Could not write " + folder().string() + ".");
}

TEST_F(ProfileBridgeTest, LaunchingWithNoProfilesDoesNothing) {
    install(model::world(0));

    profile().launch();

    EXPECT_TRUE(notifier.messages().empty());
}

TEST_F(ProfileBridgeTest, LaunchingAPortThatIsNotThereFails) {
    install(model::world(1));

    profile().launch();

    EXPECT_EQ(runs().stateOf("profile:id-alpha"), State::RunState::Failed);
    EXPECT_EQ(said(ttk::Severity::Error).size(), 1U);
}

TEST_F(ProfileBridgeTest, LaunchingAnotherCardSelectsItFirst) {
    install(model::world(3));

    profile().launchAt(1);

    EXPECT_EQ(cfg().profileName, "Bravo");
    EXPECT_EQ(runs().stateOf("profile:id-bravo"), State::RunState::Failed);
    EXPECT_EQ(runs().stateOf("profile:id-alpha"), State::RunState::None);
}

TEST_F(ProfileBridgeTest, LaunchingACardThatIsNotThereDoesNothing) {
    install(model::world(1));

    profile().launchAt(1);
    profile().launchAt(-1);

    EXPECT_TRUE(notifier.messages().empty());
}

TEST_F(ProfileBridgeTest, DonorsAreProfilesOnThePortWithAConfigWritten) {
    Config made = model::world(4);

    made.profiles[3].port = "Chocolate";
    install(made);

    model::write(portConfig("Alpha"));
    model::write(portConfig("Bravo"));
    model::write(portConfig("Delta"));

    ProfileBridge::pushConfigDonors();

    ASSERT_EQ(cfg().configDonors.size(), 1U);
    EXPECT_EQ(cfg().configDonors.front().id, "id-bravo");
    EXPECT_EQ(cfg().configDonors.front().name, "Bravo");
    EXPECT_EQ(cfg().configDonors.front().file, ttk::Format::from_path(portConfig("Bravo")));
}

TEST_F(ProfileBridgeTest, ADonorThatSharesThePortsConfigSaysSo) {
    Config made = model::world(2);

    made.profiles[1].sharedConfig = true;
    install(made);

    model::write(portConfig("Bravo"));

    ProfileBridge::pushConfigDonors();

    ASSERT_EQ(cfg().configDonors.size(), 1U);
    EXPECT_TRUE(cfg().configDonors.front().shared);
}

TEST_F(ProfileBridgeTest, AProfileWithNoPortHasNoDonors) {
    Config made = model::world(2);

    made.profiles[0].port.clear();
    made.profiles[1].port.clear();
    install(made);

    model::write(portConfig("Bravo"));

    ProfileBridge::pushConfigDonors();

    EXPECT_TRUE(cfg().configDonors.empty());
}

TEST_F(ProfileBridgeTest, CopyingAnEngineConfigCopiesTheFile) {
    Config made = model::world(2);

    made.general.profileConfigs = true;
    install(made);

    model::write(portConfig("Bravo"), "seta bravo 1");

    profile().copyEngineConfig("id-bravo");

    std::ifstream copied(portConfig("Alpha"));
    std::string text;

    std::getline(copied, text);

    EXPECT_EQ(text, "seta bravo 1");
    EXPECT_EQ(said(ttk::Severity::Success).size(), 1U);
}

TEST_F(ProfileBridgeTest, ACopiedConfigNothingReadsIsAWarning) {
    install(model::world(2));

    model::write(portConfig("Bravo"));

    profile().copyEngineConfig("id-bravo");

    const std::vector<ttk::Notice> warnings = said(ttk::Severity::Warning);

    ASSERT_EQ(warnings.size(), 1U);
    EXPECT_TRUE(warnings.front().body.contains("nothing reads it yet")) << warnings.front().body;
}

TEST_F(ProfileBridgeTest, ACopyFromAConfigNeverWrittenIsAnError) {
    install(model::world(2));

    profile().copyEngineConfig("id-bravo");

    const std::vector<ttk::Notice> errors = said(ttk::Severity::Error);

    ASSERT_EQ(errors.size(), 1U);
    EXPECT_TRUE(errors.front().body.starts_with("Could not copy Bravo's engine config: ")) << errors.front().body;
}

TEST_F(ProfileBridgeTest, ACopyFromAnUnknownProfileDoesNothing) {
    install(model::world(2));

    profile().copyEngineConfig("id-nobody");

    EXPECT_TRUE(notifier.messages().empty());
}

TEST_F(ProfileBridgeTest, NoConfigIsCopiedWhileEitherProfileRuns) {
    install(model::world(2));

    model::write(portConfig("Bravo"));
    hold("id-bravo");

    profile().copyEngineConfig("id-bravo");

    EXPECT_FALSE(fs::exists(portConfig("Alpha")));
    EXPECT_EQ(said(ttk::Severity::Warning).size(), 1U);
}

}
