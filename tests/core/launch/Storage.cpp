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

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Env.h"
#include "ttk/system/Paths.h"

#include "Launch.h"
#include "core/launch/Storage.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

using launch::Strings;

class StorageTest : public ::testing::Test {
protected:
    Config config;

    void SetUp() override {
        fs::remove_all(launch::profiles());

        config = launch::single("/games/gzdoom");
        config.general.profileConfigs = true;
        profile().config = "alpha.cfg";
    }

    Profile &profile() { return config.activeProfile(); }

    static fs::path own(const std::string &stem = "alpha") { return launch::profiles() / stem; }

    // Written in the order given, each a minute after the last, so the newest is the last.
    static void written(const fs::path &directory, const Strings &names) {
        const auto start = fs::file_time_type::clock::now() - std::chrono::hours(1);
        int minute = 0;

        for (const std::string &name : names) {
            launch::write(directory / name, name);
            fs::last_write_time(directory / name, start + std::chrono::minutes(minute++));
        }
    }

    void on(const char *program, const bool dos = false) {
        config.ports.front().file = (fs::path("/games") / program).string();
        config.ports.front().dosbox = dos;
    }
};

Profile named(const std::string &id, const std::string &file) {
    Profile profile;

    profile.id = id;
    profile.config = file;

    return profile;
}

}

TEST_F(StorageTest, AnAbsoluteAddOnWithNothingThereIsMarkedGone) {
    const fs::path folder = tests::Sandbox::scratch("storage-addons");

    launch::write(folder / "there.wad", "PWAD");
    profile().files = {{.file = (folder / "there.wad").string()},
                       {.file = folder.string()},
                       {.file = (folder / "gone.wad").string()},
                       {.file = (folder / "gone.deh").string(), .enabled = false}};

    Storage::markGoneAddons(profile());

    EXPECT_FALSE(profile().files[0].gone);
    EXPECT_FALSE(profile().files[1].gone) << "a folder is there";
    EXPECT_TRUE(profile().files[2].gone);
    EXPECT_TRUE(profile().files[3].gone) << "turned off";
}

TEST_F(StorageTest, ARelativeAddOnIsNeverMarkedGone) {
    profile().files = {{.file = "nowhere.wad"}, {.file = (fs::path("mods") / "nowhere.wad").string()}};

    Storage::markGoneAddons(profile());

    EXPECT_FALSE(profile().files[0].gone);
    EXPECT_FALSE(profile().files[1].gone);
}

TEST_F(StorageTest, AnAddOnThatComesBackIsNoLongerMarkedGone) {
    const fs::path file = tests::Sandbox::scratch("storage-back") / "back.wad";

    profile().files = {{.file = file.string()}};
    Storage::markGoneAddons(profile());

    EXPECT_TRUE(profile().files[0].gone);

    launch::write(file, "PWAD");
    Storage::markGoneAddons(profile());

    EXPECT_FALSE(profile().files[0].gone);
}

TEST_F(StorageTest, AProfileConfigLivesInAFolderNamedForIt) {
    EXPECT_EQ(Storage::configFile(profile()), own() / "alpha.cfg");
}

TEST_F(StorageTest, AnAbsoluteProfileConfigIsWhereItSays) {
    profile().config = "/configs/mine.cfg";

    EXPECT_EQ(Storage::configFile(profile()), fs::path("/configs/mine.cfg"));
}

TEST_F(StorageTest, AProfileWithNoConfigHasNoFile) {
    profile().config.clear();

    EXPECT_EQ(Storage::configFile(profile()), fs::path());
    EXPECT_EQ(Storage::profileDirectory(profile()), fs::path());
    EXPECT_EQ(Storage::replayDirectory(profile()), fs::path());
}

TEST_F(StorageTest, TheLaunchConfigNeedsProfileConfigsOn) {
    EXPECT_EQ(Storage::configFile(config), own() / "alpha.cfg");

    config.general.profileConfigs = false;

    EXPECT_EQ(Storage::configFile(config), fs::path());
}

TEST_F(StorageTest, ASharedConfigLeavesThePortItsOwn) {
    profile().set(Switch::sharedConfig, true);

    EXPECT_EQ(Storage::configFile(config), fs::path());
    EXPECT_EQ(Storage::saveDirectory(config), fs::path());
}

TEST_F(StorageTest, ADosPortThatTakesAConfigIsHandedOneWhateverTheSetting) {
    on("MBF.EXE", true);
    config.general.profileConfigs = false;

    EXPECT_EQ(Storage::configFile(config), own() / "zdl.cfg");
}

TEST_F(StorageTest, ADosPortWithoutAConfigSwitchIsHandedNone) {
    on("DOOM2.EXE", true);

    EXPECT_EQ(Storage::configFile(config), fs::path());
}

TEST_F(StorageTest, TheExtraConfigSitsBesideTheConfig) {
    EXPECT_EQ(Storage::extraConfigFile("/a/b/alpha.cfg"), fs::path("/a/b/alpha-extra.cfg"));
    EXPECT_EQ(Storage::extraConfigFile("/a/b/alpha"), fs::path("/a/b/alpha-extra"));
}

TEST_F(StorageTest, SavesGoInAFolderBesideTheConfig) {
    EXPECT_EQ(Storage::saveDirectory(config), own() / "saves");
    EXPECT_EQ(Storage::saveFolder(config), own() / "saves");
}

TEST_F(StorageTest, APortWithoutASaveSwitchHasNoSaveFolder) {
    on("doomlegacy");

    EXPECT_EQ(Storage::saveDirectory(config), own() / "saves");
    EXPECT_EQ(Storage::saveFolder(config), fs::path());
    EXPECT_TRUE(Storage::saves(config).empty());
}

TEST_F(StorageTest, SavesAreListedNewestFirst) {
    written(own() / "saves", {"save1.zds", "save3.zds", "save2.zds"});

    EXPECT_EQ(Storage::saves(config), (Strings{"save2.zds", "save3.zds", "save1.zds"}));
}

TEST_F(StorageTest, SavesAreOnlyThePortsOwnKind) {
    written(own() / "saves", {"save1.zds", "boom1.dsg", "notes.txt", "LOUD.ZDS"});
    fs::create_directories(own() / "saves" / "folder.zds");

    EXPECT_EQ(Storage::saves(config), (Strings{"LOUD.ZDS", "save1.zds"}));
}

TEST_F(StorageTest, ABoomPortListsItsOwnKindOfSave) {
    on("prboom-plus");
    written(own() / "saves", {"save1.zds", "prboom-plus-savegame0.dsg"});

    EXPECT_EQ(Storage::saves(config), (Strings{"prboom-plus-savegame0.dsg"}));
}

TEST_F(StorageTest, ABoomProfileListsOnlyTheSavesItsSlotsLoad) {
    on("prboom-plus");
    written(own() / "saves", {"prboom-plus-savegame2.dsg", "prbmsav3.dsg", "quick.dsg"});

    EXPECT_EQ(Storage::saves(config), (Strings{"prboom-plus-savegame2.dsg"}));
}

TEST_F(StorageTest, ANuggetPeriodicAutosaveIsNoSlot) {
    on("nugget-doom");
    written(own() / "saves", {"nuggsav3.dsg", "autosav3.dsg", "autosave.dsg"});
    profile().save = {.enabled = true, .file = "autosav3.dsg"};

    EXPECT_EQ(Storage::saves(config), (Strings{"autosave.dsg", "nuggsav3.dsg"}));
    EXPECT_EQ(Storage::saveTrouble(config),
              "This port loads a save by the slot it sits in, and no slot holds autosav3.dsg.");
}

TEST_F(StorageTest, ACherryProfileListsItsSlotsButNotItsPeriodicAutosaves) {
    on("cherry-doom");
    written(own() / "saves", {"chersav3.dsg", "autosav3.dsg", "autosave.dsg"});

    EXPECT_EQ(Storage::saves(config), (Strings{"autosave.dsg", "chersav3.dsg"}));
}

TEST_F(StorageTest, SavesWrittenTogetherAreInNaturalOrder) {
    const fs::path saves = own() / "saves";
    const auto when = fs::file_time_type::clock::now() - std::chrono::hours(1);

    for (const char *name : {"save10.zds", "save9.zds", "save1.zds"}) {
        launch::write(saves / name, name);
        fs::last_write_time(saves / name, when);
    }

    EXPECT_EQ(Storage::saves(config), (Strings{"save1.zds", "save9.zds", "save10.zds"}));
}

TEST_F(StorageTest, NoSaveFolderOnDiskListsNothing) {
    EXPECT_TRUE(Storage::saves(config).empty());
}

TEST_F(StorageTest, ASaveNameIsFoundInTheSaveFolder) {
    profile().save.file = "save1.zds";

    EXPECT_EQ(Storage::saveFile(config), own() / "saves" / "save1.zds");
}

TEST_F(StorageTest, AnAbsoluteSaveIsWhereItSays) {
    config.general.profileConfigs = false;
    profile().save.file = "/saves/save1.zds";

    EXPECT_EQ(Storage::saveFile(config), fs::path("/saves/save1.zds"));
}

TEST_F(StorageTest, ASaveInTheProfileFolderIsForgottenForACopy) {
    profile().save = {.enabled = true, .file = "save1.zds"};

    Storage::forgetOwnSave(config);

    EXPECT_EQ(profile().save, SaveSettings{});
}

TEST_F(StorageTest, AnAbsoluteSaveIsKeptForACopy) {
    profile().save = {.enabled = true, .file = "/saves/save1.zds"};

    Storage::forgetOwnSave(config);

    EXPECT_EQ(profile().save, (SaveSettings{.enabled = true, .file = "/saves/save1.zds"}));
}

TEST_F(StorageTest, ASaveOfThePortSettingsIsKeptForACopy) {
    profile().set(Switch::sharedConfig, true);
    profile().save = {.enabled = true, .file = "save1.zds"};

    Storage::forgetOwnSave(config);

    EXPECT_EQ(profile().save, (SaveSettings{.enabled = true, .file = "save1.zds"}));
}

TEST_F(StorageTest, ASaveNameWithNoFolderIsNowhere) {
    on("mystery-engine");
    config.general.profileConfigs = false;
    profile().save.file = "save1.zds";

    EXPECT_EQ(Storage::saveFile(config), fs::path());
}

TEST_F(StorageTest, TheSlotIsTheNumberOnTheEndOfTheName) {
    EXPECT_EQ(Storage::saveSlot("save07.zds"), 7);
    EXPECT_EQ(Storage::saveSlot("prbmsav3.dsg"), 3);
    EXPECT_EQ(Storage::saveSlot("doomsav0.dsg"), 0);
    EXPECT_EQ(Storage::saveSlot("5.dsg"), 5);
    EXPECT_EQ(Storage::saveSlot("save9999"), 9999);
}

TEST_F(StorageTest, ANameWithoutATrailingNumberHasNoSlot) {
    EXPECT_EQ(Storage::saveSlot("quick.zds"), -1);
    EXPECT_EQ(Storage::saveSlot("3rd.zds"), -1);
    EXPECT_EQ(Storage::saveSlot(""), -1);
}

TEST_F(StorageTest, TheDsdaLineNamesTheSlotAtTheEnd) {
    EXPECT_EQ(Storage::saveSlot("dsda-doom-savegame0.dsg"), 0);
    EXPECT_EQ(Storage::saveSlot("dsda-doom-savegame118.dsg"), 118);
    EXPECT_EQ(Storage::saveSlot("nyan-doom-savegame7.dsg"), 7);
}

TEST_F(StorageTest, DsdaListsItsSaves) {
    on("dsda-doom");
    written(own() / "saves", {"dsda-doom-savegame1.dsg", "notes.txt", "dsda-doom-savegame12.dsg"});

    EXPECT_EQ(Storage::saves(config), (Strings{"dsda-doom-savegame12.dsg", "dsda-doom-savegame1.dsg"}));
}

TEST_F(StorageTest, ASlotPastTheLastThePortTakesIsTrouble) {
    on("dsda-doom");
    written(own() / "saves", {"dsda-doom-savegame119.dsg", "dsda-doom-savegame118.dsg"});
    profile().save = {.enabled = true, .file = "dsda-doom-savegame119.dsg"};

    EXPECT_EQ(Storage::saveTrouble(config),
              "This port loads slots up to 118, so it cannot load dsda-doom-savegame119.dsg.");

    profile().save.file = "dsda-doom-savegame118.dsg";

    EXPECT_EQ(Storage::saveTrouble(config), "");
}

TEST_F(StorageTest, MoreThanFourDigitsIsNoSlot) {
    EXPECT_EQ(Storage::saveSlot("save12345.dsg"), -1);
}

TEST_F(StorageTest, ADisabledSaveIsNoTrouble) {
    profile().save = {.enabled = false, .file = "/nowhere/save1.zds"};

    EXPECT_EQ(Storage::saveTrouble(config), "");
}

TEST_F(StorageTest, ASaveThatIsGoneIsTrouble) {
    profile().save = {.enabled = true, .file = "save1.zds"};

    EXPECT_EQ(Storage::saveTrouble(config),
              "There is no save at " + (own() / "saves" / "save1.zds").string() + " any more.");
}

TEST_F(StorageTest, ASlotPortWithASaveWithoutANumberIsTrouble) {
    on("prboom-plus");
    written(own() / "saves", {"quick.dsg"});
    profile().save = {.enabled = true, .file = "quick.dsg"};

    EXPECT_EQ(Storage::saveTrouble(config),
              "This port loads a save by the slot it sits in, and there is no number in quick.dsg to take one from.");
}

TEST_F(StorageTest, AWoofProfileLoadsItsAutosaveAsSlot255) {
    on("woof");
    written(own() / "saves", {"woofsav1.dsg", "autosave.dsg"});
    profile().save = {.enabled = true, .file = "autosave.dsg"};

    EXPECT_EQ(Storage::saves(config), (Strings{"autosave.dsg", "woofsav1.dsg"}));
    EXPECT_EQ(Storage::loadName(config, Storage::saveFile(config), Dialect::of(config)), "255");
    EXPECT_EQ(Storage::saveTrouble(config), "");
}

TEST_F(StorageTest, ABoomProfileHasNoAutosaveToLoad) {
    on("prboom-plus");
    written(own() / "saves", {"autosave.dsg"});
    profile().save = {.enabled = true, .file = "autosave.dsg"};

    EXPECT_EQ(Storage::loadName(config, Storage::saveFile(config), Dialect::of(config)), "");
    EXPECT_EQ(
            Storage::saveTrouble(config),
            "This port loads a save by the slot it sits in, and there is no number in autosave.dsg to take one from.");
}

TEST_F(StorageTest, ASaveThatIsThereIsNoTrouble) {
    written(own() / "saves", {"save1.zds"});
    profile().save = {.enabled = true, .file = "save1.zds"};

    EXPECT_EQ(Storage::saveTrouble(config), "");
}

TEST_F(StorageTest, APlainNameOwnsItsFolder) {
    EXPECT_TRUE(Storage::ownsDirectory(config, profile()));
}

TEST_F(StorageTest, ANameWithADirectoryOwnsNothing) {
    EXPECT_FALSE(Storage::ownsDirectory(config, named("x", "/configs/mine.cfg")));
    EXPECT_FALSE(Storage::ownsDirectory(config, named("x", "sub/mine.cfg")));
    EXPECT_FALSE(Storage::ownsDirectory(config, named("x", "")));
}

TEST_F(StorageTest, DotNamesOwnNothing) {
    EXPECT_FALSE(Storage::ownsDirectory(config, named("x", "..cfg")));
    EXPECT_FALSE(Storage::ownsDirectory(config, named("x", "...cfg")));
}

TEST_F(StorageTest, AFolderTwoProfilesShareIsNeithers) {
    config.profiles.push_back(named("other", "ALPHA.cfg"));

    EXPECT_FALSE(Storage::ownsDirectory(config, profile()));
}

TEST_F(StorageTest, RenamingBeforeAnythingIsWrittenChangesOnlyTheName) {
    std::string error;

    EXPECT_TRUE(Storage::renameDirectory(profile(), "beta.cfg", &error));
    EXPECT_EQ(profile().config, "beta.cfg");
    EXPECT_FALSE(fs::exists(own("beta")));
}

TEST_F(StorageTest, RenamingMovesTheFolderAndTheConfigsNamedAfterIt) {
    launch::write(own() / "alpha.cfg", "settings");
    launch::write(own() / "alpha-extra.cfg", "more");
    launch::write(own() / "saves" / "save1.zds", "save");

    ASSERT_TRUE(Storage::renameDirectory(profile(), "beta.cfg"));

    EXPECT_EQ(profile().config, "beta.cfg");
    EXPECT_FALSE(fs::exists(own()));
    EXPECT_EQ(launch::slurp(own("beta") / "beta.cfg"), "settings");
    EXPECT_EQ(launch::slurp(own("beta") / "beta-extra.cfg"), "more");
    EXPECT_EQ(launch::slurp(own("beta") / "saves" / "save1.zds"), "save");
}

TEST_F(StorageTest, RenamingOntoAFolderThatIsThereUndoesEverything) {
    std::string error;

    launch::write(own() / "alpha.cfg", "settings");
    launch::write(own("beta") / "beta.cfg", "theirs");

    EXPECT_FALSE(Storage::renameDirectory(profile(), "beta.cfg", &error));

    EXPECT_EQ(profile().config, "alpha.cfg");
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(launch::slurp(own() / "alpha.cfg"), "settings");
    EXPECT_EQ(launch::slurp(own("beta") / "beta.cfg"), "theirs");
}

TEST_F(StorageTest, RenamingToTheSameNameIsDone) {
    launch::write(own() / "alpha.cfg", "settings");

    EXPECT_TRUE(Storage::renameDirectory(profile(), "alpha.cfg"));
    EXPECT_EQ(launch::slurp(own() / "alpha.cfg"), "settings");
}

TEST_F(StorageTest, DiscardingRemovesTheFolderAndAllInIt) {
    launch::write(own() / "alpha.cfg", "settings");
    launch::write(own() / "replays" / "run.lmp", "demo");

    EXPECT_TRUE(Storage::discardDirectory(profile()));
    EXPECT_FALSE(fs::exists(own()));
}

TEST_F(StorageTest, DiscardingAProfileWithNoFolderIsDone) {
    EXPECT_TRUE(Storage::discardDirectory(named("x", "")));
}

TEST_F(StorageTest, AProfileRunsInItsOwnFolderWhichIsMadeForIt) {
    EXPECT_EQ(Storage::runDirectory(config, "/games"), own());
    EXPECT_TRUE(fs::is_directory(own()));
}

TEST_F(StorageTest, AProfileWithNoFolderRunsInThePortsDirectory) {
    profile().config.clear();

    EXPECT_EQ(Storage::runDirectory(config, "/games"), fs::path("/games"));
}

TEST_F(StorageTest, ANativePortsConfigIsTheProfilesOwn) {
    EXPECT_EQ(Storage::portConfigFile(config, profile()), own() / "alpha.cfg");
}

TEST_F(StorageTest, ADosPortsConfigHasANameDosCanSpell) {
    on("MBF.EXE", true);

    EXPECT_EQ(Storage::portConfigFile(config, profile()), own() / "zdl.cfg");
}

TEST_F(StorageTest, APortOutsideTheZdoomFamilyKeepsItsConfigApart) {
    on("dsda-doom");

    EXPECT_EQ(Storage::configFile(config), own() / "dsda.cfg");
    EXPECT_EQ(Storage::portConfigFile(config, profile()), own() / "dsda.cfg");
    EXPECT_EQ(Storage::saveDirectory(config), own() / "saves");
}

TEST_F(StorageTest, AnAbsoluteConfigIsUsedAsItStandsByAnyPort) {
    on("dsda-doom");
    profile().config = "/configs/mine.cfg";

    EXPECT_EQ(Storage::configFile(config), fs::path("/configs/mine.cfg"));
}

TEST_F(StorageTest, TheConfigToCopyFromFallsBackToTheProfileConfigItsPortWrote) {
    on("dsda-doom");
    launch::write(own() / "alpha.cfg", "mouse_sensitivity 10\n");

    Profile other = named("other", "beta.cfg");
    other.port = profile().port;

    EXPECT_EQ(Storage::writtenPortConfig(config, profile()), own() / "alpha.cfg");
    EXPECT_TRUE(Storage::copyPortConfig(config, profile(), other));
    EXPECT_EQ(launch::slurp(own("beta") / "dsda.cfg"), "mouse_sensitivity 10\n");

    launch::write(own() / "dsda.cfg", "current");

    EXPECT_EQ(Storage::writtenPortConfig(config, profile()), own() / "dsda.cfg");
}

TEST_F(StorageTest, AnOldConfigNoZdoomPortWroteIsTakenUp) {
    on("chocolate-doom");
    launch::write(own() / "alpha.cfg", "key_up 72\n");
    launch::write(own() / "alpha-extra.cfg", "fullscreen 0\n");

    Storage::adoptConfig(config);

    EXPECT_EQ(launch::slurp(own() / "chocolate.cfg"), "key_up 72\n");
    EXPECT_EQ(launch::slurp(own() / "chocolate-extra.cfg"), "fullscreen 0\n");
    EXPECT_TRUE(fs::exists(own() / "alpha.cfg"));
}

TEST_F(StorageTest, AZdoomIniIsNotTakenUpByAnotherPort) {
    on("dsda-doom");
    launch::write(own() / "alpha.cfg", "# This file was generated by GZDoom g4.14.2 on Fri Nov 13\n[Doom.Bindings]\n");

    Storage::adoptConfig(config);

    EXPECT_FALSE(fs::exists(own() / "dsda.cfg"));
}

TEST_F(StorageTest, TakingUpNeverWritesOverTheFamilyConfig) {
    on("woof");
    launch::write(own() / "alpha.cfg", "old");
    launch::write(own() / "woof.cfg", "current");

    Storage::adoptConfig(config);

    EXPECT_EQ(launch::slurp(own() / "woof.cfg"), "current");
}

TEST_F(StorageTest, CopyingAPortConfigPutsItInTheOtherFolder) {
    const Profile other = named("other", "beta.cfg");

    launch::write(own() / "alpha.cfg", "settings");

    EXPECT_TRUE(Storage::copyPortConfig(config, profile(), other));
    EXPECT_EQ(launch::slurp(own("beta") / "beta.cfg"), "settings");
}

TEST_F(StorageTest, CopyingAConfigThatIsNotThereFails) {
    std::string error;

    EXPECT_FALSE(Storage::copyPortConfig(config, profile(), named("other", "beta.cfg"), &error));
    EXPECT_FALSE(error.empty());
}

TEST_F(StorageTest, CopyingBetweenProfilesWithoutConfigsIsNothingToDo) {
    EXPECT_TRUE(Storage::copyPortConfig(config, profile(), named("other", "")));
    EXPECT_TRUE(Storage::copyPortConfig(config, profile(), profile()));
}

TEST_F(StorageTest, ReplaysGoBesideTheConfigWhateverTheSetting) {
    config.general.profileConfigs = false;

    EXPECT_EQ(Storage::replayDirectory(config), own() / "replays");
}

TEST_F(StorageTest, ReplaysAreListedNewestFirstAndOnlyDemos) {
    written(own() / "replays", {"a.lmp", "c.LMP", "notes.txt", "b.lmp"});

    EXPECT_EQ(Storage::replays(config), (Strings{"b.lmp", "c.LMP", "a.lmp"}));
}

TEST_F(StorageTest, AReplayNameAlwaysCarriesLmp) {
    profile().replay.file = "run1";

    EXPECT_EQ(Storage::replayFile(config), own() / "replays" / "run1.lmp");

    profile().replay.file = "run1.LMP";

    EXPECT_EQ(Storage::replayFile(config), own() / "replays" / "run1.LMP");
}

TEST_F(StorageTest, AnAbsoluteReplayIsWhereItSays) {
    profile().replay.file = "/demos/run1.lmp";

    EXPECT_EQ(Storage::replayFile(config), fs::path("/demos/run1.lmp"));
}

TEST_F(StorageTest, AReplayNameWithNoFolderIsNowhere) {
    profile().config.clear();
    profile().replay.file = "run1";

    EXPECT_EQ(Storage::replayFile(config), fs::path());
}

TEST_F(StorageTest, NoReplayIsNoTrouble) {
    profile().replay = {.mode = ReplayMode::Off, .file = "gone"};

    EXPECT_EQ(Storage::replayTrouble(config), "");
}

TEST_F(StorageTest, PlayingADemoThatIsGoneIsTrouble) {
    profile().replay = {.mode = ReplayMode::Play, .file = "gone"};

    EXPECT_EQ(Storage::replayTrouble(config),
              "There is no demo at " + (own() / "replays" / "gone.lmp").string() + " any more.");
}

TEST_F(StorageTest, RecordingANewDemoIsNoTrouble) {
    profile().replay = {.mode = ReplayMode::Record, .file = "a-long-name-for-a-demo"};

    EXPECT_EQ(Storage::replayTrouble(config), "");
}

TEST_F(StorageTest, RecordingUnderANameDosCannotSpellIsTroubleForADosPort) {
    on("DOOM2.EXE", true);
    profile().replay = {.mode = ReplayMode::Record, .file = "a-long-name-for-a-demo"};

    EXPECT_EQ(Storage::replayTrouble(config),
              "DOS cannot spell a-long-name-for-a-demo.lmp, and eight characters and three is all it can spell, so "
              "there would be nothing by that name to record into.");
}

TEST_F(StorageTest, OnlyUzdoomHasANetgameSaveFolder) {
    EXPECT_EQ(Storage::netgameSaveFolder(config), fs::path());

    on("zandronum");

    EXPECT_EQ(Storage::netgameSaveFolder(config), fs::path());
}

#ifndef _WIN32
TEST_F(StorageTest, UzdoomKeepsNetgameSavesUnderTheDataHome) {
    on("uzdoom");

    EXPECT_EQ(Storage::netgameSaveFolder(config),
              fs::path(ttk::Env::get("XDG_DATA_HOME")) / "games" / "uzdoom" / "savegames" / "netgame");
}

TEST_F(StorageTest, UzdoomFallsBackToLocalShareWithoutADataHome) {
    const std::string set = ttk::Env::get("XDG_DATA_HOME");

    on("uzdoom");
    ttk::Env::set("XDG_DATA_HOME", "");

    const fs::path folder = Storage::netgameSaveFolder(config);

    ttk::Env::set("XDG_DATA_HOME", set.c_str());

    EXPECT_EQ(folder, ttk::Paths::home_directory() / ".local" / "share" / "games" / "uzdoom" / "savegames" / "netgame");
}

TEST_F(StorageTest, AUzdoomUnderDosboxHasNoNetgameSaveFolder) {
    on("uzdoom", true);

    EXPECT_EQ(Storage::netgameSaveFolder(config), fs::path());
}

TEST_F(StorageTest, AProfileWithoutAPortHasNoNetgameSaveFolder) {
    profile().port.clear();

    EXPECT_EQ(Storage::netgameSaveFolder(config), fs::path());
}

TEST_F(StorageTest, AFlatpakUzdoomKeepsNetgameSavesInItsSandbox) {
    const fs::path sandbox =
            ttk::Paths::home_directory() / ".var" / "app" / "org.zdoom.UZDoom" / "data" / "games" / "uzdoom";

    config.ports.front().file = "/var/lib/flatpak/exports/bin/org.zdoom.UZDoom";

    EXPECT_EQ(Storage::netgameSaveFolder(config), sandbox / "savegames" / "netgame");

    config.ports.front().file =
            (ttk::Paths::home_directory() / ".local/share/flatpak/exports/bin/org.zdoom.UZDoom").string();

    EXPECT_EQ(Storage::netgameSaveFolder(config), sandbox / "savegames" / "netgame");
}
#endif
