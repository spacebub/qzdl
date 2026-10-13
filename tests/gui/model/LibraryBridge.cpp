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

#include "ttk/notices/Notice.h"

#include "Model.h"
#include "core/config/Config.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/LibraryBridge.h"
#include "gui/state/State.h"

namespace {

namespace fs = std::filesystem;

class LibraryBridgeTest : public model::Bridged {
protected:
    LibraryBridge &library() const { return bridge().library(); }

    static std::vector<std::string> shelfNames() {
        std::vector<std::string> out;

        for (const State::ProfileCard &card : cfg().shelfProfiles) {
            out.push_back(card.name);
        }

        return out;
    }

    static std::vector<std::string> shelfGames() {
        std::vector<std::string> out;

        for (const State::NameRow &row : cfg().shelfGames) {
            out.push_back(row.name);
        }

        return out;
    }
};

TEST_F(LibraryBridgeTest, AnUnfilteredShelfHoldsEveryProfileAndGame) {
    install(model::world(4));

    EXPECT_EQ(shelfNames(), (std::vector<std::string>{"Alpha", "Bravo", "Charlie", "Delta"}));
    EXPECT_EQ(shelfGames(), (std::vector<std::string>{"Doom II", "Plutonia"}));
}

TEST_F(LibraryBridgeTest, AShelfCardKeepsItsIndexInTheList) {
    install(model::world(4));

    library().setFilter("delta");

    ASSERT_EQ(cfg().shelfProfiles.size(), 1U);
    EXPECT_EQ(cfg().shelfProfiles.front().index, 3);
}

TEST_F(LibraryBridgeTest, TheFilterMatchesAnyPartOfTheNameWhateverTheCase) {
    install(model::world(4));

    library().setFilter("ARL");

    EXPECT_EQ(shelfNames(), (std::vector<std::string>{"Charlie"}));
}

TEST_F(LibraryBridgeTest, TheFilterAppliesToTheGamesToo) {
    install(model::world(2));

    library().setFilter("plu");

    EXPECT_EQ(shelfGames(), (std::vector<std::string>{"Plutonia"}));
    EXPECT_TRUE(shelfNames().empty());
}

TEST_F(LibraryBridgeTest, TheFilterIsPushedForTheFieldToShow) {
    install(model::world(2));

    library().setFilter("bra");

    EXPECT_EQ(cfg().filter, "bra");
}

TEST_F(LibraryBridgeTest, ClearingTheFilterBringsEverythingBack) {
    install(model::world(3));

    library().setFilter("bra");
    library().setFilter("");

    EXPECT_EQ(shelfNames().size(), 3U);
    EXPECT_EQ(shelfGames().size(), 2U);
}

TEST_F(LibraryBridgeTest, AFilterThatMatchesNothingEmptiesTheShelf) {
    install(model::world(3));

    library().setFilter("zzz");

    EXPECT_TRUE(cfg().shelfProfiles.empty());
    EXPECT_TRUE(cfg().shelfGames.empty());
}

TEST_F(LibraryBridgeTest, TheShelfRevisionMovesWhenTheShelfChanges) {
    install(model::world(3));

    const int before = cfg().shelfRev;

    library().setFilter("bra");

    EXPECT_GT(cfg().shelfRev, before);
}

TEST_F(LibraryBridgeTest, PushingAnUnchangedShelfLeavesItsRevision) {
    install(model::world(3));

    const int before = cfg().shelfRev;

    library().pushShelf();
    library().setFilter("");

    EXPECT_EQ(cfg().shelfRev, before);
}

TEST_F(LibraryBridgeTest, AChangeNoCardShowsLeavesTheShelf) {
    install(model::world(3));

    const int before = cfg().shelfRev;

    bridge().profile().setExtra("-nomusic");
    bridge().profile().setSkill(4);

    EXPECT_EQ(cfg().shelfRev, before);
    EXPECT_GT(cfg().rev, 0);
}

TEST_F(LibraryBridgeTest, AChangeACardShowsMovesTheShelf) {
    install(model::world(3));

    const int before = cfg().shelfRev;

    bridge().profile().setPort("Chocolate");

    EXPECT_GT(cfg().shelfRev, before);
    EXPECT_EQ(cfg().shelfProfiles.front().port, "Chocolate");
}

TEST_F(LibraryBridgeTest, TheGameRevisionMovesWithWhatALibraryLaunchReads) {
    install(model::world(2));

    const int before = cfg().gameRev;

    bridge().settings().setAlwaysAdd("-nosound");

    EXPECT_GT(cfg().gameRev, before);
}

TEST_F(LibraryBridgeTest, TheGameRevisionStaysForWhatALibraryLaunchIgnores) {
    install(model::world(2));

    const int before = cfg().gameRev;

    bridge().profile().setSkill(3);
    bridge().profile().setWarp("MAP02");
    library().pushGameRev();

    EXPECT_EQ(cfg().gameRev, before);
}

TEST_F(LibraryBridgeTest, TheGameRevisionMovesWithTheActiveProfilesPort) {
    install(model::world(2));

    const int before = cfg().gameRev;

    bridge().profile().setPort("Chocolate");

    EXPECT_GT(cfg().gameRev, before);
}

TEST_F(LibraryBridgeTest, TheGameRevisionMovesWhenAGameIsAdded) {
    install(model::world(2));

    const int before = cfg().gameRev;

    bridge().lists().addIwads({"/games/tnt.wad"});

    EXPECT_GT(cfg().gameRev, before);
}

TEST_F(LibraryBridgeTest, AGameLaunchesOnTheActiveProfilesPortByDefault) {
    install(model::world(1));

    const std::string line = LibraryBridge::gameCommandLine("Plutonia");

    EXPECT_TRUE(line.starts_with("/ports/gzdoom")) << line;
    EXPECT_TRUE(line.contains("-iwad /games/plutonia.wad")) << line;
}

TEST_F(LibraryBridgeTest, AGameLaunchesOnTheLibrarysPortWhenOneIsSet) {
    Config made = model::world(1);

    made.general.gamePort = "Chocolate";
    install(made);

    EXPECT_TRUE(LibraryBridge::gameCommandLine("Doom II").starts_with("/ports/chocolate-doom"));
}

TEST_F(LibraryBridgeTest, ALibraryPortNoLongerListedCountsAsUnset) {
    Config made = model::world(1);

    made.general.gamePort = "Gone";
    install(made);

    EXPECT_TRUE(LibraryBridge::gameCommandLine("Doom II").starts_with("/ports/gzdoom"));
}

TEST_F(LibraryBridgeTest, AGameLaunchCarriesNoneOfTheProfilesFiles) {
    Config made = model::world(1);

    made.profiles.front().files = {{.file = "/addons/mod.wad", .enabled = true}};
    made.profiles.front().skill = 4;
    install(made);

    const std::string line = LibraryBridge::gameCommandLine("Doom II");

    EXPECT_FALSE(line.contains("mod.wad")) << line;
    EXPECT_FALSE(line.contains("-skill")) << line;
}

TEST_F(LibraryBridgeTest, AGameLaunchUsesThePortsOwnConfig) {
    Config made = model::world(1);

    made.general.profileConfigs = true;
    install(made);
    preview();

    EXPECT_TRUE(cfg().commandLine.contains("-config")) << cfg().commandLine;
    EXPECT_FALSE(LibraryBridge::gameCommandLine("Doom II").contains("-config"));
}

TEST_F(LibraryBridgeTest, AGameLaunchCarriesTheArgumentsAlwaysAdded) {
    Config made = model::world(1);

    made.general.alwaysAdd = "-nosound";
    install(made);

    EXPECT_TRUE(LibraryBridge::gameCommandLine("Doom II").contains("-nosound"));
}

TEST_F(LibraryBridgeTest, WithNoProfileTheLibrarysPortStillLaunches) {
    Config made = model::world(0);

    made.general.gamePort = "Woof";
    install(made);

    EXPECT_TRUE(LibraryBridge::gameCommandLine("Doom II").starts_with("/ports/woof"));
}

TEST_F(LibraryBridgeTest, LaunchingAGameWithNoPortIsRefused) {
    install(model::world(0));

    library().launchGame("Doom II");

    EXPECT_EQ(said(ttk::Severity::Error).size(), 1U);
    EXPECT_EQ(runs().stateOf(ConfigBridge::gameKey("Doom II")), State::RunState::Failed);
}

TEST_F(LibraryBridgeTest, LaunchingAGameRunsUnderTheGamesKey) {
    Config made = model::world(1);
    const fs::path script = folder() / "bin" / "woof";

    model::write(script, "#!/bin/sh\n");
    fs::permissions(script, fs::perms::owner_all, fs::perm_options::replace);

    made.ports.front().file = script.string();
    install(made);

    library().launchGame("Plutonia");

    EXPECT_EQ(runs().stateOf(ConfigBridge::gameKey("Plutonia")), State::RunState::Launching);
    EXPECT_EQ(runs().stateOf(ConfigBridge::profileKey("id-alpha")), State::RunState::None);

    waitOut(ConfigBridge::gameKey("Plutonia"));
}

}
