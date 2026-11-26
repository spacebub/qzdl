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
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/notices/Notice.h"
#include "ttk/util/Format.h"

#include "Model.h"
#include "core/config/Config.h"
#include "core/launch/KnownPorts.h"
#include "core/ports/Catalog.h"
#include "core/ports/Detect.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/ListsBridge.h"
#include "gui/state/State.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace {

class ListsBridgeTest : public model::Bridged {
protected:
    ListsBridge &lists() const { return bridge().lists(); }

    // Alpha with four add-ons, the second turned off.
    void loaded() const {
        Config made = model::world(2);

        made.profiles.front().files = {
                {.file = "/addons/one.wad", .enabled = true},
                {.file = "/addons/two.wad", .enabled = false},
                {.file = "/addons/three.pk3", .enabled = true},
                {.file = "/addons/four.deh", .enabled = true},
        };

        install(made);
    }

    static std::vector<std::string> files() {
        std::vector<std::string> out;

        for (const FileEntry &entry : config().activeProfile().files) {
            out.push_back(entry.file);
        }

        return out;
    }
};

TEST_F(ListsBridgeTest, TheActiveProfilesFilesAreListedInOrder) {
    loaded();

    ASSERT_EQ(cfg().files.size(), 4U);
    EXPECT_EQ(cfg().files[0].name, "one.wad");
    EXPECT_EQ(cfg().files[2].name, "three.pk3");
    EXPECT_EQ(cfg().files[2].file, "/addons/three.pk3");
    EXPECT_EQ(cfg().files[2].directory, "/addons");
    EXPECT_EQ(cfg().files[3].index, 3);
}

TEST_F(ListsBridgeTest, AFileRowSaysWhetherItIsLoaded) {
    loaded();

    EXPECT_TRUE(cfg().files[0].loaded);
    EXPECT_FALSE(cfg().files[1].loaded);
}

TEST_F(ListsBridgeTest, EnabledCountCountsTheLoadedFiles) {
    loaded();

    EXPECT_EQ(cfg().enabledCount, 3);
}

TEST_F(ListsBridgeTest, AFileThatIsNotOnDiskIsMissing) {
    Config made = model::world(1);

    made.profiles.front().files = {
            {.file = (folder() / "absent.wad").string(), .enabled = true},
            {.file = tests::Corpus::addons(1).front(), .enabled = true},
    };
    install(made);

    EXPECT_TRUE(cfg().files[0].missing);
    EXPECT_FALSE(cfg().files[1].missing);
}

TEST_F(ListsBridgeTest, AMissingFileStaysListedButLeavesTheLaunch) {
    Config made = model::world(1);
    const std::string kept = tests::Corpus::addons(1).front();

    made.profiles.front().files = {
            {.file = (folder() / "absent.wad").string(), .enabled = true},
            {.file = kept, .enabled = true},
    };
    install(made);
    preview();

    EXPECT_TRUE(cfg().files[0].missing);
    EXPECT_FALSE(cfg().commandLine.contains("absent.wad")) << cfg().commandLine;
    EXPECT_TRUE(cfg().commandLine.contains(kept)) << cfg().commandLine;
}

TEST_F(ListsBridgeTest, MarkingAFileGoneSavesNothing) {
    Config made = model::world(1);

    made.profiles.front().files = {{.file = (folder() / "absent.wad").string(), .enabled = true}};
    install(made);
    settle();

    EXPECT_TRUE(config().activeProfile().files.front().gone);
    EXPECT_TRUE(saved().profiles.empty());
}

TEST_F(ListsBridgeTest, ARelativeFileIsMissingAsBefore) {
    Config made = model::world(1);

    made.profiles.front().files = {{.file = "nowhere-relative.wad", .enabled = true}};
    install(made);

    EXPECT_TRUE(cfg().files[0].missing);
    EXPECT_FALSE(config().activeProfile().files.front().gone);
}

TEST_F(ListsBridgeTest, GamesAreListedByNameWithTheirKindLowered) {
    Config made = model::world(1);

    made.iwads.push_back(model::game("TNT", "/games/TNT.WAD"));
    install(made);

    ASSERT_EQ(cfg().iwads.size(), 3U);
    EXPECT_EQ(cfg().iwadNames, (std::vector<std::string>{"Doom II", "Plutonia", "TNT"}));
    EXPECT_EQ(cfg().iwads[2].kind, ".wad");
    EXPECT_EQ(cfg().iwads[2].directory, "/games");
    EXPECT_EQ(cfg().iwads[2].index, 2);
}

TEST_F(ListsBridgeTest, AGameOnDiskIsNotMissing) {
    Config made = model::world(1);

    made.iwads.front().file = tests::Corpus::iwad().string();
    install(made);

    EXPECT_FALSE(cfg().iwads[0].missing);
    EXPECT_TRUE(cfg().iwads[1].missing);
}

TEST_F(ListsBridgeTest, OnlyADosEngineCarriesTheDosBadge) {
    install(model::world(1));

    ASSERT_EQ(cfg().engineBadges.size(), cfg().engineNames.size());
    EXPECT_EQ(cfg().engineBadges.back(), "DOS");
    EXPECT_EQ(cfg().engineBadges.front(), "");
    EXPECT_TRUE(cfg().engines.back().dosbox);
    EXPECT_FALSE(cfg().engines.front().dosbox);
}

TEST_F(ListsBridgeTest, ARowCopiesTheManagedFlag) {
    Config made = model::world(1);

    made.engines.front().managed = true;
    install(made);

    EXPECT_TRUE(cfg().engines[0].managed);
    EXPECT_FALSE(cfg().engines[1].managed);
}

TEST_F(ListsBridgeTest, TheFoundBadgeFollowsTheDetectedFlag) {
    Config made = model::world(1);

    made.engines.front().detected = true;
    install(made);

    EXPECT_TRUE(cfg().engines[0].detected);
    EXPECT_FALSE(cfg().engines[1].detected);
}

#ifdef __linux__
TEST_F(ListsBridgeTest, ARowSaysWhetherTheEngineIsAFlatpakExport) {
    Config made = model::world(1);

    made.engines.front().file = "/var/lib/flatpak/exports/bin/org.zdoom.UZDoom";
    install(made);

    EXPECT_TRUE(cfg().engines[0].flatpak);
    EXPECT_FALSE(cfg().engines[1].flatpak);
}

TEST_F(ListsBridgeTest, ADosEngineAtAnExportPathGetsNoFlatpakPill) {
    Config made = model::world(1);

    made.engines.back().file = "/var/lib/flatpak/exports/bin/org.zdoom.UZDoom";
    install(made);

    ASSERT_TRUE(cfg().engines.back().dosbox);
    EXPECT_FALSE(cfg().engines.back().flatpak);
}

TEST_F(ListsBridgeTest, AStoredFlatpakReleaseIsSaved) {
    const std::string file = "/var/lib/flatpak/exports/bin/org.zdoom.UZDoom";
    Config made = model::world(1);

    made.engines.front().file = file;
    made.engines.front().version = "4.14.3";
    install(made);

    lists().storeVersions(std::vector<Detect::Installed>{{.file = file, .version = "5.0.3"}});
    settle();

    EXPECT_EQ(config().engines.front().version, "5.0.3");
    ASSERT_FALSE(saved().engines.empty());
    EXPECT_EQ(saved().engines.front().version, "5.0.3");
}

TEST_F(ListsBridgeTest, AReleaseTheEngineStoresAlreadySavesNothing) {
    const std::string file = "/var/lib/flatpak/exports/bin/org.zdoom.UZDoom";
    Config made = model::world(1);

    made.engines.front().file = file;
    made.engines.front().version = "4.14.3";
    install(made);

    lists().storeVersions(std::vector<Detect::Installed>{{.file = file, .version = "4.14.3"}});
    settle();

    EXPECT_TRUE(saved().engines.empty());
}
#endif

TEST_F(ListsBridgeTest, APortDetectionWouldFindShowsNoBadgeWithoutTheFlag) {
    const std::filesystem::path file = tests::Sandbox::root() / "bin" / "helion";

    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file) << "#!/bin/sh\n";
    std::filesystem::permissions(file, std::filesystem::perms::owner_all);

    Config made = model::world(1);

    made.engines.front().file = ttk::Format::from_path(file);
    install(made);

    EXPECT_FALSE(cfg().engines[0].detected);

    std::filesystem::remove(file);
}

TEST_F(ListsBridgeTest, AFileInThePortsFolderDoesNotMakeAnEngineManaged) {
    Config made = model::world(1);

    made.engines.front().file = ttk::Format::from_path(Catalog::directory() / "gzdoom" / "gzdoom");
    made.iwads.front().file = ttk::Format::from_path(Catalog::directory() / "doom2.wad");
    install(made);

    EXPECT_FALSE(cfg().engines[0].managed);
    EXPECT_FALSE(cfg().iwads[0].managed);
}

TEST_F(ListsBridgeTest, AddingAnEngineWithNoFileDoesNothing) {
    install(model::world(1));

    EXPECT_EQ(lists().addEngine({.name = "Named", .file = "", .dosbox = false}), "");
    EXPECT_EQ(config().engines.size(), 7U);
}

TEST_F(ListsBridgeTest, AnAddedEngineWithNoNameIsNamedAfterItsFile) {
    install(model::world(0));

    EXPECT_EQ(lists().addEngine({.name = "", .file = "/opt/prboom-plus", .dosbox = false}), "PrBoom+");
    EXPECT_EQ(config().engines.back().name, "PrBoom+");
    EXPECT_EQ(cfg().engineNames.back(), "PrBoom+");
}

TEST_F(ListsBridgeTest, AnAddedEngineTakesANameNoOtherHas) {
    install(model::world(0));

    EXPECT_EQ(lists().addEngine({.name = "gzdoom", .file = "/opt/gz", .dosbox = false}), "gzdoom (2)");
    EXPECT_EQ(lists().addEngine({.name = "GZDOOM", .file = "/opt/gz2", .dosbox = false}), "GZDOOM (3)");
}

TEST_F(ListsBridgeTest, AddingAnEngineKeepsWhatItHolds) {
    install(model::world(0));

    std::ignore = lists().addEngine(
            {.name = "Doom", .file = "/opt/doom.exe", .dosbox = true, .port = Port::Port::doom, .managed = true});

    EXPECT_EQ(config().engines.back().port, Port::Port::doom);
    EXPECT_TRUE(config().engines.back().managed);
    EXPECT_TRUE(config().engines.back().dosbox);
    EXPECT_EQ(cfg().engineBadges.back(), "DOS");
}

TEST_F(ListsBridgeTest, AddingAnEngineIsSaved) {
    install(model::world(0));

    std::ignore = lists().addEngine(
            {.name = "Boom", .file = "/opt/boom.exe", .dosbox = true, .port = Port::Port::boom, .managed = true});
    settle();

    ASSERT_FALSE(saved().engines.empty());
    EXPECT_EQ(saved().engines.back().port, Port::Port::boom);
    EXPECT_TRUE(saved().engines.back().managed);
}

TEST_F(ListsBridgeTest, RenamingAnEngineRenamesItInEveryProfile) {
    install(model::world(3));

    lists().updateEngine(0, {.name = "Zandronum", .file = "/ports/gzdoom", .dosbox = false});

    EXPECT_EQ(config().engines.front().name, "Zandronum");

    for (const Profile &each : config().profiles) {
        EXPECT_EQ(each.engine, "Zandronum") << each.name;
    }

    EXPECT_EQ(cfg().engine, "Zandronum");
}

TEST_F(ListsBridgeTest, RenamingTheLibrarysEngineRenamesTheSetting) {
    Config made = model::world(1);

    made.general.gameEngine = "GZDoom";
    install(made);

    lists().updateEngine(0, {.name = "Zandronum", .file = "/ports/gzdoom", .dosbox = false});

    EXPECT_EQ(config().general.gameEngine, "Zandronum");
    EXPECT_EQ(cfg().gameEngine, "Zandronum");
}

TEST_F(ListsBridgeTest, PointingTheEngineElsewhereRefreshesTheNetgameSaveFolder) {
    Config made = model::world(1);

    made.profiles.front().engine = "UZDoom";
    made.profiles.front().multiplayer.gameType = GameType::Coop;
    made.profiles.front().multiplayer.players = 4;
    install(made);

    ASSERT_FALSE(cfg().netSaveFolder.empty());

    lists().updateEngine(5, {.name = "UZDoom", .file = "/ports/gzdoom", .dosbox = false});

    EXPECT_TRUE(cfg().netSaveFolder.empty());
}

TEST_F(ListsBridgeTest, AnEngineKeepingItsNameIsNotNumbered) {
    install(model::world(1));

    lists().updateEngine(0, {.name = "GZDoom", .file = "/ports/gzdoom-new", .dosbox = false});

    EXPECT_EQ(config().engines.front().name, "GZDoom");
    EXPECT_EQ(config().engines.front().file, "/ports/gzdoom-new");
}

TEST_F(ListsBridgeTest, AnEngineCannotBeRenamedOntoAnother) {
    install(model::world(1));

    lists().updateEngine(0, {.name = "Chocolate", .file = "/ports/gzdoom", .dosbox = false});

    EXPECT_EQ(config().engines.front().name, "Chocolate (2)");
}

TEST_F(ListsBridgeTest, MarkingTheActiveEngineDosTellsTheProfile) {
    install(model::world(1));

    EXPECT_FALSE(cfg().dosPort);

    lists().updateEngine(0, {.name = "GZDoom", .file = "/ports/gzdoom", .dosbox = true});

    EXPECT_TRUE(cfg().dosPort);
    EXPECT_TRUE(cfg().shelfProfiles.front().dosPort);
}

TEST_F(ListsBridgeTest, UpdatingARowThatIsNotThereDoesNothing) {
    install(model::world(1));

    lists().updateEngine(7, {.name = "Ghost", .file = "/ports/ghost", .dosbox = false});
    lists().updateEngine(-1, {.name = "Ghost", .file = "/ports/ghost", .dosbox = false});

    EXPECT_EQ(config().engines.size(), 7U);
    EXPECT_EQ(config().findEngine("Ghost"), nullptr);
}

TEST_F(ListsBridgeTest, UpdatingAnEngineIsSaved) {
    install(model::world(1));

    lists().updateEngine(1, {.name = "Choco", .file = "/ports/chocolate-doom", .dosbox = false});
    settle();

    EXPECT_NE(saved().findEngine("Choco"), nullptr);
}

TEST_F(ListsBridgeTest, AnAddedEngineKeepsItsPort) {
    install(model::world(1));

    std::ignore = lists().addEngine({.name = "Mine", .file = "/opt/engine", .port = Port::Port::woof});

    EXPECT_EQ(config().engines.back().port, Port::Port::woof);
}

TEST_F(ListsBridgeTest, UpdatingAnEngineSetsItsPortAndKeepsItsManagedFlag) {
    install(model::world(1));
    config().engines[1].managed = true;

    lists().updateEngine(1, {.name = "Choco", .file = "/ports/chocolate-doom", .port = Port::Port::chocolate});

    EXPECT_EQ(config().engines[1].port, Port::Port::chocolate);
    EXPECT_TRUE(config().engines[1].managed);

    config().engines[1].managed = false;
    lists().updateEngine(1, {.name = "Choco", .file = "/ports/chocolate-doom", .managed = true});

    EXPECT_FALSE(config().engines[1].managed);
}

TEST_F(ListsBridgeTest, UpdatingADetectedEngineKeepsTheFlagWhileTheFileStays) {
    install(model::world(1));
    config().engines[1].detected = true;

    lists().updateEngine(1, {.name = "Renamed", .file = config().engines[1].file, .detected = false});

    EXPECT_TRUE(config().engines[1].detected);
    EXPECT_TRUE(cfg().engines[1].detected);

    lists().updateEngine(1, {.name = "Renamed", .file = "/ports/elsewhere", .detected = true});

    EXPECT_FALSE(config().engines[1].detected);
    EXPECT_FALSE(cfg().engines[1].detected);
}

TEST_F(ListsBridgeTest, AnAddedEngineKeepsItsVersion) {
    install(model::world(1));

    std::ignore = lists().addEngine({.name = "Mine", .file = "/opt/dsda-doom", .version = "0.27.0"});

    EXPECT_EQ(config().engines.back().version, "0.27.0");
}

TEST_F(ListsBridgeTest, UpdatingAnEngineSetsItsVersion) {
    install(model::world(1));
    config().engines[1].version = "0.25.0";

    lists().updateEngine(1, {.name = "Choco", .file = "/ports/chocolate-doom", .version = "0.28.0"});

    EXPECT_EQ(config().engines[1].version, "0.28.0");

    lists().updateEngine(1, {.name = "Choco", .file = "/ports/chocolate-doom"});

    EXPECT_TRUE(config().engines[1].version.empty());
}

TEST_F(ListsBridgeTest, OnlyAnEngineWithReleasesAndNoVersionNeedsOne) {
    Config made = model::world(1);

    made.engines.push_back(model::engine("LZDoom", "/ports/lzdoom"));
    install(made);

    EXPECT_TRUE(cfg().engines[0].needsVersion);
    EXPECT_TRUE(cfg().engines[1].needsVersion);
    EXPECT_TRUE(cfg().engines[2].needsVersion);
    EXPECT_TRUE(cfg().engines[3].needsVersion);
    EXPECT_TRUE(cfg().engines[4].needsVersion);
    EXPECT_TRUE(cfg().engines[5].needsVersion);
    EXPECT_FALSE(cfg().engines[6].needsVersion);
    EXPECT_FALSE(cfg().engines[7].needsVersion);
}

TEST_F(ListsBridgeTest, APickedVersionClearsTheNeedForOne) {
    install(model::world(1));

    lists().updateEngine(3, {.name = "DSDA", .file = "/ports/dsda-doom", .version = "0.28.0"});

    EXPECT_FALSE(cfg().engines[3].needsVersion);

    lists().updateEngine(3, {.name = "DSDA", .file = "/ports/dsda-doom"});

    EXPECT_TRUE(cfg().engines[3].needsVersion);
}

TEST_F(ListsBridgeTest, AVersionWithoutAReleaseStillNeedsOne) {
    Config made = model::world(1);

    made.engines[3].version = "garbage";
    install(made);

    EXPECT_TRUE(cfg().engines[3].needsVersion);
}

TEST_F(ListsBridgeTest, AReleaseNoRangeNamesNeedsNoMore) {
    Config made = model::world(1);

    made.engines[3].version = "9.9.9";
    install(made);

    EXPECT_FALSE(cfg().engines[3].needsVersion);
}

TEST_F(ListsBridgeTest, ADownloadedEngineNeedsNoVersion) {
    Config made = model::world(1);

    made.engines[3].managed = true;
    install(made);

    EXPECT_FALSE(cfg().engines[3].needsVersion);
}

TEST_F(ListsBridgeTest, AFileInThePortsFolderStillNeedsAVersion) {
    Config made = model::world(1);

    made.engines[3].file = ttk::Format::from_path(Catalog::directory() / "dsda-doom" / "dsda-doom");
    install(made);

    EXPECT_TRUE(cfg().engines[3].needsVersion);
    EXPECT_FALSE(cfg().engines[3].managed);
}

TEST_F(ListsBridgeTest, AnEngineRowIsCopiedOutOrEmpty) {
    install(model::world(1));

    EXPECT_EQ(ListsBridge::engine(1).name, config().engines[1].name);
    EXPECT_EQ(ListsBridge::engine(7).file, "");
    EXPECT_EQ(ListsBridge::engine(-1).file, "");
}

TEST_F(ListsBridgeTest, RemovingAnEngineTakesItOffTheList) {
    install(model::world(1));

    lists().removeEngine(1);

    EXPECT_EQ(config().engines.size(), 6U);
    EXPECT_EQ(config().findEngine("Chocolate"), nullptr);
    EXPECT_EQ(cfg().engineNames.size(), 6U);
    EXPECT_EQ(cfg().engineBadges.size(), 6U);
}

TEST_F(ListsBridgeTest, RemovingTheLibrarysEngineClearsTheSetting) {
    Config made = model::world(1);

    made.general.gameEngine = "Chocolate";
    install(made);

    lists().removeEngine(1);

    EXPECT_EQ(config().general.gameEngine, "");
    EXPECT_EQ(cfg().gameEngine, "");
}

TEST_F(ListsBridgeTest, RemovingAnotherEngineKeepsTheLibrarysEngine) {
    Config made = model::world(1);

    made.general.gameEngine = "Chocolate";
    install(made);

    lists().removeEngine(2);

    EXPECT_EQ(config().general.gameEngine, "Chocolate");
}

TEST_F(ListsBridgeTest, RemovingAnEngineTakesItOutOfEveryProfileThatNamedIt) {
    install(model::world(2));

    lists().removeEngine(0);

    for (const Profile &each : config().profiles) {
        EXPECT_EQ(each.engine, "") << each.name;
    }

    EXPECT_FALSE(cfg().profileCards.front().ready);
}

TEST_F(ListsBridgeTest, RemovingARowThatIsNotThereDoesNothing) {
    install(model::world(1));

    lists().removeEngine(-1);
    lists().removeEngine(7);
    lists().removeIwad(2);
    lists().removeFile(0);

    EXPECT_EQ(config().engines.size(), 7U);
    EXPECT_EQ(config().iwads.size(), 2U);
}

TEST_F(ListsBridgeTest, RemovingAnEngineIsSaved) {
    install(model::world(1));

    lists().removeEngine(1);
    settle();

    EXPECT_EQ(saved().engines.size(), 6U);
}

TEST_F(ListsBridgeTest, MovingAnEngineReordersTheList) {
    install(model::world(1));

    lists().moveEngine(0, 2);

    EXPECT_EQ(cfg().engineNames[0], "Chocolate");
    EXPECT_EQ(cfg().engineNames[1], "PrBoom");
    EXPECT_EQ(cfg().engineNames[2], "GZDoom");
}

TEST_F(ListsBridgeTest, MovingAnEngineIsSaved) {
    install(model::world(1));

    lists().moveEngine(6, 0);
    settle();

    EXPECT_EQ(saved().engines.front().name, "Vanilla");
}

TEST_F(ListsBridgeTest, AddedFilesGoOnTheEndLoaded) {
    loaded();

    lists().addFiles({"/addons/five.wad", "/addons/six.wad"});

    EXPECT_EQ(files().size(), 6U);
    EXPECT_EQ(files().back(), "/addons/six.wad");
    EXPECT_TRUE(cfg().files.back().loaded);
    EXPECT_EQ(cfg().enabledCount, 5);
}

TEST_F(ListsBridgeTest, AFileAlreadyListedIsNotAddedAgainAndKeepsItsState) {
    loaded();

    lists().addFiles({"/addons/two.wad", "/addons/five.wad", "/addons/./one.wad"});

    EXPECT_EQ(files().size(), 5U);
    EXPECT_EQ(files().back(), "/addons/five.wad");
    EXPECT_FALSE(config().activeProfile().files[1].enabled);
}

TEST_F(ListsBridgeTest, TheSamePathTwiceInOneCallIsAddedOnce) {
    loaded();

    lists().insertFiles({"/addons/five.wad", "/addons/five.wad"}, 1);

    EXPECT_EQ(files().size(), 5U);
    EXPECT_EQ(files()[1], "/addons/five.wad");
}

#ifndef _WIN32
TEST_F(ListsBridgeTest, AFileListedInAnotherCaseIsAddedOnAPosixBuild) {
    loaded();

    lists().addFiles({"/addons/ONE.wad"});

    EXPECT_EQ(files().size(), 5U);
    EXPECT_EQ(files().back(), "/addons/ONE.wad");
}
#endif

TEST_F(ListsBridgeTest, AddingOnlyListedFilesChangesNothing) {
    loaded();

    const int before = cfg().rev;

    lists().addFiles({"/addons/one.wad"});

    EXPECT_EQ(files().size(), 4U);
    EXPECT_EQ(cfg().rev, before);
}

TEST_F(ListsBridgeTest, InsertedFilesGoInFrontOfTheRowAndOutOfRangeClamps) {
    loaded();

    lists().insertFiles({"/addons/a.wad", "/addons/b.wad"}, 1);

    EXPECT_EQ(files(),
              (std::vector<std::string>{"/addons/one.wad",
                                        "/addons/a.wad",
                                        "/addons/b.wad",
                                        "/addons/two.wad",
                                        "/addons/three.pk3",
                                        "/addons/four.deh"}));
    EXPECT_TRUE(cfg().files[1].loaded);

    lists().insertFiles({"/addons/first.wad"}, -3);
    lists().insertFiles({"/addons/last.wad"}, 99);

    EXPECT_EQ(files().front(), "/addons/first.wad");
    EXPECT_EQ(files().back(), "/addons/last.wad");
}

TEST_F(ListsBridgeTest, AddedFilesShowOnTheProfilesCard) {
    loaded();

    lists().addFiles({"/addons/five.wad"});

    EXPECT_EQ(cfg().profileCards.front().files, 5);
    EXPECT_EQ(cfg().profileCards.front().loaded, 4);
}

TEST_F(ListsBridgeTest, AddingFilesMovesTheRevision) {
    loaded();

    const int before = cfg().rev;

    lists().addFiles({"/addons/five.wad"});

    EXPECT_GT(cfg().rev, before);
}

TEST_F(ListsBridgeTest, TogglingAFileMovesTheFilesRevision) {
    loaded();

    const int before = cfg().filesRev;

    lists().setFileEnabled(1, true);

    EXPECT_GT(cfg().filesRev, before);
}

TEST_F(ListsBridgeTest, PushingTheSameFilesLeavesTheFilesRevision) {
    loaded();

    const int before = cfg().filesRev;

    lists().push();

    EXPECT_EQ(cfg().filesRev, before);
}

TEST_F(ListsBridgeTest, AddingFilesIsSaved) {
    loaded();

    lists().addFiles({"/addons/five.wad"});
    settle();

    EXPECT_EQ(saved().activeProfile().files.size(), 5U);
}

TEST_F(ListsBridgeTest, RemovingAFileTakesThatOne) {
    loaded();

    lists().removeFile(1);

    EXPECT_EQ(files(), (std::vector<std::string>{"/addons/one.wad", "/addons/three.pk3", "/addons/four.deh"}));
    EXPECT_EQ(cfg().files.size(), 3U);
}

TEST_F(ListsBridgeTest, RemovingAFileOutOfRangeDoesNothing) {
    loaded();

    lists().removeFile(4);
    lists().removeFile(-1);

    EXPECT_EQ(files().size(), 4U);
}

TEST_F(ListsBridgeTest, ClearingTheFilesEmptiesTheList) {
    loaded();

    lists().clearFiles();

    EXPECT_TRUE(files().empty());
    EXPECT_TRUE(cfg().files.empty());
    EXPECT_EQ(cfg().enabledCount, 0);
    EXPECT_EQ(cfg().profileCards.front().files, 0);
}

TEST_F(ListsBridgeTest, ClearingTheFilesIsSaved) {
    loaded();

    lists().clearFiles();
    settle();

    EXPECT_TRUE(saved().activeProfile().files.empty());
}

TEST_F(ListsBridgeTest, MovingAFileReordersTheList) {
    loaded();

    lists().moveFile(3, 0);

    EXPECT_EQ(
            files(),
            (std::vector<std::string>{"/addons/four.deh", "/addons/one.wad", "/addons/two.wad", "/addons/three.pk3"}));
    EXPECT_EQ(cfg().files[0].name, "four.deh");
}

TEST_F(ListsBridgeTest, TurningAFileOffLeavesItListed) {
    loaded();

    lists().setFileEnabled(0, false);

    EXPECT_EQ(files().size(), 4U);
    EXPECT_FALSE(config().activeProfile().files[0].enabled);
    EXPECT_FALSE(cfg().files[0].loaded);
    EXPECT_EQ(cfg().enabledCount, 2);
    EXPECT_EQ(cfg().profileCards.front().loaded, 2);
}

TEST_F(ListsBridgeTest, TurningAFileOnLoadsIt) {
    loaded();

    lists().setFileEnabled(1, true);

    EXPECT_TRUE(cfg().files[1].loaded);
    EXPECT_EQ(cfg().enabledCount, 4);
}

TEST_F(ListsBridgeTest, TurningAFileOutOfRangeDoesNothing) {
    loaded();

    lists().setFileEnabled(4, false);

    EXPECT_EQ(cfg().enabledCount, 3);
}

TEST_F(ListsBridgeTest, TurningAFileOffIsSaved) {
    loaded();

    lists().setFileEnabled(0, false);
    settle();

    EXPECT_FALSE(saved().activeProfile().files[0].enabled);
}

TEST_F(ListsBridgeTest, TurningAFileOffChangesTheLaunch) {
    Config made = model::world(2);

    model::write(folder() / "one.wad");
    model::write(folder() / "three.pk3");
    made.profiles.front().files = {
            {.file = (folder() / "one.wad").string(), .enabled = true},
            {.file = (folder() / "three.pk3").string(), .enabled = true},
    };
    install(made);

    lists().setFileEnabled(0, false);
    preview();

    EXPECT_FALSE(cfg().commandLine.contains("one.wad")) << cfg().commandLine;
    EXPECT_TRUE(cfg().commandLine.contains("three.pk3")) << cfg().commandLine;
}

TEST_F(ListsBridgeTest, AnAddedGameIsNamedFromWhatItSays) {
    install(model::world(0));

    lists().addIwads({tests::Corpus::iwad().string()});

    EXPECT_EQ(config().iwads.back().name, "Test Doom");
    EXPECT_EQ(cfg().iwadNames.back(), "Test Doom");
}

TEST_F(ListsBridgeTest, TheSameGameAddedTwiceTakesASecondName) {
    install(model::world(0));

    lists().addIwads({tests::Corpus::iwad().string(), tests::Corpus::iwad().string()});

    EXPECT_EQ(config().iwads.back().name, "Test Doom (2)");
}

TEST_F(ListsBridgeTest, AFolderIsNotAddedAsAGame) {
    install(model::world(0));

    lists().addIwads({folder().string()});

    EXPECT_EQ(config().iwads.size(), 2U);

    const std::vector<ttk::Notice> warnings = said(ttk::Severity::Warning);

    ASSERT_EQ(warnings.size(), 1U);
    EXPECT_TRUE(warnings.front().body.contains("was left out")) << warnings.front().body;
}

TEST_F(ListsBridgeTest, AnEmptyPathIsSkipped) {
    install(model::world(0));

    lists().addIwads({"", tests::Corpus::iwad().string()});

    EXPECT_EQ(config().iwads.size(), 3U);
    EXPECT_TRUE(said(ttk::Severity::Warning).empty());
}

TEST_F(ListsBridgeTest, AddingGamesIsSaved) {
    install(model::world(0));

    lists().addIwads({tests::Corpus::iwad().string()});
    settle();

    EXPECT_EQ(saved().iwads.size(), 3U);
}

TEST_F(ListsBridgeTest, RenamingAGameRenamesItInEveryProfile) {
    install(model::world(2));

    lists().updateIwad(0, "Hell on Earth", "/games/doom2.wad");

    for (const Profile &each : config().profiles) {
        EXPECT_EQ(each.iwad, "Hell on Earth") << each.name;
    }

    EXPECT_EQ(cfg().iwad, "Hell on Earth");
    EXPECT_EQ(cfg().profileCards.front().iwad, "Hell on Earth");
}

TEST_F(ListsBridgeTest, RenamingAGameIsSaved) {
    install(model::world(1));

    lists().updateIwad(0, "Hell on Earth", "/games/doom2.wad");
    settle();

    EXPECT_NE(saved().findIwad("Hell on Earth"), nullptr);
    EXPECT_EQ(saved().activeProfile().iwad, "Hell on Earth");
}

TEST_F(ListsBridgeTest, PointingAGameAtAnotherFileIsSaved) {
    install(model::world(1));

    lists().updateIwad(0, "Doom II", "/games/doom2-new.wad");
    settle();

    ASSERT_NE(saved().findIwad("Doom II"), nullptr);
    EXPECT_EQ(saved().findIwad("Doom II")->file, "/games/doom2-new.wad");
}

TEST_F(ListsBridgeTest, AGameWithNoNameIsNamedFromItsFile) {
    install(model::world(1));

    lists().updateIwad(1, "", tests::Corpus::iwad().string());

    EXPECT_EQ(config().iwads[1].name, "Test Doom");
}

TEST_F(ListsBridgeTest, PointingAGameAtAFolderIsRefused) {
    install(model::world(1));

    lists().updateIwad(0, "Doom II", folder().string());

    EXPECT_EQ(config().iwads.front().file, "/games/doom2.wad");

    const std::vector<ttk::Notice> warnings = said(ttk::Severity::Warning);

    ASSERT_EQ(warnings.size(), 1U);
    EXPECT_TRUE(warnings.front().body.contains("Doom II is unchanged")) << warnings.front().body;
}

TEST_F(ListsBridgeTest, AGameAlreadyOnAFolderCanStillBeRenamed) {
    Config made = model::world(1);

    made.iwads.front().file = folder().string();
    install(made);

    lists().updateIwad(0, "Legacy", folder().string());

    EXPECT_EQ(config().iwads.front().name, "Legacy");
    EXPECT_TRUE(said(ttk::Severity::Warning).empty());
}

TEST_F(ListsBridgeTest, RemovingAGameTakesItOffTheList) {
    install(model::world(1));

    lists().removeIwad(1);

    EXPECT_EQ(cfg().iwadNames, (std::vector<std::string>{"Doom II"}));
}

TEST_F(ListsBridgeTest, RemovingAGameIsSaved) {
    install(model::world(1));

    lists().removeIwad(1);
    settle();

    EXPECT_EQ(saved().iwads.size(), 1U);
}

TEST_F(ListsBridgeTest, RemovingAGameTakesItOutOfEveryProfileThatNamedIt) {
    install(model::world(2));

    lists().removeIwad(0);

    for (const Profile &each : config().profiles) {
        EXPECT_EQ(each.iwad, "") << each.name;
    }
}

TEST_F(ListsBridgeTest, MovingAGameReordersTheList) {
    install(model::world(1));

    lists().moveIwad(1, 0);

    EXPECT_EQ(cfg().iwadNames, (std::vector<std::string>{"Plutonia", "Doom II"}));
}

TEST_F(ListsBridgeTest, MovingAGameIsSaved) {
    install(model::world(1));

    lists().moveIwad(1, 0);
    settle();

    EXPECT_EQ(saved().iwads.front().name, "Plutonia");
}

TEST_F(ListsBridgeTest, GroupedFilesAreLinedUpUnderTheirGroupsWithTheUngroupedLast) {
    install(model::grouped());

    EXPECT_EQ(model::lineFiles(), (std::vector<int>{-1, 1, 3, -1, 0, -1, 2}));
    EXPECT_EQ(cfg().addonLines[2].section, 0);
    EXPECT_EQ(cfg().addonLines[6].section, 2);
    ASSERT_EQ(cfg().fileSections.size(), 3U);

    const State::Section &maps = cfg().fileSections[0];

    EXPECT_EQ(maps,
              (State::Section{.group = 1, .name = "Maps", .first = 0, .count = 2, .loaded = 1, .collapsed = false}));
    EXPECT_EQ(cfg().fileSections[1].first, 3);
    EXPECT_EQ(cfg().fileSections[2].name, "Ungrouped");
    EXPECT_EQ(cfg().fileSections[2].group, 0);
    EXPECT_TRUE(cfg().addonsGrouped);
}

TEST_F(ListsBridgeTest, GroupedFileRowsStayInLoadOrderAndNameTheirGroup) {
    install(model::grouped());

    EXPECT_EQ(
            files(),
            (std::vector<std::string>{"/addons/one.wad", "/addons/two.wad", "/addons/three.pk3", "/addons/four.deh"}));
    EXPECT_EQ(cfg().files[0].group, "Gameplay");
    EXPECT_EQ(cfg().files[1].group, "Maps");
    EXPECT_EQ(cfg().files[2].group, "");
}

TEST_F(ListsBridgeTest, AFoldedGroupShowsItsHeaderAlone) {
    Config made = model::grouped();

    made.profiles.front().fileGroups.front().collapsed = true;
    install(made);

    EXPECT_EQ(model::lineFiles(), (std::vector<int>{-1, -1, 0, -1, 2}));
    EXPECT_TRUE(cfg().fileSections[0].collapsed);
    EXPECT_EQ(cfg().fileSections[0].count, 2);
    EXPECT_EQ(cfg().fileSections[1].first, 1);
}

TEST_F(ListsBridgeTest, WithTheSwitchOffTheLinesAreTheFiles) {
    install(model::grouped(false));

    EXPECT_EQ(model::lineFiles(), (std::vector<int>{0, 1, 2, 3}));
    EXPECT_EQ(cfg().addonLines[0].section, -1);
    EXPECT_TRUE(cfg().fileSections.empty());
    EXPECT_EQ(cfg().files[0].group, "");
    EXPECT_FALSE(cfg().addonsGrouped);
}

TEST_F(ListsBridgeTest, WithNoGroupTheLinesAreTheFiles) {
    Config made = model::grouped();

    made.profiles.front().fileGroups.clear();
    install(made);

    EXPECT_EQ(model::lineFiles(), (std::vector<int>{0, 1, 2, 3}));
    EXPECT_TRUE(cfg().fileSections.empty());
    EXPECT_EQ(cfg().files[0].group, "");
    EXPECT_FALSE(cfg().addonsGrouped);
}

TEST_F(ListsBridgeTest, TheLoadOrderViewLinesUpTheFilesWithTheirGroups) {
    install(model::grouped());

    const int before = cfg().filesRev;

    lists().setAddonView(State::AddonView::LoadOrder);

    EXPECT_EQ(model::lineFiles(), (std::vector<int>{0, 1, 2, 3}));
    EXPECT_TRUE(cfg().fileSections.empty());
    EXPECT_EQ(cfg().files[0].group, "Gameplay");
    EXPECT_TRUE(cfg().addonsGrouped);
    EXPECT_GT(cfg().filesRev, before);

    lists().setAddonView(State::AddonView::Groups);

    EXPECT_EQ(cfg().fileSections.size(), 3U);
}

TEST_F(ListsBridgeTest, TurningTheSwitchOffLinesUpTheFiles) {
    install(model::grouped());

    bridge().settings().setGroupAddons(false);

    EXPECT_EQ(model::lineFiles(), (std::vector<int>{0, 1, 2, 3}));
    EXPECT_TRUE(cfg().fileSections.empty());
}

TEST_F(ListsBridgeTest, SettingAFileGroupLeavesTheLoadOrder) {
    install(model::grouped());

    const int before = cfg().filesRev;

    lists().setFileGroup(2, 1);

    EXPECT_EQ(
            files(),
            (std::vector<std::string>{"/addons/one.wad", "/addons/two.wad", "/addons/three.pk3", "/addons/four.deh"}));
    EXPECT_EQ(config().activeProfile().files[2].group, 1);
    EXPECT_EQ(model::lineFiles(), (std::vector<int>{-1, 1, 2, 3, -1, 0, -1}));
    EXPECT_EQ(cfg().fileSections[2].count, 0);
    EXPECT_GT(cfg().filesRev, before);
}

TEST_F(ListsBridgeTest, SettingAFileGroupIsSaved) {
    install(model::grouped());

    lists().setFileGroup(0, 1);
    settle();

    EXPECT_EQ(saved().activeProfile().files[0].group, 1);
}

TEST_F(ListsBridgeTest, MovingAFileKeepsTheGroups) {
    install(model::grouped());

    lists().moveFile(3, 0);

    std::vector<int> groups;

    for (const FileEntry &entry : config().activeProfile().files) {
        groups.push_back(entry.group);
    }

    EXPECT_EQ(groups, (std::vector<int>{1, 2, 1, 0}));
    EXPECT_EQ(model::lineFiles(), (std::vector<int>{-1, 0, 2, -1, 1, -1, 3}));
}

TEST_F(ListsBridgeTest, FilesDroppedOnAGroupGoToTheEndOfTheLoadOrderInIt) {
    install(model::grouped());

    lists().addFiles({"/addons/five.wad"}, 2);

    EXPECT_EQ(files().back(), "/addons/five.wad");
    EXPECT_EQ(config().activeProfile().files.back().group, 2);
    EXPECT_EQ(model::lineFiles(), (std::vector<int>{-1, 1, 3, -1, 0, 4, -1, 2}));
}

TEST_F(ListsBridgeTest, FilesDroppedOnTheLoadOrderGoInAtTheGapUngrouped) {
    install(model::grouped());
    lists().setAddonView(State::AddonView::LoadOrder);

    lists().insertFiles({"/addons/five.wad"}, 1);

    EXPECT_EQ(files()[1], "/addons/five.wad");
    EXPECT_EQ(config().activeProfile().files[1].group, 0);
    EXPECT_EQ(cfg().files[1].group, "");
}

TEST_F(ListsBridgeTest, AddedFilesAreUngrouped) {
    install(model::grouped());

    lists().addFiles({"/addons/five.wad"});

    EXPECT_EQ(config().activeProfile().files.back().group, 0);
    EXPECT_EQ(cfg().fileSections[2].count, 2);
}

TEST_F(ListsBridgeTest, TickingAGroupWithAFileClearTicksAllOfIt) {
    install(model::grouped());

    const int before = cfg().filesRev;

    lists().tickGroup(1);

    EXPECT_TRUE(config().activeProfile().files[1].enabled);
    EXPECT_TRUE(config().activeProfile().files[3].enabled);
    EXPECT_TRUE(config().activeProfile().files[0].enabled);
    EXPECT_EQ(cfg().fileSections[0].loaded, 2);
    EXPECT_GT(cfg().filesRev, before);
}

TEST_F(ListsBridgeTest, TickingAGroupWithAllTickedClearsIt) {
    install(model::grouped());

    lists().tickGroup(2);

    EXPECT_FALSE(config().activeProfile().files[0].enabled);
    EXPECT_TRUE(config().activeProfile().files[3].enabled);
    EXPECT_EQ(cfg().fileSections[1].loaded, 0);
    EXPECT_EQ(cfg().enabledCount, 2);
}

TEST_F(ListsBridgeTest, TickingTheUngroupedTakesFilesOfAGroupNoneHas) {
    Config made = model::grouped();

    made.profiles.front().files[0].group = 9;
    install(made);

    lists().tickGroup(0);

    EXPECT_FALSE(config().activeProfile().files[0].enabled);
    EXPECT_FALSE(config().activeProfile().files[2].enabled);
    EXPECT_TRUE(config().activeProfile().files[3].enabled);
}

TEST_F(ListsBridgeTest, ClearingTheFilesKeepsTheGroups) {
    install(model::grouped());

    lists().clearFiles();

    EXPECT_TRUE(files().empty());
    EXPECT_EQ(config().activeProfile().fileGroups.size(), 2U);
    EXPECT_EQ(model::lineFiles(), (std::vector<int>{-1, -1, -1}));
}

TEST_F(ListsBridgeTest, ADuplicateCopiesTheFileGroups) {
    install(model::grouped());

    bridge().profile().duplicateProfile();

    const Profile &copy = config().activeProfile();

    EXPECT_EQ(copy.name, "Alpha (2)");
    EXPECT_EQ(copy.fileGroups, config().profiles.front().fileGroups);
    EXPECT_EQ(copy.files[0].group, 2);
    EXPECT_EQ(cfg().fileSections.size(), 3U);
}

TEST_F(ListsBridgeTest, RowOfDescribesOneEntry) {
    install(model::world(1));

    const State::NameRow row = ListsBridge::rowOf(config().engines, 6);

    EXPECT_EQ(row.index, 6);
    EXPECT_EQ(row.name, "Vanilla");
    EXPECT_EQ(row.kind, ".exe");
    EXPECT_TRUE(row.dosbox);
    EXPECT_TRUE(row.missing);
}

}
