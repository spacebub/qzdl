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
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Launch.h"
#include "core/launch/Dialect.h"
#include "core/launch/dialects/Dialects.h"
#include "core/ports/Catalog.h"

namespace {

using Dialect::Complevels;
using Dialect::Feature;
using Dialect::Netgames;
using Dialect::SaveNames;

Dialect::Port named(const char *program) {
    return Dialect::of(std::filesystem::path("/games") / program);
}

bool holds(const std::span<const int> levels, const int level) {
    return std::ranges::find(levels, level) != levels.end();
}

}

TEST(DialectOf, AnUnknownNameIsTakenForTheZdoomFamilyAndMarkedAGuess) {
    const Dialect::Port port = named("mystery-engine");

    EXPECT_FALSE(port.has(Feature::recognised));
    EXPECT_TRUE(port.has(Feature::iwad));
    EXPECT_EQ(port.save, "-savedir");
    EXPECT_EQ(port.loads, SaveNames::name);
    EXPECT_EQ(port.netgame, Netgames::zdoom);
    EXPECT_TRUE(port.has(Feature::exec));
    EXPECT_TRUE(port.has(Feature::map));
}

TEST(DialectOf, GzdoomIsTheZdoomFamilyLoadingSavesByName) {
    const Dialect::Port port = named("gzdoom");

    EXPECT_EQ(port.loads, SaveNames::name);
    EXPECT_EQ(port.saveExt, ".zds");
    EXPECT_EQ(port.deh, "-deh");
    EXPECT_EQ(port.bex, "-bex");
    EXPECT_TRUE(port.has(Feature::netmode));
    EXPECT_FALSE(port.has(Feature::dos));
}

TEST(DialectOf, OnlyTheWholeNameZdoomLoadsSavesByPath) {
    EXPECT_EQ(named("zdoom").loads, SaveNames::path);
    EXPECT_EQ(named("zandronum").loads, SaveNames::path);
    EXPECT_EQ(named("gzdoom").loads, SaveNames::name);
    EXPECT_EQ(named("lzdoom").loads, SaveNames::name);
}

TEST(DialectOf, UzdoomHasNoNetmode) {
    EXPECT_FALSE(named("uzdoom").has(Feature::netmode));
    EXPECT_FALSE(named("uzdoom").has(Feature::netmode));
}

TEST(DialectOf, TheNameIsReadWithoutItsExtensionOrCase) {
    EXPECT_EQ(named("DSDA-Doom.exe").complevel, Complevels::dsda);
    EXPECT_EQ(named("Woof.AppImage").complevel, Complevels::woof);
}

TEST(DialectOf, AFlatpakExportIsNamedByTheLastPartOfItsAppId) {
    const std::filesystem::path exports = "/var/lib/flatpak/exports/bin";

    EXPECT_EQ(Dialect::of(exports / "io.github.fabiangreffrath.woof").complevel, Complevels::woof);
    EXPECT_EQ(Dialect::of(exports / "io.github.kraflab.dsda-doom").complevel, Complevels::dsda);
    EXPECT_FALSE(Dialect::of(exports / "org.zdoom.UZDoom").has(Feature::netmode));
    EXPECT_EQ(Dialect::of("/home/player/.local/share/flatpak/exports/bin/org.chocolate_doom.ChocolateDoom").netgame,
              Netgames::chocolate);
}

TEST(DialectOf, TheCrispyFlatpakIsNamedByTheProgramItRuns) {
    const Dialect::Port port = Dialect::of("/var/lib/flatpak/exports/bin/io.github.fabiangreffrath.Doom");

    EXPECT_TRUE(port.has(Feature::recognised));
    EXPECT_EQ(port.id, Dialect::Id::chocolate);
    EXPECT_EQ(port.complevel, Complevels::gameVersion);
    EXPECT_EQ(port.configName, "crispy");
    EXPECT_EQ(port.line, PortLine::crispy);
    EXPECT_TRUE(port.has(Feature::levelstat));
    EXPECT_EQ(port.bex, "-deh");
}

TEST(DialectOf, PortsThatWriteOtherFormatsGetTheirOwnConfigName) {
    EXPECT_TRUE(named("gzdoom").configName.empty());
    EXPECT_TRUE(named("uzdoom").configName.empty());
    EXPECT_TRUE(named("zandronum").configName.empty());
    EXPECT_EQ(named("chocolate-doom").configName, "chocolate");
    EXPECT_EQ(named("crispy-doom").configName, "crispy");
    EXPECT_EQ(named("woof").configName, "woof");
    EXPECT_EQ(named("nugget-doom").configName, "nugget");
    EXPECT_EQ(named("dsda-doom").configName, "dsda");
    EXPECT_EQ(named("nyan-doom").configName, "nyan");
    EXPECT_EQ(named("prboom-plus").configName, "prboom");
}

TEST(DialectOf, ChocolateTakesASecondConfigAndHostsItsOwnLobby) {
    const Dialect::Port port = named("chocolate-doom");

    EXPECT_TRUE(port.has(Feature::extraConfig));
    EXPECT_EQ(port.netgame, Netgames::chocolate);
    EXPECT_EQ(port.loads, SaveNames::slot);
    EXPECT_FALSE(port.has(Feature::levelstat));
}

TEST(DialectOf, CrispyIsChocolateThatWritesALevelstat) {
    const Dialect::Port port = named("crispy-doom");

    EXPECT_TRUE(port.has(Feature::extraConfig));
    EXPECT_TRUE(port.has(Feature::levelstat));
}

TEST(DialectOf, ChocolateMergesAddOnsWhereCrispyMergesOnFile) {
    EXPECT_EQ(named("chocolate-doom").pwads, "-merge");
    EXPECT_EQ(named("chocolate-heretic").pwads, "-merge");
    EXPECT_EQ(named("crispy-doom").pwads, "-file");
    EXPECT_EQ(named("gzdoom").pwads, "-file");
    EXPECT_EQ(Dialect::of("/games/mystery-engine", "chocolate").pwads, "-merge");
}

TEST(DialectOf, TheWoofLineSharesOneComplevelTable) {
    for (const char *program : {"woof", "nugget-doom", "cherry-doom"}) {
        const Dialect::Port port = named(program);

        EXPECT_EQ(port.complevel, Complevels::woof) << program;
        EXPECT_EQ(port.netgame, Netgames::chocolate) << program;
        EXPECT_EQ(port.save, "-save") << program;
    }
}

TEST(DialectOf, DsdaLoadsBySlotAndPlaysNoNetgame) {
    const Dialect::Port port = named("dsda-doom");

    EXPECT_EQ(port.loads, SaveNames::slot);
    EXPECT_EQ(port.lastSlot, 118);
    EXPECT_EQ(port.netgame, Netgames::none);
    EXPECT_TRUE(port.has(Feature::levelstat));
}

TEST(DialectOf, NyanDoomIsDsdaWithItsOwnConfig) {
    const Dialect::Port port = named("nyan-doom");

    EXPECT_TRUE(port.has(Feature::recognised));
    EXPECT_EQ(port.id, Dialect::Id::dsda);
    EXPECT_EQ(port.complevel, Complevels::dsda);
    EXPECT_EQ(port.loads, SaveNames::slot);
    EXPECT_EQ(port.configName, "nyan");
    EXPECT_EQ(Dialect::of("/games/nyan-doom", "dsda").configName, "nyan");
}

TEST(DialectOf, SkulltagIsZandronumWithoutATeamPick) {
    const Dialect::Port port = named("skulltag");

    EXPECT_TRUE(port.has(Feature::recognised));
    EXPECT_EQ(port.id, Dialect::Id::zandronum);
    EXPECT_EQ(port.loads, SaveNames::path);
    EXPECT_EQ(port.display, Dialect::Windowing::oldCvar);
    EXPECT_FALSE(port.has(Feature::pistolStart));
    EXPECT_FALSE(port.has(Feature::teamPick));
    EXPECT_EQ(named("Skulltag.exe").id, Dialect::Id::zandronum);
}

TEST(DialectOf, ThePrboomLineOnlyJoinsAndPatchesBexThroughDeh) {
    for (const char *program : {"prboom-plus", "glboom-plus", "rude"}) {
        const Dialect::Port port = named(program);

        EXPECT_EQ(port.netgame, Netgames::prboom) << program;
        EXPECT_EQ(port.complevel, Complevels::prboom) << program;
        EXPECT_EQ(port.bex, "-deh") << program;
        EXPECT_FALSE(port.has(Feature::exec)) << program;
    }
}

TEST(DialectOf, BoomKeepsItsOwnConfigWhereMbfTakesOne) {
    EXPECT_FALSE(named("boom").has(Feature::configFile));
    EXPECT_TRUE(named("mbf").has(Feature::configFile));
    EXPECT_TRUE(named("winmbf").has(Feature::configFile));
    EXPECT_EQ(named("boom").complevel, Complevels::none);
}

TEST(DialectOf, HelionCannotRespawnOrTimeADemo) {
    const Dialect::Port port = named("helion");

    EXPECT_FALSE(port.has(Feature::respawn));
    EXPECT_FALSE(port.has(Feature::timedemo));
    EXPECT_EQ(port.saveExt, ".hsg");
}

TEST(DialectOf, HelionWritesALevelstatAndRecordsToTheExactPath) {
    const Dialect::Port port = named("helion");

    EXPECT_TRUE(port.has(Feature::levelstat));
    EXPECT_FALSE(port.has(Feature::recordAddsLmp));
    EXPECT_EQ(port.complevel, Complevels::helion);
    EXPECT_TRUE(named("gzdoom").has(Feature::recordAddsLmp));
}

TEST(DialectOf, EternityHasNoNetgame) {
    EXPECT_EQ(named("eternity").netgame, Netgames::none);
    EXPECT_EQ(named("eternity").deh, "-deh");
    EXPECT_EQ(named("eternity").bex, "-bex");
}

TEST(DialectOf, ThePlainDosReleasesTakeNoIwadNoPatchAndNoSaveFolder) {
    for (const char *program : {"doom", "doom2", "doomu", "doom95", "dosdoom"}) {
        const Dialect::Port port = named(program);

        EXPECT_FALSE(port.has(Feature::iwad)) << program;
        EXPECT_TRUE(port.deh.empty()) << program;
        EXPECT_TRUE(port.save.empty()) << program;
        EXPECT_FALSE(port.has(Feature::configFile)) << program;
    }
}

TEST(DialectOf, DoomLegacyPatchesThroughDehacked) {
    for (const char *program : {"doom3", "legacy", "doomlegacy"}) {
        const Dialect::Port port = named(program);

        EXPECT_EQ(port.deh, "-dehacked") << program;
        EXPECT_EQ(port.bex, "-dehacked") << program;
        EXPECT_FALSE(port.has(Feature::iwad)) << program;
    }
}

TEST(DialectOf, ThePlainNamesMatchOnlyWhole) {
    EXPECT_TRUE(named("doom2").deh.empty());
    EXPECT_FALSE(named("doom2-plus").has(Feature::recognised));
}

TEST(DialectOfConfig, AProfileWithNoPortGetsTheDefaults) {
    Config config = launch::single("/games/gzdoom");

    config.activeProfile().port = "Nothing";

    const Dialect::Port port = Dialect::of(config);

    EXPECT_FALSE(port.has(Feature::dos));
    EXPECT_TRUE(port.has(Feature::recognised));
    EXPECT_EQ(port.netgame, Netgames::zdoom);
}

TEST(DialectOfConfig, ANativePortIsAnsweredForByItsName) {
    const Dialect::Port port = Dialect::of(launch::single("/games/dsda-doom"));

    EXPECT_FALSE(port.has(Feature::dos));
    EXPECT_EQ(port.complevel, Complevels::dsda);
}

TEST(DialectOfConfig, UnderDosboxNothingAfterTheLastDosReleaseApplies) {
    const Dialect::Port port = Dialect::of(launch::single("/games/MBF.EXE", true));

    EXPECT_TRUE(port.has(Feature::dos));
    EXPECT_TRUE(port.has(Feature::recognised));
    EXPECT_TRUE(port.has(Feature::configFile));
    EXPECT_TRUE(port.save.empty());
    EXPECT_FALSE(port.has(Feature::exec));
    EXPECT_FALSE(port.has(Feature::map));
    EXPECT_EQ(port.netgame, Netgames::none);
    EXPECT_EQ(port.complevel, Complevels::none);
    EXPECT_FALSE(port.has(Feature::longtics));
    EXPECT_FALSE(port.has(Feature::soloNet));
    EXPECT_FALSE(port.has(Feature::levelstat));
    EXPECT_FALSE(port.has(Feature::extraConfig));
}

TEST(DialectOfConfig, AnUnknownNameUnderDosboxIsVanilla) {
    const Dialect::Port port = Dialect::of(launch::single("/games/HACKED.EXE", true));

    EXPECT_TRUE(port.has(Feature::dos));
    EXPECT_FALSE(port.has(Feature::recognised));
    EXPECT_FALSE(port.has(Feature::iwad));
    EXPECT_TRUE(port.deh.empty());
    EXPECT_FALSE(port.has(Feature::configFile));
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
    for (const Complevels which : {Complevels::woof, Complevels::prboom, Complevels::dsda, Complevels::helion}) {
        EXPECT_TRUE(holds(Dialect::complevels(which), -1));
    }
}

TEST(DialectComplevels, HelionNamesItsLevels) {
    EXPECT_EQ(std::ranges::to<std::vector>(Dialect::complevels(Complevels::helion)),
              (std::vector<int>{-1, 2, 9, 11, 21}));
    EXPECT_EQ(Dialect::complevelWord(Complevels::helion, 2), "vanilla");
    EXPECT_EQ(Dialect::complevelWord(Complevels::helion, 9), "boom");
    EXPECT_EQ(Dialect::complevelWord(Complevels::helion, 11), "mbf");
    EXPECT_EQ(Dialect::complevelWord(Complevels::helion, 21), "mbf21");
    EXPECT_EQ(Dialect::complevelWord(Complevels::helion, -1), "");
}

TEST(DialectComplevels, TheOtherPortsTakeTheNumber) {
    EXPECT_EQ(Dialect::complevelWord(Complevels::dsda, 21), "21");
    EXPECT_EQ(Dialect::complevelWord(Complevels::prboom, 0), "0");
    EXPECT_EQ(Dialect::complevelWord(Complevels::woof, -1), "");
}

TEST(DialectComplevels, NoneIsEmpty) {
    EXPECT_TRUE(Dialect::complevels(Complevels::none).empty());
}

TEST(DialectComplevels, WoofTakesOnlyItsOwnLevels) {
    EXPECT_EQ(std::ranges::to<std::vector>(Dialect::complevels(Complevels::woof)),
              (std::vector<int>{-1, 2, 3, 4, 9, 11, 21, 24}));
}

TEST(DialectComplevels, DsdaAddsMbf21ToThePrboomTable) {
    EXPECT_TRUE(holds(Dialect::complevels(Complevels::dsda), 21));
    EXPECT_FALSE(holds(Dialect::complevels(Complevels::prboom), 21));
    EXPECT_TRUE(holds(Dialect::complevels(Complevels::prboom), 17));
}

TEST(DialectDemos, APortWithoutDemosOffersNoneOfTheRest) {
    const Dialect::Port port = Dialect::defined({
            .features = Feature::timedemo | Feature::fastdemo | Feature::longtics | Feature::soloNet,
            .complevel = Complevels::dsda,
    });

    EXPECT_FALSE(port.has(Feature::demos));
    EXPECT_FALSE(port.has(Feature::timedemo));
    EXPECT_FALSE(port.has(Feature::fastdemo));
    EXPECT_EQ(Dialect::replayComplevel(port), Complevels::none);
    EXPECT_FALSE(port.has(Feature::longtics));
    EXPECT_FALSE(port.has(Feature::soloNet));
}

TEST(DialectDemos, DsdaRecordsWithEveryOption) {
    const Dialect::Port port = named("dsda-doom");

    EXPECT_TRUE(port.has(Feature::demos));
    EXPECT_TRUE(port.has(Feature::timedemo));
    EXPECT_TRUE(port.has(Feature::fastdemo));
    EXPECT_EQ(Dialect::replayComplevel(port), Complevels::dsda);
    EXPECT_TRUE(port.has(Feature::longtics));
    EXPECT_TRUE(port.has(Feature::soloNet));
}

TEST(DialectSaves, AFolderNeedsASaveSwitch) {
    EXPECT_TRUE(named("gzdoom").has(Feature::saveFolder));
    EXPECT_FALSE(named("doom2").has(Feature::saveFolder));
    EXPECT_TRUE(named("prboom-plus").loadsBy(SaveNames::slot));
}

TEST(DialectNet, TheZdoomFamilyHasEveryCvar) {
    const Dialect::Port net = named("gzdoom");

    EXPECT_TRUE(net.has(Feature::hosts));
    EXPECT_TRUE(net.has(Feature::joins));
    EXPECT_TRUE(net.has(Feature::players));
    EXPECT_TRUE(net.has(Feature::fragLimit));
    EXPECT_TRUE(net.has(Feature::timeLimitCvar));
    EXPECT_TRUE(net.has(Feature::savegame));
    EXPECT_TRUE(net.has(Feature::extratic));
    EXPECT_TRUE(net.has(Feature::netmode));
    EXPECT_TRUE(net.has(Feature::dup));
    EXPECT_FALSE(net.has(Feature::listing));
}

TEST(DialectNet, ChocolateHostsAndListsButSetsNoCvars) {
    const Dialect::Port net = named("chocolate-doom");

    EXPECT_TRUE(net.has(Feature::hosts));
    EXPECT_TRUE(net.has(Feature::joins));
    EXPECT_TRUE(net.has(Feature::listing));
    EXPECT_TRUE(net.has(Feature::dup));
    EXPECT_FALSE(net.has(Feature::players));
    EXPECT_FALSE(net.has(Feature::timeLimitCvar));
    EXPECT_FALSE(net.has(Feature::extratic));
    EXPECT_FALSE(net.has(Feature::netmode));
}

TEST(DialectNet, PrboomOnlyJoins) {
    const Dialect::Port net = named("prboom-plus");

    EXPECT_FALSE(net.has(Feature::hosts));
    EXPECT_TRUE(net.has(Feature::joins));
    EXPECT_FALSE(net.has(Feature::dup));
}

TEST(DialectNet, NoneOffersNothing) {
    const Dialect::Port net = named("dsda-doom");

    EXPECT_FALSE(net.has(Feature::hosts));
    EXPECT_FALSE(net.has(Feature::joins));
    EXPECT_FALSE(net.has(Feature::listing));
}

TEST(DialectNet, OnlyUzdoomKeepsNetgameSavesInAFolderApart) {
    EXPECT_TRUE(named("uzdoom").has(Feature::netgameFolder));
    EXPECT_TRUE(named("uzdoom").has(Feature::netgameFolder));
    EXPECT_TRUE(Dialect::of("/var/lib/flatpak/exports/bin/org.zdoom.UZDoom").has(Feature::netgameFolder));
    EXPECT_FALSE(named("gzdoom").has(Feature::netgameFolder));
    EXPECT_FALSE(named("zandronum").has(Feature::netgameFolder));
}

TEST(DialectChoices, EveryCatalogPortSpeaksAChoiceThatAgreesOnDos) {
    for (const Catalog::Port &listed : Catalog::ports()) {
        const Dialect::Port port = named(std::string(listed.program).c_str());
        ASSERT_TRUE(port.has(Feature::recognised)) << listed.id;
        EXPECT_EQ(Dialect::choice(port.id).dos, listed.dos) << listed.id;
    }
}

TEST(DialectChoices, EveryChoiceIsSpokenWhenPickedForAnUnknownName) {
    for (const Dialect::Choice &each : Dialect::choices()) {
        const Dialect::Port port = Dialect::of("/games/mystery-engine", each.word);

        EXPECT_EQ(port.id, each.id);
        EXPECT_TRUE(port.has(Feature::recognised)) << each.word;
    }
}

TEST(DialectPicked, APickedDialectWinsOverTheName) {
    const Dialect::Port port = Dialect::of("/games/gzdoom", "dsda");

    EXPECT_EQ(port.id, Dialect::Id::dsda);
    EXPECT_EQ(port.complevel, Complevels::dsda);
    EXPECT_EQ(port.configName, "dsda");
}

TEST(DialectPicked, TheNameKeepsItsDetailsWhenItNamesAPortOfThePickedDialect) {
    EXPECT_EQ(Dialect::of("/games/nugget-doom", "woof").configName, "nugget");
    EXPECT_TRUE(Dialect::of("/games/crispy-doom", "chocolate").has(Feature::levelstat));
    EXPECT_TRUE(Dialect::of("/games/mbf.exe", "boom").has(Feature::configFile));
}

TEST(DialectPicked, APlainDialectTakesTheConfigNameOfItsLeadPort) {
    EXPECT_EQ(Dialect::of("/games/mystery-engine", "woof").configName, "woof");
    EXPECT_EQ(Dialect::of("/games/mystery-engine", "prboom").configName, "prboom");
    EXPECT_FALSE(Dialect::of("/games/mystery-engine", "chocolate").has(Feature::levelstat));
}

TEST(DialectPicked, NoIdOrAnUnknownOneLeavesItToTheName) {
    EXPECT_EQ(Dialect::of("/games/woof", "").id, Dialect::Id::woof);
    EXPECT_EQ(Dialect::of("/games/woof", "quake").id, Dialect::Id::woof);
    EXPECT_FALSE(Dialect::of("/games/mystery-engine", "").has(Feature::recognised));
}

TEST(DialectPicked, AnEntryHandsItsDialectToTheProfile) {
    Config config;
    config.ports.push_back({.name = "Mine", .file = "/games/mystery-engine", .dialect = "woof"});
    config.addProfile("x");
    config.activeProfile().port = "Mine";

    EXPECT_EQ(Dialect::of(config).id, Dialect::Id::woof);
    EXPECT_EQ(Dialect::of(config).complevel, Complevels::woof);
}

TEST(DialectPicked, AnEntryUnderDosboxKeepsItsPickedDialect) {
    Config config;
    config.ports.push_back({.name = "Mine", .file = "/games/run.bat", .dosbox = true, .dialect = "legacy"});
    config.addProfile("x");
    config.activeProfile().port = "Mine";

    const Dialect::Port port = Dialect::of(config);

    EXPECT_EQ(port.id, Dialect::Id::legacy);
    EXPECT_TRUE(port.has(Feature::dos));
    EXPECT_EQ(port.deh, "-dehacked");
}

TEST(DialectPicked, ADownloadedPortIgnoresAStoredDialect) {
    EXPECT_EQ(Dialect::of(PortEntry{.name = "Woof!", .file = "/data/woof", .portId = "woof", .dialect = "dsda"}).id,
              Dialect::Id::woof);
}

TEST(DialectPicked, ANativeNameUnderDosboxIsTakenForVanilla) {
    const Dialect::Port port = Dialect::of(PortEntry{.name = "Old", .file = "/games/prboom.exe", .dosbox = true});

    EXPECT_EQ(port.id, Dialect::Id::doom);
    EXPECT_TRUE(port.has(Feature::dos));
    EXPECT_FALSE(port.has(Feature::recognised));
}

TEST(DialectPicked, ADosNameUnderDosboxKeepsItsDialect) {
    EXPECT_EQ(Dialect::of(PortEntry{.name = "MBF", .file = "/games/mbf.exe", .dosbox = true}).id, Dialect::Id::boom);
}

#ifndef _WIN32
TEST(DialectDosFromName, OffWindowsADosSuffixIsADosProgram) {
    EXPECT_EQ(Dialect::dosFromName("/games/mbf.EXE"), true);
    EXPECT_EQ(Dialect::dosFromName("/games/run.bat"), true);
    EXPECT_EQ(Dialect::dosFromName("/games/doom.com"), true);
    EXPECT_EQ(Dialect::dosFromName("/games/woof"), false);
    EXPECT_EQ(Dialect::dosFromName("/games/Helion.AppImage"), false);
}
#endif

TEST(DialectGameplay, EachPortStartsAMapWithAPistolItsOwnWay) {
    EXPECT_TRUE(named("gzdoom").startsBy(Dialect::PistolStart::cvar));
    EXPECT_TRUE(named("uzdoom").startsBy(Dialect::PistolStart::cvar));
    EXPECT_FALSE(named("zdoom").has(Feature::pistolStart));
    EXPECT_FALSE(named("zandronum").has(Feature::pistolStart));
    EXPECT_TRUE(named("crispy-doom").startsBy(Dialect::PistolStart::flag));
    EXPECT_FALSE(named("chocolate-doom").has(Feature::pistolStart));
    EXPECT_TRUE(named("helion").startsBy(Dialect::PistolStart::flag));

    for (const char *program : {"dsda-doom", "prboom-plus", "woof", "nugget-doom"}) {
        EXPECT_TRUE(named(program).startsBy(Dialect::PistolStart::flag)) << program;
    }
}

TEST(DialectGameplay, OnlyTheZdoomFamilyNeedsCheatsTurnedOn) {
    for (const char *program : {"gzdoom", "uzdoom", "zdoom", "zandronum"}) {
        EXPECT_TRUE(named(program).has(Feature::cheats)) << program;
    }

    for (const char *program : {"dsda-doom", "woof", "chocolate-doom", "helion"}) {
        EXPECT_FALSE(named(program).has(Feature::cheats)) << program;
    }
}

TEST(DialectGameplay, OnlyTheChocolateLineAndHelionHaveNoShotdir) {
    EXPECT_TRUE(named("gzdoom").has(Feature::shotdir));
    EXPECT_TRUE(named("dsda-doom").has(Feature::shotdir));
    EXPECT_TRUE(named("woof").has(Feature::shotdir));
    EXPECT_FALSE(named("crispy-doom").has(Feature::shotdir));
    EXPECT_FALSE(named("helion").has(Feature::shotdir));
}

TEST(DialectGameplay, EachPortSkipsItsAutoloadWithItsOwnSwitch) {
    for (const char *program : {"gzdoom", "uzdoom", "zandronum", "dsda-doom", "woof", "nugget-doom", "crispy-doom"}) {
        EXPECT_EQ(named(program).noAutoload, "-noautoload") << program;
    }

    EXPECT_EQ(named("prboom-plus").noAutoload, "-noload");
    EXPECT_EQ(named("eternity").noAutoload, "-noload");
    EXPECT_TRUE(named("helion").noAutoload.empty());
}

TEST(DialectGameplay, OnlyHelionHasNoSoundEffectsSwitch) {
    for (const char *program : {"gzdoom", "zandronum", "dsda-doom", "woof", "chocolate-doom", "eternity", "helion"}) {
        EXPECT_TRUE(named(program).has(Feature::nomusic)) << program;
    }

    EXPECT_EQ(named("gzdoom").nosfx, "-nosfx");
    EXPECT_EQ(named("eternity").nosfx, "-nosfx");
    EXPECT_TRUE(named("helion").nosfx.empty());
}

TEST(DialectGameplay, EachPortIsToldWindowOrFullscreenItsOwnWay) {
    EXPECT_EQ(named("gzdoom").display, Dialect::Windowing::cvar);
    EXPECT_EQ(named("uzdoom").display, Dialect::Windowing::cvar);
    EXPECT_EQ(named("zandronum").display, Dialect::Windowing::oldCvar);
    EXPECT_EQ(named("prboom-plus").display, Dialect::Windowing::noWindow);
    EXPECT_EQ(named("dsda-doom").display, Dialect::Windowing::switches);
    EXPECT_EQ(named("woof").display, Dialect::Windowing::switches);
    EXPECT_EQ(named("crispy-doom").display, Dialect::Windowing::switches);
    EXPECT_EQ(named("eternity").display, Dialect::Windowing::none);
    EXPECT_EQ(named("helion").display, Dialect::Windowing::none);

    EXPECT_TRUE(named("gzdoom").has(Feature::displayKept));
    EXPECT_TRUE(named("woof").has(Feature::displayKept));
    EXPECT_FALSE(named("dsda-doom").has(Feature::displayKept));
    EXPECT_FALSE(named("prboom-plus").has(Feature::displayKept));
}

TEST(DialectGameplay, ZandronumTakesNoTeamPick) {
    EXPECT_TRUE(named("zdoom").has(Feature::teamPick));
    EXPECT_TRUE(named("zandronum").has(Feature::teams));
    EXPECT_FALSE(named("zandronum").has(Feature::teamPick));
    EXPECT_FALSE(named("chocolate-doom").has(Feature::teams));
}

TEST(DialectGameplay, TheBoomLineContinuesADemoAndSomeInsistOnAComplevel) {
    EXPECT_TRUE(named("prboom-plus").has(Feature::continues));
    EXPECT_FALSE(named("prboom-plus").has(Feature::continueComplevel));
    EXPECT_TRUE(named("dsda-doom").has(Feature::continueComplevel));
    EXPECT_TRUE(named("nugget-doom").has(Feature::continueComplevel));
    EXPECT_FALSE(named("gzdoom").has(Feature::continues));
    EXPECT_FALSE(named("crispy-doom").has(Feature::continues));
}

TEST(DialectGameplay, NothingNewReachesAPortUnderDosbox) {
    const Dialect::Port port = Dialect::of(PortEntry{.name = "MBF", .file = "/games/mbf.exe", .dosbox = true});

    EXPECT_FALSE(port.has(Feature::pistolStart));
    EXPECT_FALSE(port.has(Feature::cheats));
    EXPECT_FALSE(port.has(Feature::shotdir));
    EXPECT_FALSE(port.has(Feature::continues));
    EXPECT_EQ(port.display, Dialect::Windowing::none);
}

TEST(DialectGameplay, EachDosPortTurnsOffMusicAndEffectsItsOwnWay) {
    const auto dos = [](const char *file) {
        return Dialect::of(PortEntry{.name = "DOS", .file = file, .dosbox = true});
    };

    EXPECT_TRUE(dos("/games/doom.exe").has(Feature::nomusic));
    EXPECT_EQ(dos("/games/doom.exe").nosfx, "-nosfx");
    EXPECT_EQ(dos("/games/mbf.exe").nosfx, "-nosfx");
    EXPECT_TRUE(dos("/games/legacy.exe").has(Feature::nomusic));
    EXPECT_EQ(dos("/games/legacy.exe").nosfx, "-nosound");
}

TEST(DialectGameplay, OnlyMbfAndBoomSkipPreloadsUnderDosbox) {
    const auto dos = [](const char *file) {
        return Dialect::of(PortEntry{.name = "DOS", .file = file, .dosbox = true});
    };

    EXPECT_EQ(dos("/games/mbf.exe").noAutoload, "-noload");
    EXPECT_EQ(dos("/games/boom.exe").noAutoload, "-noload");
    EXPECT_TRUE(dos("/games/legacy.exe").noAutoload.empty());
    EXPECT_TRUE(dos("/games/doom.exe").noAutoload.empty());
}

TEST(DialectOf, ChocolateAndCrispyListTheDoomReleases) {
    const std::span<const int> levels = Dialect::complevels(Dialect::Complevels::gameVersion);

    ASSERT_EQ(levels.size(), 12U);
    EXPECT_EQ(levels.front(), -1);
    EXPECT_EQ(Dialect::complevelWord(Dialect::Complevels::gameVersion, levels[1]), "1.2");
    EXPECT_EQ(Dialect::complevelWord(Dialect::Complevels::gameVersion, levels.back()), "chex");
    EXPECT_EQ(Dialect::versionName(levels[7]), "Ultimate Doom");
    EXPECT_EQ(Dialect::versionName(9), "") << "a complevel is no version";
    EXPECT_EQ(Dialect::complevelWord(Dialect::Complevels::gameVersion, 9), "");
    EXPECT_EQ(Dialect::complevelSwitch(Dialect::Complevels::gameVersion), "-gameversion");
    EXPECT_EQ(Dialect::complevelSwitch(Dialect::Complevels::helion), "+complevel");
    EXPECT_EQ(Dialect::complevelSwitch(Dialect::Complevels::dsda), "-complevel");
    EXPECT_EQ(named("chocolate-doom").complevel, Dialect::Complevels::gameVersion);
    EXPECT_EQ(named("crispy-doom").complevel, Dialect::Complevels::gameVersion);
    EXPECT_EQ(named("chocolate-hexen").complevel, Dialect::Complevels::none);
}

TEST(DialectOf, DeathmatchThreeIsHostedByWoofAndCrispyOnly) {
    EXPECT_TRUE(named("woof").has(Feature::deathmatch3));
    EXPECT_TRUE(named("nugget-doom").has(Feature::deathmatch3));
    EXPECT_TRUE(named("crispy-doom").has(Feature::deathmatch3));
    EXPECT_FALSE(named("chocolate-doom").has(Feature::deathmatch3));
    EXPECT_FALSE(named("crispy-heretic").has(Feature::deathmatch3));
    EXPECT_FALSE(named("gzdoom").has(Feature::deathmatch3));
}

namespace {

PortEntry release(const std::string &version, const char *program = "dsda-doom") {
    return PortEntry{.name = "Port", .file = (std::filesystem::path("/games") / program).string(), .version = version};
}

std::vector<std::string_view> firsts(const std::span<const Dialect::VersionRange> ranges) {
    std::vector<std::string_view> out;

    for (const Dialect::VersionRange &each : ranges) {
        out.push_back(each.first);
    }

    return out;
}

}

TEST(DialectVersions, DsdaDoomListsItsRangesNewestFirst) {
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release(""));

    EXPECT_EQ(firsts(ranges),
              (std::vector<std::string_view>{
                      "0.30.0", "0.28.0", "0.27.0", "0.26.0", "0.25.0", "0.22.3", "0.21.2", "0.21.1"}));
    EXPECT_EQ(ranges.front().label, "0.30.0 and later");
    EXPECT_EQ(ranges[1].label, "0.28.0 to 0.29.4");
    EXPECT_EQ(ranges[5].label, "0.22.3 to 0.24.3");
    EXPECT_EQ(ranges.back().label, "0.21.1 and earlier");
}

TEST(DialectVersions, TheRangesDoNotDependOnTheVersionStored) {
    EXPECT_EQ(firsts(Dialect::versions(release("0.25.0"))), firsts(Dialect::versions(release(""))));
    EXPECT_EQ(firsts(Dialect::versions(release("garbage"))), firsts(Dialect::versions(release(""))));
}

TEST(DialectVersions, EveryOtherPortHasNoRanges) {
    for (const char *program : {
                 "nyan-doom",
                 "woof",
                 "nugget-doom",
                 "gzdoom",
                 "uzdoom",
                 "prboom-plus",
                 "chocolate-doom",
                 "crispy-doom",
                 "helion",
                 "eternity",
                 "zandronum",
                 "zdoom",
                 "mystery-engine",
         }) {
        EXPECT_TRUE(Dialect::versions(release("", program)).empty()) << program;
    }
}

TEST(DialectVersions, ADsdaDialectPickedByHandHasTheRangesOfDsdaDoom) {
    PortEntry entry = release("", "mystery-engine");

    entry.dialect = "dsda";

    EXPECT_EQ(Dialect::versions(entry).size(), 8U);

    entry.file = "/games/nyan-doom";

    EXPECT_TRUE(Dialect::versions(entry).empty());
}

TEST(DialectVersions, ADsdaDoomRunUnderDosboxHasNoRanges) {
    PortEntry entry = release("0.25.0");

    entry.dosbox = true;

    EXPECT_TRUE(Dialect::versions(entry).empty());
}

TEST(DialectVersions, AnEntryWithRangesAndNoVersionNeedsOne) {
    EXPECT_TRUE(Dialect::needsVersion(release("")));
}

TEST(DialectVersions, AVersionThatNamesARangeNeedsNoMore) {
    for (const Dialect::VersionRange &each : Dialect::versions(release(""))) {
        EXPECT_FALSE(Dialect::needsVersion(release(std::string(each.first)))) << each.first;
    }
}

TEST(DialectVersions, AVersionThatNamesNoRangeStillNeedsOne) {
    for (const char *version : {"0.29.4", "garbage", "0.30.0 and later"}) {
        EXPECT_TRUE(Dialect::needsVersion(release(version))) << version;
    }
}

TEST(DialectVersions, APortWithNoRangesNeedsNoVersion) {
    EXPECT_FALSE(Dialect::needsVersion(release("", "gzdoom")));
    EXPECT_FALSE(Dialect::needsVersion(release("", "woof")));
}

TEST(DialectVersions, ADownloadedPortIsNeverAsked) {
    PortEntry entry = release("");

    entry.portId = "dsda-doom";

    EXPECT_FALSE(Dialect::needsVersion(entry));
}

TEST(DialectVersions, ADosboxProgramIsNeverAsked) {
    PortEntry entry = release("");

    entry.dosbox = true;

    EXPECT_FALSE(Dialect::needsVersion(entry));
}

TEST(DialectVersions, AnEntryWithNoVersionSpeaksTheLatestRelease) {
    const Dialect::Port latest = Dialect::of(release(""));
    const Dialect::Port named = Dialect::of(std::filesystem::path("/games/dsda-doom"));

    EXPECT_EQ(latest.loads, SaveNames::slot);
    EXPECT_TRUE(latest.has(Feature::recordsWithSkill));
    EXPECT_FALSE(latest.has(Feature::continuesWithSkill));
    EXPECT_TRUE(latest.has(Feature::recordsWithComplevel));
    EXPECT_EQ(latest.withoutRules, 0U);
    EXPECT_TRUE(latest.startsBy(Dialect::PistolStart::flag));
    EXPECT_EQ(latest.loads, named.loads);
    EXPECT_EQ(latest.line, named.line);
    EXPECT_EQ(latest.lastSlot, named.lastSlot);
}

TEST(DialectVersions, TheNewestRangeSpeaksTheLatestRelease) {
    const Dialect::Port latest = Dialect::of(release(""));
    const Dialect::Port newest = Dialect::of(release("0.30.0"));

    EXPECT_EQ(newest.loads, latest.loads);
    EXPECT_EQ(newest.has(Feature::recordsWithSkill), latest.has(Feature::recordsWithSkill));
    EXPECT_EQ(newest.has(Feature::continuesWithSkill), latest.has(Feature::continuesWithSkill));
    EXPECT_EQ(newest.has(Feature::recordsWithComplevel), latest.has(Feature::recordsWithComplevel));
    EXPECT_EQ(newest.withoutRules, latest.withoutRules);
    EXPECT_EQ(newest.pistolStart, latest.pistolStart);
}

TEST(DialectVersions, AVersionThatNamesNoRangeSpeaksTheLatestRelease) {
    for (const char *version : {"0.29.4", "0.27.3", "0.30.0 and later", "garbage", "0.2"}) {
        const Dialect::Port port = Dialect::of(release(version));

        EXPECT_EQ(port.loads, SaveNames::slot) << version;
        EXPECT_TRUE(port.has(Feature::recordsWithSkill)) << version;
        EXPECT_EQ(port.withoutRules, 0U) << version;
    }
}

TEST(DialectVersions, ASaveIsLoadedFrom0_30_0OnAndFromNoReleaseBetween0_22_3And0_29_4) {
    for (const char *version : {"0.30.0", "0.21.2", "0.21.1"}) {
        EXPECT_EQ(Dialect::of(release(version)).loads, SaveNames::slot) << version;
    }

    for (const char *version : {"0.28.0", "0.27.0", "0.26.0", "0.25.0", "0.22.3"}) {
        EXPECT_FALSE(Dialect::of(release(version)).has(Feature::loadgame)) << version;
        EXPECT_FALSE(Dialect::of(release(version)).loadsBy(SaveNames::slot)) << version;
    }
}

TEST(DialectVersions, RecordingNeedsASkillFrom0_27_0AndContinuingUntil0_28_0) {
    for (const char *version : {"0.30.0", "0.28.0"}) {
        EXPECT_TRUE(Dialect::of(release(version)).has(Feature::recordsWithSkill)) << version;
        EXPECT_FALSE(Dialect::of(release(version)).has(Feature::continuesWithSkill)) << version;
    }

    EXPECT_TRUE(Dialect::of(release("0.27.0")).has(Feature::recordsWithSkill));
    EXPECT_TRUE(Dialect::of(release("0.27.0")).has(Feature::continuesWithSkill));

    for (const char *version : {"0.26.0", "0.25.0", "0.22.3", "0.21.2", "0.21.1"}) {
        EXPECT_FALSE(Dialect::of(release(version)).has(Feature::recordsWithSkill)) << version;
        EXPECT_FALSE(Dialect::of(release(version)).has(Feature::continuesWithSkill)) << version;
    }
}

TEST(DialectVersions, RecordingNeedsAComplevelFrom0_25_0) {
    for (const char *version : {"0.30.0", "0.28.0", "0.27.0", "0.26.0", "0.25.0"}) {
        EXPECT_TRUE(Dialect::of(release(version)).has(Feature::recordsWithComplevel)) << version;
    }

    for (const char *version : {"0.22.3", "0.21.2", "0.21.1"}) {
        EXPECT_FALSE(Dialect::of(release(version)).has(Feature::recordsWithComplevel)) << version;
    }
}

TEST(DialectVersions, ChainEpisodesAndGameSpeedAreRulesFrom0_26_0) {
    using Rules::Rule;

    for (const char *version : {"0.30.0", "0.28.0", "0.27.0", "0.26.0"}) {
        EXPECT_FALSE(Dialect::lacksRule(Dialect::of(release(version)), Rule::chainEpisodes)) << version;
        EXPECT_FALSE(Dialect::lacksRule(Dialect::of(release(version)), Rule::speed)) << version;
    }

    for (const char *version : {"0.25.0", "0.22.3", "0.21.2", "0.21.1"}) {
        const Dialect::Port port = Dialect::of(release(version));

        EXPECT_TRUE(Dialect::lacksRule(port, Rule::chainEpisodes)) << version;
        EXPECT_TRUE(Dialect::lacksRule(port, Rule::speed)) << version;
        EXPECT_FALSE(Dialect::lacksRule(port, Rule::turbo)) << version;
        EXPECT_FALSE(Dialect::lacksRule(port, Rule::coopSpawns)) << version;
        EXPECT_FALSE(Dialect::lacksRule(port, Rule::stroller)) << version;
        EXPECT_FALSE(Dialect::lacksRule(port, Rule::noCheats)) << version;
    }
}

TEST(DialectVersions, PistolStartIsASwitchFrom0_21_2) {
    EXPECT_TRUE(Dialect::of(release("0.21.2")).startsBy(Dialect::PistolStart::flag));
    EXPECT_FALSE(Dialect::of(release("0.21.1")).has(Feature::pistolStart));
}

TEST(DialectVersions, AnOlderReleaseKeepsWhatNoBreakChanges) {
    const Dialect::Port latest = Dialect::of(release(""));
    const Dialect::Port oldest = Dialect::of(release("0.21.1"));

    EXPECT_EQ(oldest.id, latest.id);
    EXPECT_EQ(oldest.line, latest.line);
    EXPECT_EQ(oldest.save, latest.save);
    EXPECT_EQ(oldest.saveExt, latest.saveExt);
    EXPECT_EQ(oldest.lastSlot, latest.lastSlot);
    EXPECT_EQ(oldest.configName, latest.configName);
    EXPECT_EQ(oldest.complevel, latest.complevel);
    EXPECT_EQ(oldest.has(Feature::continues), latest.has(Feature::continues));
    EXPECT_EQ(oldest.has(Feature::continueComplevel), latest.has(Feature::continueComplevel));
    EXPECT_EQ(oldest.display, latest.display);
}

TEST(DialectVersions, AVersionOnAnotherPortChangesNothing) {
    for (const char *program : {"nyan-doom", "woof", "gzdoom", "prboom-plus", "chocolate-doom"}) {
        const Dialect::Port without = Dialect::of(release("", program));
        const Dialect::Port with = Dialect::of(release("0.25.0", program));

        EXPECT_EQ(with.loads, without.loads) << program;
        EXPECT_EQ(with.has(Feature::recordsWithSkill), without.has(Feature::recordsWithSkill)) << program;
        EXPECT_EQ(with.has(Feature::recordsWithComplevel), without.has(Feature::recordsWithComplevel)) << program;
        EXPECT_EQ(with.withoutRules, without.withoutRules) << program;
        EXPECT_EQ(with.pistolStart, without.pistolStart) << program;
    }
}

TEST(DialectVersions, ADsdaDoomRunUnderDosboxSpeaksVanillaWhateverTheVersion) {
    PortEntry entry = release("0.28.0");

    entry.dosbox = true;

    const Dialect::Port port = Dialect::of(entry);

    EXPECT_TRUE(port.has(Feature::dos));
    EXPECT_EQ(port.id, Dialect::Id::doom);
    EXPECT_EQ(port.loads, SaveNames::slot);
    EXPECT_FALSE(port.has(Feature::continuesWithSkill));
}

TEST(DialectVersions, ChangingTheVersionOfTheSameEntryChangesWhatItSpeaks) {
    PortEntry entry = release("0.28.0");

    EXPECT_FALSE(Dialect::of(entry).has(Feature::loadgame));

    entry.version = "0.30.0";

    EXPECT_EQ(Dialect::of(entry).loads, SaveNames::slot);

    entry.version = "0.27.0";

    EXPECT_TRUE(Dialect::of(entry).has(Feature::continuesWithSkill));

    entry.version.clear();

    EXPECT_FALSE(Dialect::of(entry).has(Feature::continuesWithSkill));
    EXPECT_EQ(Dialect::of(entry).loads, SaveNames::slot);
}

TEST(DialectVersions, TheConfigSpeaksTheVersionOfThePortTheProfileRuns) {
    Config config = launch::single("/games/dsda-doom");

    config.ports.front().version = "0.27.0";

    EXPECT_TRUE(Dialect::of(config).has(Feature::continuesWithSkill));
    EXPECT_TRUE(Dialect::of(config, config.activeProfile()).has(Feature::continuesWithSkill));
}

namespace {

static_assert(static_cast<unsigned>(Feature::count) <= 64);
static_assert((Feature::demos | Feature::longtics).has(Feature::longtics));
static_assert(!(Feature::demos | Feature::longtics).has(Feature::soloNet));
static_assert(((Feature::demos | Feature::longtics) & ~Dialect::Features(Feature::demos)).has(Feature::longtics));
static_assert(!((Feature::demos | Feature::longtics) & ~Dialect::Features(Feature::demos)).has(Feature::demos));

constexpr Dialect::Features DSDA_VERSIONED = Feature::loadgame | Feature::recordsWithSkill | Feature::continuesWithSkill
                                             | Feature::recordsWithComplevel | Feature::pistolStart;

struct Expected {
    std::string_view first;
    bool loadgame;
    bool recordsWithSkill;
    bool continuesWithSkill;
    bool recordsWithComplevel;
    bool pistolStart;
    bool withoutRules;
};

constexpr std::array DSDA_EXPECTED = std::to_array<Expected>({
        {"0.30.0", true, true, false, true, true, false},
        {"0.28.0", false, true, false, true, true, false},
        {"0.27.0", false, true, true, true, true, false},
        {"0.26.0", false, false, false, true, true, false},
        {"0.25.0", false, false, false, true, true, true},
        {"0.22.3", false, false, false, false, true, true},
        {"0.21.2", true, false, false, false, true, true},
        {"0.21.1", true, false, false, false, false, true},
});

constexpr std::uint32_t CHAIN_AND_SPEED =
        Dialect::ruleBit(Rules::Rule::chainEpisodes) | Dialect::ruleBit(Rules::Rule::speed);

void markSpecial(Dialect::Port &port) {
    port.lastSlot = 7;
}

}

TEST(DialectFeatures, MasksCombineAndTakeAwayInAConstantExpression) {
    constexpr Dialect::Features both = Feature::levelstat | Feature::cheats;

    static_assert(both.has(Feature::levelstat));
    static_assert(both.has(Feature::cheats));
    static_assert(!both.has(Feature::shotdir));
    static_assert(Dialect::Features().bits == 0U);

    EXPECT_EQ((both & ~Dialect::Features(Feature::cheats)), Dialect::Features(Feature::levelstat));
}

TEST(DialectFeatures, ARangeTakesAwayWhatItLacksAndAddsWhatItGains) {
    const Dialect::Port base{.features = Feature::loadgame | Feature::levelstat | Feature::cheats};
    const Dialect::VersionRange range{
            .first = "1",
            .label = "one",
            .lacks = Feature::loadgame | Feature::shotdir,
            .gains = Feature::continuesWithSkill,
            .withoutRules = CHAIN_AND_SPEED,
            .special = nullptr,
    };

    const Dialect::Port at = Dialect::atRelease(base, range);

    EXPECT_FALSE(at.has(Feature::loadgame));
    EXPECT_TRUE(at.has(Feature::levelstat));
    EXPECT_TRUE(at.has(Feature::cheats));
    EXPECT_TRUE(at.has(Feature::continuesWithSkill));
    EXPECT_FALSE(at.has(Feature::shotdir));
    EXPECT_EQ(at.withoutRules, CHAIN_AND_SPEED);
    EXPECT_EQ(at.lastSlot, base.lastSlot);
}

TEST(DialectFeatures, ARangeAddsItsRulesToThoseAlreadyMissing) {
    Dialect::Port base;

    base.withoutRules = Dialect::ruleBit(Rules::Rule::chainEpisodes);

    const Dialect::VersionRange range{
            .first = "1",
            .label = "one",
            .lacks = {},
            .gains = {},
            .withoutRules = Dialect::ruleBit(Rules::Rule::speed),
            .special = nullptr,
    };

    EXPECT_EQ(Dialect::atRelease(base, range).withoutRules, CHAIN_AND_SPEED);
}

TEST(DialectFeatures, ARangeRunsItsSpecialCaseAfterTheMasks) {
    const Dialect::VersionRange range{
            .first = "1",
            .label = "one",
            .lacks = Feature::cheats,
            .gains = {},
            .withoutRules = 0,
            .special = markSpecial,
    };
    const Dialect::Port at = Dialect::atRelease(Dialect::Port{.features = Feature::cheats}, range);

    EXPECT_EQ(at.lastSlot, 7);
    EXPECT_FALSE(at.has(Feature::cheats));
}

TEST(DialectFeatures, EveryDialectAgreesWithTheValuesItDerivesFrom) {
    std::vector<Dialect::Port> all;

    for (const char *program : {
                 "gzdoom",         "uzdoom",
                 "zdoom",          "zandronum",
                 "dsda-doom",      "nyan-doom",
                 "prboom-plus",    "woof",
                 "nugget-doom",    "cherry-doom",
                 "chocolate-doom", "crispy-doom",
                 "crispy-heretic", "chocolate-strife",
                 "eternity",       "helion",
                 "boom",           "mbf",
                 "legacy",         "doom2",
                 "mystery-engine",
         }) {
        all.push_back(named(program));
    }

    for (const Dialect::Choice &each : Dialect::choices()) {
        all.push_back(Dialect::of("/games/mystery-engine", each.word));
    }

    for (const char *program : {"mbf.exe", "doom.exe", "legacy.exe", "mystery.exe", "gzdoom.exe"}) {
        all.push_back(Dialect::of(launch::single(std::string("/games/") + program, true)));
    }

    for (const Dialect::Port &each : all) {
        EXPECT_EQ(Dialect::defined(each).features, each.features) << each.configName;
        EXPECT_EQ(each.has(Feature::saveFolder), !each.save.empty()) << each.configName;
        EXPECT_EQ(each.has(Feature::hosts), each.netgame == Netgames::zdoom || each.netgame == Netgames::chocolate)
                << each.configName;
        EXPECT_EQ(each.has(Feature::joins), each.netgame != Netgames::none) << each.configName;
        EXPECT_EQ(each.has(Feature::listing), each.netgame == Netgames::chocolate) << each.configName;
        EXPECT_EQ(each.has(Feature::players), each.netgame == Netgames::zdoom) << each.configName;
        EXPECT_EQ(each.has(Feature::deathmatch3), each.netgame == Netgames::chocolate && !each.deathmatch3.empty())
                << each.configName;

        if (each.netgame != Netgames::zdoom) {
            EXPECT_FALSE(each.has(Feature::netmode) || each.has(Feature::teamPick) || each.has(Feature::netgameFolder))
                    << each.configName;
        }

        if (!each.has(Feature::demos)) {
            EXPECT_FALSE(each.has(Feature::timedemo) || each.has(Feature::fastdemo) || each.has(Feature::continues)
                         || each.has(Feature::longtics) || each.has(Feature::soloNet))
                    << each.configName;
        }
    }
}

TEST(DialectFeatures, TheNamesAreTriedInTheOrderTheDialectsNeedIt) {
    EXPECT_EQ(named("gzdoom").id, Dialect::Id::gzdoom);
    EXPECT_EQ(named("uzdoom").id, Dialect::Id::uzdoom);
    EXPECT_EQ(named("zdoom").id, Dialect::Id::zdoom);
    EXPECT_EQ(named("zandronum").id, Dialect::Id::zandronum);
    EXPECT_EQ(named("boom").id, Dialect::Id::boom);
    EXPECT_EQ(named("prboom-plus").id, Dialect::Id::prboom);
    EXPECT_EQ(named("crispy-doom").id, Dialect::Id::chocolate);
    EXPECT_EQ(named("crispy-doom").line, PortLine::crispy);
    EXPECT_EQ(named("nugget-doom").id, Dialect::Id::woof);
    EXPECT_EQ(named("nyan-doom").id, Dialect::Id::dsda);
    EXPECT_EQ(named("doom").id, Dialect::Id::doom);
}

TEST(DialectVersions, EveryRangeOfDsdaDoomSpeaksWhatItDidBeforeTheTable) {
    const Dialect::Port latest = Dialect::of(release(""));
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release(""));

    ASSERT_EQ(ranges.size(), DSDA_EXPECTED.size());

    for (size_t at = 0; at < ranges.size(); ++at) {
        const Expected &want = DSDA_EXPECTED[at];
        const Dialect::Port port = Dialect::of(release(std::string(want.first)));

        EXPECT_EQ(ranges[at].first, want.first);
        EXPECT_EQ(port.has(Feature::loadgame), want.loadgame) << want.first;
        EXPECT_EQ(port.has(Feature::recordsWithSkill), want.recordsWithSkill) << want.first;
        EXPECT_EQ(port.has(Feature::continuesWithSkill), want.continuesWithSkill) << want.first;
        EXPECT_EQ(port.has(Feature::recordsWithComplevel), want.recordsWithComplevel) << want.first;
        EXPECT_EQ(port.has(Feature::pistolStart), want.pistolStart) << want.first;
        EXPECT_EQ(port.withoutRules, want.withoutRules ? CHAIN_AND_SPEED : 0U) << want.first;

        EXPECT_EQ(port.features & ~DSDA_VERSIONED, latest.features & ~DSDA_VERSIONED) << want.first;
        EXPECT_EQ(port.loads, latest.loads) << want.first;
        EXPECT_EQ(port.pistolStart, latest.pistolStart) << want.first;
        EXPECT_EQ(port.lastSlot, latest.lastSlot) << want.first;
        EXPECT_EQ(port.line, latest.line) << want.first;
    }
}

TEST(DialectVersions, TheRowsOfDsdaDoomAreNewestFirstAndNamedByTheirFirstRelease) {
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release(""));
    std::vector<std::string_view> firsts;

    for (const Dialect::VersionRange &each : ranges) {
        firsts.push_back(each.first);
        EXPECT_TRUE(each.label.contains(each.first)) << each.first;
        EXPECT_EQ(each.special, nullptr) << each.first;
    }

    EXPECT_TRUE(std::ranges::is_sorted(firsts, std::greater<>(), [](const std::string_view each) {
        std::vector<int> parts;

        for (const auto piece : each | std::views::split('.')) {
            parts.push_back(std::stoi(std::string(piece.begin(), piece.end())));
        }

        return parts;
    }));
}
