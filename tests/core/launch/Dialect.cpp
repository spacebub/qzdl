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
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Launch.h"
#include "core/launch/Dialect.h"

namespace {

using Dialect::Complevels;
using Dialect::Netgames;
using Dialect::SaveNames;

Dialect::Port named(const char *program) {
    return Dialect::of(std::filesystem::path("/games") / program);
}

bool holds(const std::vector<int> &levels, const int level) {
    return std::ranges::find(levels, level) != levels.end();
}

}

TEST(DialectOf, AnUnknownNameIsTakenForTheZdoomFamilyAndMarkedAGuess) {
    const Dialect::Port speaks = named("mystery-engine");

    EXPECT_FALSE(speaks.recognised);
    EXPECT_TRUE(speaks.iwad);
    EXPECT_EQ(speaks.save, "-savedir");
    EXPECT_EQ(speaks.loads, SaveNames::name);
    EXPECT_EQ(speaks.netgame, Netgames::zdoom);
    EXPECT_TRUE(speaks.exec);
    EXPECT_TRUE(speaks.map);
}

TEST(DialectOf, GzdoomIsTheZdoomFamilyLoadingSavesByName) {
    const Dialect::Port speaks = named("gzdoom");

    EXPECT_EQ(speaks.loads, SaveNames::name);
    EXPECT_EQ(speaks.saveExt, ".zds");
    EXPECT_EQ(speaks.deh, "-deh");
    EXPECT_EQ(speaks.bex, "-bex");
    EXPECT_TRUE(speaks.netmode);
    EXPECT_FALSE(speaks.dos);
}

TEST(DialectOf, OnlyTheWholeNameZdoomLoadsSavesByPath) {
    EXPECT_EQ(named("zdoom").loads, SaveNames::path);
    EXPECT_EQ(named("zandronum").loads, SaveNames::path);
    EXPECT_EQ(named("gzdoom").loads, SaveNames::name);
    EXPECT_EQ(named("lzdoom").loads, SaveNames::name);
}

TEST(DialectOf, UzdoomHasNoNetmode) {
    EXPECT_FALSE(named("uzdoom").netmode);
    EXPECT_FALSE(Dialect::net(named("uzdoom")).netmode);
}

TEST(DialectOf, TheNameIsReadWithoutItsExtensionOrCase) {
    EXPECT_EQ(named("DSDA-Doom.exe").complevel, Complevels::dsda);
    EXPECT_EQ(named("Woof.AppImage").complevel, Complevels::woof);
}

TEST(DialectOf, ChocolateTakesASecondConfigAndHostsItsOwnLobby) {
    const Dialect::Port speaks = named("chocolate-doom");

    EXPECT_TRUE(speaks.extraConfig);
    EXPECT_EQ(speaks.netgame, Netgames::chocolate);
    EXPECT_EQ(speaks.loads, SaveNames::slot);
    EXPECT_FALSE(speaks.levelstat);
}

TEST(DialectOf, CrispyIsChocolateThatWritesALevelstat) {
    const Dialect::Port speaks = named("crispy-doom");

    EXPECT_TRUE(speaks.extraConfig);
    EXPECT_TRUE(speaks.levelstat);
}

TEST(DialectOf, TheWoofLineSharesOneComplevelTable) {
    for (const char *program : {"woof", "nugget-doom", "cherry-doom"}) {
        const Dialect::Port speaks = named(program);

        EXPECT_EQ(speaks.complevel, Complevels::woof) << program;
        EXPECT_EQ(speaks.netgame, Netgames::chocolate) << program;
        EXPECT_EQ(speaks.save, "-save") << program;
    }
}

TEST(DialectOf, DsdaLoadsNoSaveAndPlaysNoNetgame) {
    const Dialect::Port speaks = named("dsda-doom");

    EXPECT_EQ(speaks.loads, SaveNames::none);
    EXPECT_EQ(speaks.netgame, Netgames::none);
    EXPECT_TRUE(speaks.levelstat);
}

TEST(DialectOf, ThePrboomLineOnlyJoinsAndPatchesBexThroughDeh) {
    for (const char *program : {"prboom-plus", "glboom-plus", "rude"}) {
        const Dialect::Port speaks = named(program);

        EXPECT_EQ(speaks.netgame, Netgames::prboom) << program;
        EXPECT_EQ(speaks.complevel, Complevels::prboom) << program;
        EXPECT_EQ(speaks.bex, "-deh") << program;
        EXPECT_FALSE(speaks.exec) << program;
    }
}

TEST(DialectOf, BoomKeepsItsOwnConfigWhereMbfTakesOne) {
    EXPECT_FALSE(named("boom").configFile);
    EXPECT_TRUE(named("mbf").configFile);
    EXPECT_TRUE(named("winmbf").configFile);
    EXPECT_EQ(named("boom").complevel, Complevels::none);
}

TEST(DialectOf, HelionCannotRespawnOrTimeADemo) {
    const Dialect::Port speaks = named("helion");

    EXPECT_FALSE(speaks.respawn);
    EXPECT_FALSE(speaks.timedemo);
    EXPECT_EQ(speaks.saveExt, ".hsg");
}

TEST(DialectOf, EternityHasNoNetgame) {
    EXPECT_EQ(named("eternity").netgame, Netgames::none);
    EXPECT_EQ(named("eternity").deh, "-deh");
    EXPECT_EQ(named("eternity").bex, "-bex");
}

TEST(DialectOf, ThePlainDosReleasesTakeNoIwadNoPatchAndNoSaveFolder) {
    for (const char *program : {"doom", "doom2", "doomu", "doom95", "dosdoom"}) {
        const Dialect::Port speaks = named(program);

        EXPECT_FALSE(speaks.iwad) << program;
        EXPECT_TRUE(speaks.deh.empty()) << program;
        EXPECT_TRUE(speaks.save.empty()) << program;
        EXPECT_FALSE(speaks.configFile) << program;
    }
}

TEST(DialectOf, DoomLegacyPatchesThroughDehacked) {
    for (const char *program : {"doom3", "legacy", "doomlegacy"}) {
        const Dialect::Port speaks = named(program);

        EXPECT_EQ(speaks.deh, "-dehacked") << program;
        EXPECT_EQ(speaks.bex, "-dehacked") << program;
        EXPECT_FALSE(speaks.iwad) << program;
    }
}

TEST(DialectOf, ThePlainNamesMatchOnlyWhole) {
    EXPECT_TRUE(named("doom2").deh.empty());
    EXPECT_FALSE(named("doom2-plus").recognised);
}

TEST(DialectOfConfig, AProfileWithNoPortGetsTheDefaults) {
    Config config = launch::single("/games/gzdoom");

    config.activeProfile().port = "Nothing";

    const Dialect::Port speaks = Dialect::of(config);

    EXPECT_FALSE(speaks.dos);
    EXPECT_TRUE(speaks.recognised);
    EXPECT_EQ(speaks.netgame, Netgames::zdoom);
}

TEST(DialectOfConfig, ANativePortIsAnsweredForByItsName) {
    const Dialect::Port speaks = Dialect::of(launch::single("/games/dsda-doom"));

    EXPECT_FALSE(speaks.dos);
    EXPECT_EQ(speaks.complevel, Complevels::dsda);
}

TEST(DialectOfConfig, UnderDosboxNothingAfterTheLastDosReleaseApplies) {
    const Dialect::Port speaks = Dialect::of(launch::single("/games/MBF.EXE", true));

    EXPECT_TRUE(speaks.dos);
    EXPECT_TRUE(speaks.recognised);
    EXPECT_TRUE(speaks.configFile);
    EXPECT_TRUE(speaks.save.empty());
    EXPECT_FALSE(speaks.exec);
    EXPECT_FALSE(speaks.map);
    EXPECT_EQ(speaks.netgame, Netgames::none);
    EXPECT_EQ(speaks.complevel, Complevels::none);
    EXPECT_FALSE(speaks.longtics);
    EXPECT_FALSE(speaks.soloNet);
    EXPECT_FALSE(speaks.levelstat);
    EXPECT_FALSE(speaks.extraConfig);
}

TEST(DialectOfConfig, AnUnknownNameUnderDosboxIsVanilla) {
    const Dialect::Port speaks = Dialect::of(launch::single("/games/HACKED.EXE", true));

    EXPECT_TRUE(speaks.dos);
    EXPECT_FALSE(speaks.recognised);
    EXPECT_FALSE(speaks.iwad);
    EXPECT_TRUE(speaks.deh.empty());
    EXPECT_FALSE(speaks.configFile);
}

TEST(DialectOfConfig, TheProfileGivenIsTheOneAnsweredFor) {
    Config config = launch::single("/games/gzdoom");

    config.ports.push_back({.name = "Other", .file = "/games/woof"});

    Profile other = config.activeProfile();

    other.port = "Other";

    EXPECT_EQ(Dialect::of(config, other).complevel, Complevels::woof);
    EXPECT_EQ(Dialect::of(config).complevel, Complevels::none);
}

TEST(DialectComplevels, EveryTableLeavesTheChoiceToThePort) {
    for (const Complevels which : {Complevels::woof, Complevels::prboom, Complevels::dsda}) {
        EXPECT_TRUE(holds(Dialect::complevels(which), -1));
    }
}

TEST(DialectComplevels, NoneIsEmpty) {
    EXPECT_TRUE(Dialect::complevels(Complevels::none).empty());
}

TEST(DialectComplevels, WoofTakesOnlyItsOwnLevels) {
    const std::vector<int> levels = Dialect::complevels(Complevels::woof);

    EXPECT_EQ(levels, (std::vector<int>{-1, 2, 3, 4, 9, 11, 21, 24}));
}

TEST(DialectComplevels, DsdaAddsMbf21ToThePrboomTable) {
    EXPECT_TRUE(holds(Dialect::complevels(Complevels::dsda), 21));
    EXPECT_FALSE(holds(Dialect::complevels(Complevels::prboom), 21));
    EXPECT_TRUE(holds(Dialect::complevels(Complevels::prboom), 17));
}

TEST(DialectDemos, APortWithoutDemosOffersNoneOfTheRest) {
    const Dialect::Port speaks{
            .demos = false,
            .timedemo = true,
            .fastdemo = true,
            .complevel = Complevels::dsda,
            .longtics = true,
            .soloNet = true,
    };

    const Dialect::DemoSupport demos = Dialect::demos(speaks);

    EXPECT_FALSE(demos.records);
    EXPECT_FALSE(demos.timed);
    EXPECT_FALSE(demos.fast);
    EXPECT_EQ(demos.complevel, Complevels::none);
    EXPECT_FALSE(demos.longtics);
    EXPECT_FALSE(demos.soloNet);
}

TEST(DialectDemos, DsdaRecordsWithEveryOption) {
    const Dialect::DemoSupport demos = Dialect::demos(named("dsda-doom"));

    EXPECT_TRUE(demos.records);
    EXPECT_TRUE(demos.timed);
    EXPECT_TRUE(demos.fast);
    EXPECT_EQ(demos.complevel, Complevels::dsda);
    EXPECT_TRUE(demos.longtics);
    EXPECT_TRUE(demos.soloNet);
}

TEST(DialectSaves, AFolderNeedsASaveSwitch) {
    EXPECT_TRUE(Dialect::saves(named("gzdoom")).folder);
    EXPECT_FALSE(Dialect::saves(named("doom2")).folder);
    EXPECT_EQ(Dialect::saves(named("prboom-plus")).names, SaveNames::slot);
}

TEST(DialectNet, TheZdoomFamilyHasEveryCvar) {
    const Dialect::NetSupport net = Dialect::net(named("gzdoom"));

    EXPECT_TRUE(net.hosts);
    EXPECT_TRUE(net.joins);
    EXPECT_TRUE(net.players);
    EXPECT_TRUE(net.fragLimit);
    EXPECT_TRUE(net.flags);
    EXPECT_TRUE(net.savegame);
    EXPECT_TRUE(net.extratic);
    EXPECT_TRUE(net.netmode);
    EXPECT_TRUE(net.dup);
    EXPECT_FALSE(net.listing);
}

TEST(DialectNet, ChocolateHostsAndListsButSetsNoCvars) {
    const Dialect::NetSupport net = Dialect::net(named("chocolate-doom"));

    EXPECT_TRUE(net.hosts);
    EXPECT_TRUE(net.joins);
    EXPECT_TRUE(net.listing);
    EXPECT_TRUE(net.dup);
    EXPECT_FALSE(net.players);
    EXPECT_FALSE(net.flags);
    EXPECT_FALSE(net.extratic);
    EXPECT_FALSE(net.netmode);
}

TEST(DialectNet, PrboomOnlyJoins) {
    const Dialect::NetSupport net = Dialect::net(named("prboom-plus"));

    EXPECT_FALSE(net.hosts);
    EXPECT_TRUE(net.joins);
    EXPECT_FALSE(net.dup);
}

TEST(DialectNet, NoneOffersNothing) {
    const Dialect::NetSupport net = Dialect::net(named("dsda-doom"));

    EXPECT_FALSE(net.hosts);
    EXPECT_FALSE(net.joins);
    EXPECT_FALSE(net.listing);
}
