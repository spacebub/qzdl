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
#include "gui/model/GroupsBridge.h"
#include "gui/model/LibraryBridge.h"
#include "gui/state/State.h"
#include "support/Spawn.h"

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

    // Six profiles in Megawads, Speedruns, a folded Co-op, an empty group and none, and Plutonia in Doom.
    static Config shelved(const bool on = true) {
        Config made = model::world(6);
        const std::vector<int> groups = {2, 1, 0, 1, 3, 2};

        made.general.groupLibrary = on;
        made.profileGroups = {{.id = 1, .name = "Megawads", .collapsed = false},
                              {.id = 2, .name = "Speedruns", .collapsed = false},
                              {.id = 3, .name = "Co-op", .collapsed = true},
                              {.id = 4, .name = "Empty", .collapsed = false}};
        made.gameGroups = {{.id = 1, .name = "Doom", .collapsed = false}};
        made.iwads[1].group = 1;

        for (size_t at = 0; at < groups.size(); ++at) {
            made.profiles[at].group = groups[at];
        }

        return made;
    }

    static std::vector<std::string> profileNames() {
        std::vector<std::string> out;

        for (const Profile &profile : config().profiles) {
            out.push_back(profile.name);
        }

        return out;
    }

    static std::vector<std::string> gameNames() {
        std::vector<std::string> out;

        for (const GameEntry &game : config().iwads) {
            out.push_back(game.name);
        }

        return out;
    }

    static State::Section
    section(const int group, const std::string &name, const int first, const int count, const bool collapsed = false) {
        return {.group = group, .name = name, .first = first, .count = count, .loaded = 0, .collapsed = collapsed};
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

    bridge().profile().setEngine("Chocolate");

    EXPECT_GT(cfg().shelfRev, before);
    EXPECT_EQ(cfg().shelfProfiles.front().engine, "Chocolate");
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

TEST_F(LibraryBridgeTest, TheGameRevisionMovesWithTheActiveProfilesEngine) {
    install(model::world(2));

    const int before = cfg().gameRev;

    bridge().profile().setEngine("Chocolate");

    EXPECT_GT(cfg().gameRev, before);
}

TEST_F(LibraryBridgeTest, TheGameRevisionMovesWhenAGameIsAdded) {
    install(model::world(2));

    const int before = cfg().gameRev;

    bridge().lists().addIwads({"/games/tnt.wad"});

    EXPECT_GT(cfg().gameRev, before);
}

TEST_F(LibraryBridgeTest, AGameLaunchesOnTheActiveProfilesEngineByDefault) {
    install(model::world(1));

    const std::string line = LibraryBridge::gameCommandLine("Plutonia");

    EXPECT_TRUE(line.starts_with("/ports/gzdoom")) << line;
    EXPECT_TRUE(line.contains("-iwad /games/plutonia.wad")) << line;
}

TEST_F(LibraryBridgeTest, AGameLaunchesOnTheLibrarysEngineWhenOneIsSet) {
    Config made = model::world(1);

    made.general.gameEngine = "Chocolate";
    install(made);

    EXPECT_TRUE(LibraryBridge::gameCommandLine("Doom II").starts_with("/ports/chocolate-doom"));
}

TEST_F(LibraryBridgeTest, ALibraryEngineNoLongerListedCountsAsUnset) {
    Config made = model::world(1);

    made.general.gameEngine = "Gone";
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

TEST_F(LibraryBridgeTest, WithNoProfileTheLibrarysEngineStillLaunches) {
    Config made = model::world(0);

    made.general.gameEngine = "Woof";
    install(made);

    EXPECT_TRUE(LibraryBridge::gameCommandLine("Doom II").starts_with("/ports/woof"));
}

TEST_F(LibraryBridgeTest, LaunchingAGameWithNoEngineIsRefused) {
    install(model::world(0));

    library().launchGame("Doom II");

    EXPECT_EQ(said(ttk::Severity::Error).size(), 1U);
    EXPECT_EQ(runs().stateOf(ConfigBridge::gameKey("Doom II")), State::RunState::Failed);
}

TEST_F(LibraryBridgeTest, LaunchingAGameRunsUnderTheGamesKey) {
    Config made = model::world(1);
    const fs::path script = tests::Spawn::port(folder() / "bin", "woof", {});

    made.engines.front().file = script.string();
    install(made);

    library().launchGame("Plutonia");

    EXPECT_EQ(runs().stateOf(ConfigBridge::gameKey("Plutonia")), State::RunState::Launching);
    EXPECT_EQ(runs().stateOf(ConfigBridge::profileKey("id-alpha")), State::RunState::None);

    waitOut(ConfigBridge::gameKey("Plutonia"));
}

TEST_F(LibraryBridgeTest, AGroupedShelfShowsTheGroupsInOrderWithTheRestLast) {
    install(shelved());

    EXPECT_EQ(shelfNames(), (std::vector<std::string>{"Bravo", "Delta", "Alpha", "Foxtrot", "Charlie"}));
    EXPECT_EQ(cfg().shelfProfileSections,
              (std::vector<State::Section>{section(1, "Megawads", 0, 2),
                                           section(2, "Speedruns", 2, 2),
                                           section(3, "Co-op", 4, 1, true),
                                           section(4, "Empty", 4, 0),
                                           section(0, "Ungrouped", 4, 1)}));
    EXPECT_EQ(shelfGames(), (std::vector<std::string>{"Plutonia", "Doom II"}));
    EXPECT_EQ(cfg().shelfGameSections,
              (std::vector<State::Section>{section(1, "Doom", 0, 1), section(0, "Ungrouped", 1, 1)}));
}

TEST_F(LibraryBridgeTest, AShelfNotGroupedHasNoSectionsAndTheListOrder) {
    install(shelved(false));

    EXPECT_EQ(shelfNames(), (std::vector<std::string>{"Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot"}));
    EXPECT_TRUE(cfg().shelfProfileSections.empty());
    EXPECT_TRUE(cfg().shelfGameSections.empty());
}

TEST_F(LibraryBridgeTest, TurningTheSwitchOnGroupsTheShelf) {
    install(shelved(false));

    bridge().settings().setGroupLibrary(true);

    EXPECT_EQ(cfg().shelfProfileSections.size(), 5U);
}

TEST_F(LibraryBridgeTest, AFilterHidesAGroupWithNoMatchButKeepsTheRest) {
    install(shelved());

    library().setFilter("a");

    EXPECT_EQ(shelfNames(), (std::vector<std::string>{"Bravo", "Delta", "Alpha", "Charlie"}));
    EXPECT_EQ(cfg().shelfProfileSections,
              (std::vector<State::Section>{
                      section(1, "Megawads", 0, 2), section(2, "Speedruns", 2, 1), section(0, "Ungrouped", 3, 1)}));

    library().setFilter("plu");

    EXPECT_EQ(cfg().shelfGameSections,
              (std::vector<State::Section>{section(1, "Doom", 0, 1), section(0, "Ungrouped", 1, 0)}));
}

TEST_F(LibraryBridgeTest, AFilterShowsTheMatchesInAFoldedGroup) {
    install(shelved());

    library().setFilter("echo");

    EXPECT_EQ(shelfNames(), (std::vector<std::string>{"Echo"}));
    EXPECT_EQ(cfg().shelfProfileSections,
              (std::vector<State::Section>{section(3, "Co-op", 0, 1), section(0, "Ungrouped", 1, 0)}));
}

TEST_F(LibraryBridgeTest, ADropAlreadyBetweenItsNeighboursOnlyTakesTheGroup) {
    install(shelved());

    library().dropProfile(4, 0, 1);

    EXPECT_EQ(profileNames(), (std::vector<std::string>{"Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot"}));
    EXPECT_EQ(config().profiles[2].group, 1);
    EXPECT_EQ(shelfNames(), (std::vector<std::string>{"Bravo", "Charlie", "Delta", "Alpha", "Foxtrot"}));
}

TEST_F(LibraryBridgeTest, ADropEarlierMovesTheCardUp) {
    install(shelved());

    library().dropProfile(3, 0, 0);

    EXPECT_EQ(profileNames(), (std::vector<std::string>{"Alpha", "Foxtrot", "Bravo", "Charlie", "Delta", "Echo"}));
    EXPECT_EQ(config().profiles[1].group, 1);
}

TEST_F(LibraryBridgeTest, ADropLaterMovesTheCardDown) {
    install(shelved());

    library().dropProfile(2, 0, 2);

    EXPECT_EQ(profileNames(), (std::vector<std::string>{"Bravo", "Charlie", "Delta", "Alpha", "Echo", "Foxtrot"}));
    EXPECT_EQ(shelfNames(), (std::vector<std::string>{"Bravo", "Delta", "Alpha", "Foxtrot", "Charlie"}));
}

TEST_F(LibraryBridgeTest, ADropAtTheEndOfItsOwnGroupMovesBehindTheLast) {
    install(shelved());

    library().dropProfile(0, 0, 1);

    EXPECT_EQ(profileNames(), (std::vector<std::string>{"Alpha", "Charlie", "Delta", "Bravo", "Echo", "Foxtrot"}));
    EXPECT_EQ(config().profiles[3].group, 1);
}

TEST_F(LibraryBridgeTest, ADropOnAFoldedBandGoesToTheEndOfThatGroup) {
    install(shelved());

    library().dropProfile(4, 2, 1);

    EXPECT_EQ(profileNames(), (std::vector<std::string>{"Alpha", "Bravo", "Delta", "Echo", "Charlie", "Foxtrot"}));
    EXPECT_EQ(config().profiles[4].group, 3);
    EXPECT_EQ(shelfNames(), (std::vector<std::string>{"Bravo", "Delta", "Alpha", "Foxtrot"}));
}

TEST_F(LibraryBridgeTest, ADropIntoAnEmptyGroupTakesItWithoutMoving) {
    install(shelved());

    library().dropProfile(2, 3, 0);

    EXPECT_EQ(profileNames(), (std::vector<std::string>{"Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot"}));
    EXPECT_EQ(config().profiles[0].group, 4);
    EXPECT_EQ(cfg().shelfProfileSections[3], section(4, "Empty", 3, 1));
}

TEST_F(LibraryBridgeTest, AGameDroppedIntoAGroupJoinsIt) {
    install(shelved());

    library().dropIwad(1, 0, 0);

    EXPECT_EQ(gameNames(), (std::vector<std::string>{"Doom II", "Plutonia"}));
    EXPECT_EQ(config().iwads[0].group, 1);
    EXPECT_EQ(shelfGames(), (std::vector<std::string>{"Doom II", "Plutonia"}));
}

TEST_F(LibraryBridgeTest, ADropIsSaved) {
    install(shelved());

    library().dropProfile(3, 0, 0);
    shell.advance(model::AUTOSAVE + 0.1);

    EXPECT_EQ(saved().profiles[1].name, "Foxtrot");
    EXPECT_EQ(saved().profiles[1].group, 1);
}

TEST_F(LibraryBridgeTest, WorkingTheGroupsNeverReordersTheShelf) {
    install(shelved());

    using List = GroupsBridge::List;
    GroupsBridge &groups = bridge().groups();
    const std::vector<std::string> profiles = profileNames();
    const std::vector<std::string> games = gameNames();

    for (const List list : {List::Profiles, List::Games}) {
        groups.add(list, "New");
        groups.rename(list, 1, "Renamed");
        groups.fold(list, 1, true);
        groups.move(list, 1, 1);
        groups.remove(list, 1);
    }

    EXPECT_EQ(profileNames(), profiles);
    EXPECT_EQ(gameNames(), games);
}

}
