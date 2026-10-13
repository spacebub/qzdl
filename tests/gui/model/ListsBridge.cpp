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
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/notices/Notice.h"
#include "ttk/util/Format.h"

#include "Model.h"
#include "core/config/Config.h"
#include "core/ports/Catalog.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/ListsBridge.h"
#include "gui/state/State.h"
#include "support/Corpus.h"

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

    made.profiles.front().files = {{.file = (folder() / "absent.wad").string(), .enabled = true},
                                   {.file = tests::Corpus::addons(1).front(), .enabled = true}};
    install(made);

    EXPECT_TRUE(cfg().files[0].missing);
    EXPECT_FALSE(cfg().files[1].missing);
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

TEST_F(ListsBridgeTest, OnlyADosPortCarriesTheDosBadge) {
    install(model::world(1));

    ASSERT_EQ(cfg().portBadges.size(), cfg().portNames.size());
    EXPECT_EQ(cfg().portBadges.back(), "DOS");
    EXPECT_EQ(cfg().portBadges.front(), "");
    EXPECT_TRUE(cfg().ports.back().dosbox);
    EXPECT_FALSE(cfg().ports.front().dosbox);
}

TEST_F(ListsBridgeTest, APortUnderTheDownloadsIsFetched) {
    Config made = model::world(1);

    made.ports.front().file = ttk::Format::from_path(Catalog::directory() / "gzdoom" / "gzdoom");
    install(made);

    EXPECT_TRUE(cfg().ports[0].fetched);
    EXPECT_FALSE(cfg().ports[1].fetched);
}

TEST_F(ListsBridgeTest, AGameIsNeverFetched) {
    Config made = model::world(1);

    made.iwads.front().file = ttk::Format::from_path(Catalog::directory() / "doom2.wad");
    install(made);

    EXPECT_FALSE(cfg().iwads[0].fetched);
}

TEST_F(ListsBridgeTest, AddingAPortWithNoFileDoesNothing) {
    install(model::world(1));

    EXPECT_EQ(lists().addPort("", "Named", false), "");
    EXPECT_EQ(config().ports.size(), 7U);
}

TEST_F(ListsBridgeTest, AnAddedPortWithNoNameIsNamedAfterItsFile) {
    install(model::world(0));

    EXPECT_EQ(lists().addPort("/opt/prboom-plus", "", false), "PrBoom+");
    EXPECT_EQ(config().ports.back().name, "PrBoom+");
    EXPECT_EQ(cfg().portNames.back(), "PrBoom+");
}

TEST_F(ListsBridgeTest, AnAddedPortTakesANameNoOtherHas) {
    install(model::world(0));

    EXPECT_EQ(lists().addPort("/opt/gz", "gzdoom", false), "gzdoom (2)");
    EXPECT_EQ(lists().addPort("/opt/gz2", "GZDOOM", false), "GZDOOM (3)");
}

TEST_F(ListsBridgeTest, AnAddedPortKeepsItsIdAndDosFlag) {
    install(model::world(0));

    std::ignore = lists().addPort("/opt/doom.exe", "Doom", true, "doom-dos");

    EXPECT_EQ(config().ports.back().portId, "doom-dos");
    EXPECT_TRUE(config().ports.back().dosbox);
    EXPECT_EQ(cfg().portBadges.back(), "DOS");
}

TEST_F(ListsBridgeTest, AddingAPortIsSaved) {
    install(model::world(0));

    std::ignore = lists().addPort("/opt/doom.exe", "Doom", true, "doom-dos");
    settle();

    ASSERT_FALSE(saved().ports.empty());
    EXPECT_EQ(saved().ports.back().portId, "doom-dos");
}

TEST_F(ListsBridgeTest, RenamingAPortRenamesItInEveryProfile) {
    install(model::world(3));

    lists().updatePort(0, "Zandronum", "/ports/gzdoom", false);

    EXPECT_EQ(config().ports.front().name, "Zandronum");

    for (const Profile &each : config().profiles) {
        EXPECT_EQ(each.port, "Zandronum") << each.name;
    }

    EXPECT_EQ(cfg().port, "Zandronum");
}

TEST_F(ListsBridgeTest, RenamingTheLibrarysPortRenamesTheSetting) {
    Config made = model::world(1);

    made.general.gamePort = "GZDoom";
    install(made);

    lists().updatePort(0, "Zandronum", "/ports/gzdoom", false);

    EXPECT_EQ(config().general.gamePort, "Zandronum");
    EXPECT_EQ(cfg().gamePort, "Zandronum");
}

TEST_F(ListsBridgeTest, APortKeepingItsNameIsNotNumbered) {
    install(model::world(1));

    lists().updatePort(0, "GZDoom", "/ports/gzdoom-new", false);

    EXPECT_EQ(config().ports.front().name, "GZDoom");
    EXPECT_EQ(config().ports.front().file, "/ports/gzdoom-new");
}

TEST_F(ListsBridgeTest, APortCannotBeRenamedOntoAnother) {
    install(model::world(1));

    lists().updatePort(0, "Chocolate", "/ports/gzdoom", false);

    EXPECT_EQ(config().ports.front().name, "Chocolate (2)");
}

TEST_F(ListsBridgeTest, MarkingTheActivePortDosTellsTheProfile) {
    install(model::world(1));

    EXPECT_FALSE(cfg().dosPort);

    lists().updatePort(0, "GZDoom", "/ports/gzdoom", true);

    EXPECT_TRUE(cfg().dosPort);
    EXPECT_TRUE(cfg().shelfProfiles.front().dosPort);
}

TEST_F(ListsBridgeTest, UpdatingARowThatIsNotThereDoesNothing) {
    install(model::world(1));

    lists().updatePort(7, "Ghost", "/ports/ghost", false);
    lists().updatePort(-1, "Ghost", "/ports/ghost", false);

    EXPECT_EQ(config().ports.size(), 7U);
    EXPECT_EQ(config().findPort("Ghost"), nullptr);
}

TEST_F(ListsBridgeTest, UpdatingAPortIsSaved) {
    install(model::world(1));

    lists().updatePort(1, "Choco", "/ports/chocolate-doom", false);
    settle();

    EXPECT_NE(saved().findPort("Choco"), nullptr);
}

TEST_F(ListsBridgeTest, RemovingAPortTakesItOffTheList) {
    install(model::world(1));

    lists().removePort(1);

    EXPECT_EQ(config().ports.size(), 6U);
    EXPECT_EQ(config().findPort("Chocolate"), nullptr);
    EXPECT_EQ(cfg().portNames.size(), 6U);
    EXPECT_EQ(cfg().portBadges.size(), 6U);
}

TEST_F(ListsBridgeTest, RemovingTheLibrarysPortClearsTheSetting) {
    Config made = model::world(1);

    made.general.gamePort = "Chocolate";
    install(made);

    lists().removePort(1);

    EXPECT_EQ(config().general.gamePort, "");
    EXPECT_EQ(cfg().gamePort, "");
}

TEST_F(ListsBridgeTest, RemovingAnotherPortKeepsTheLibrarysPort) {
    Config made = model::world(1);

    made.general.gamePort = "Chocolate";
    install(made);

    lists().removePort(2);

    EXPECT_EQ(config().general.gamePort, "Chocolate");
}

TEST_F(ListsBridgeTest, RemovingAPortTakesItOutOfEveryProfileThatNamedIt) {
    install(model::world(2));

    lists().removePort(0);

    for (const Profile &each : config().profiles) {
        EXPECT_EQ(each.port, "") << each.name;
    }

    EXPECT_FALSE(cfg().profileCards.front().ready);
}

TEST_F(ListsBridgeTest, RemovingARowThatIsNotThereDoesNothing) {
    install(model::world(1));

    lists().removePort(-1);
    lists().removePort(7);
    lists().removeIwad(2);
    lists().removeFile(0);

    EXPECT_EQ(config().ports.size(), 7U);
    EXPECT_EQ(config().iwads.size(), 2U);
}

TEST_F(ListsBridgeTest, RemovingAPortIsSaved) {
    install(model::world(1));

    lists().removePort(1);
    settle();

    EXPECT_EQ(saved().ports.size(), 6U);
}

TEST_F(ListsBridgeTest, MovingAPortReordersTheList) {
    install(model::world(1));

    lists().movePort(0, 2);

    EXPECT_EQ(cfg().portNames[0], "Chocolate");
    EXPECT_EQ(cfg().portNames[1], "PrBoom");
    EXPECT_EQ(cfg().portNames[2], "GZDoom");
}

TEST_F(ListsBridgeTest, MovingAPortIsSaved) {
    install(model::world(1));

    lists().movePort(6, 0);
    settle();

    EXPECT_EQ(saved().ports.front().name, "Vanilla");
}

TEST_F(ListsBridgeTest, AddedFilesGoOnTheEndLoaded) {
    loaded();

    lists().addFiles({"/addons/five.wad", "/addons/six.wad"});

    EXPECT_EQ(files().size(), 6U);
    EXPECT_EQ(files().back(), "/addons/six.wad");
    EXPECT_TRUE(cfg().files.back().loaded);
    EXPECT_EQ(cfg().enabledCount, 5);
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
    loaded();

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

TEST_F(ListsBridgeTest, PortsAnswersTheConfigsList) {
    install(model::world(1));

    EXPECT_EQ(&ListsBridge::ports(), &config().ports);
}

TEST_F(ListsBridgeTest, RowOfDescribesOneEntry) {
    install(model::world(1));

    const State::NameRow row = ListsBridge::rowOf(config().ports, 6, true);

    EXPECT_EQ(row.index, 6);
    EXPECT_EQ(row.name, "Vanilla");
    EXPECT_EQ(row.kind, ".exe");
    EXPECT_TRUE(row.dosbox);
    EXPECT_TRUE(row.missing);
}

}
