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
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Launch.h"
#include "core/launch/Dialect.h"
#include "core/launch/KnownPorts.h"
#include "core/launch/dialects/Dialects.h"
#include "core/ports/Catalog.h"

namespace {

using Dialect::Complevels;
using Dialect::Feature;
using Dialect::Netgames;
using Dialect::SaveNames;

constexpr Dialect::Features CHAIN_AND_SPEED = Feature::chainEpisodes | Feature::gameSpeed;

Dialect::Traits named(const char *file) {
    return Dialect::of(std::filesystem::path("/games") / file);
}

// Empty for a port without the feature.
std::string_view spelled(const Dialect::Traits &traits, const Feature feature) {
    return traits.has(feature) ? Dialect::arg(traits, feature).name : std::string_view();
}

bool holds(const std::span<const int> levels, const int level) {
    return std::ranges::find(levels, level) != levels.end();
}

}

TEST(DialectAnswerKey, TheKeyRefersToTheStoredFieldsOfTheEngine) {
    const Engine engine{.name = "GZ", .file = "/opt/gzdoom", .port = Port::Port::gzdoom, .version = "4.12.1"};
    const auto key = Dialect::answerKey(engine);

    static_assert(std::is_same_v<decltype(Dialect::answerKey(engine)),
                                 std::tuple<const std::string &,
                                            const Port::Port &,
                                            const std::string &,
                                            const bool &,
                                            const bool &>>);
    EXPECT_EQ(&std::get<0>(key), &engine.file);
    EXPECT_EQ(&std::get<2>(key), &engine.version);
}

TEST(DialectOf, AnUnknownNameIsTakenForTheZdoomFamilyAndMarkedAGuess) {
    const Dialect::Traits traits = named("mystery-engine");

    EXPECT_FALSE(traits.has(Feature::recognised));
    EXPECT_TRUE(traits.has(Feature::iwad));
    EXPECT_EQ(spelled(traits, Feature::saveFolder), "-savedir");
    EXPECT_EQ(traits.loads, SaveNames::name);
    EXPECT_EQ(traits.netgame, Netgames::zdoom);
    EXPECT_TRUE(traits.has(Feature::exec));
    EXPECT_TRUE(traits.has(Feature::map));
}

TEST(DialectOf, GzdoomIsTheZdoomFamilyLoadingSavesByName) {
    const Dialect::Traits traits = named("gzdoom");

    EXPECT_EQ(traits.loads, SaveNames::name);
    EXPECT_EQ(traits.saveExt, ".zds");
    EXPECT_EQ(spelled(traits, Feature::deh), "-deh");
    EXPECT_EQ(spelled(traits, Feature::bex), "-deh");
    EXPECT_TRUE(traits.has(Feature::netmode));
    EXPECT_FALSE(traits.has(Feature::dos));
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

#ifdef __linux__
TEST(DialectOf, AFlatpakExportIsNamedByTheLastPartOfItsAppId) {
    const std::filesystem::path exports = "/var/lib/flatpak/exports/bin";

    EXPECT_EQ(Dialect::of(exports / "io.github.fabiangreffrath.woof").complevel, Complevels::woof);
    EXPECT_EQ(Dialect::of(exports / "io.github.kraflab.dsda-doom").complevel, Complevels::dsda);
    EXPECT_FALSE(Dialect::of(exports / "org.zdoom.UZDoom").has(Feature::netmode));
    EXPECT_EQ(Dialect::of("/home/player/.local/share/flatpak/exports/bin/org.chocolate_doom.ChocolateDoom").netgame,
              Netgames::chocolate);
}

TEST(DialectOf, TheCrispyFlatpakIsNamedByThePortItRuns) {
    const Dialect::Traits traits = Dialect::of("/var/lib/flatpak/exports/bin/io.github.fabiangreffrath.Doom");

    EXPECT_TRUE(traits.has(Feature::recognised));
    EXPECT_EQ(traits.dialect, Dialect::Dialect::chocolate);
    EXPECT_EQ(traits.complevel, Complevels::gameVersion);
    EXPECT_EQ(traits.configName, "crispy");
    EXPECT_EQ(traits.line, PortLine::crispy);
    EXPECT_TRUE(traits.has(Feature::levelstat));
    EXPECT_EQ(spelled(traits, Feature::bex), "-deh");
}
#endif

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
    const Dialect::Traits traits = named("chocolate-doom");

    EXPECT_TRUE(traits.has(Feature::extraConfig));
    EXPECT_EQ(traits.netgame, Netgames::chocolate);
    EXPECT_EQ(traits.loads, SaveNames::slot);
    EXPECT_FALSE(traits.has(Feature::levelstat));
}

TEST(DialectOf, CrispyIsChocolateThatWritesALevelstat) {
    const Dialect::Traits traits = named("crispy-doom");

    EXPECT_TRUE(traits.has(Feature::extraConfig));
    EXPECT_TRUE(traits.has(Feature::levelstat));
}

TEST(DialectOf, ChocolateMergesAddOnsWhereCrispyMergesOnFile) {
    EXPECT_EQ(spelled(named("chocolate-doom"), Feature::pwads), "-merge");
    EXPECT_EQ(spelled(named("chocolate-heretic"), Feature::pwads), "-merge");
    EXPECT_EQ(spelled(named("crispy-doom"), Feature::pwads), "-file");
    EXPECT_EQ(spelled(named("gzdoom"), Feature::pwads), "-file");
    EXPECT_EQ(spelled(Dialect::of("/games/mystery-engine", Port::Port::chocolate), Feature::pwads), "-merge");
}

TEST(DialectOf, TheWoofLineSpeaksAlikeAtItsLatestReleases) {
    for (const char *file : {"woof", "nugget-doom", "cherry-doom"}) {
        const Dialect::Traits traits = named(file);

        EXPECT_EQ(traits.netgame, Netgames::chocolate) << file;
        EXPECT_EQ(spelled(traits, Feature::saveFolder), "-save") << file;
        EXPECT_EQ(traits.lastSlot, 77) << file;
        EXPECT_EQ(traits.autosave, "autosave.dsg") << file;
    }
}

TEST(DialectOf, OnlyCherryLeavesOutId24) {
    EXPECT_EQ(named("woof").complevel, Complevels::woof);
    EXPECT_EQ(named("nugget-doom").complevel, Complevels::woof);
    EXPECT_EQ(named("cherry-doom").complevel, Complevels::woofToMbf21);
}

TEST(DialectOf, OnlyNuggetAndCherryReadASavegameDir) {
    EXPECT_FALSE(named("woof").has(Feature::savegameDir));
    EXPECT_TRUE(named("nugget-doom").has(Feature::savegameDir));
    EXPECT_TRUE(named("cherry-doom").has(Feature::savegameDir));
}

TEST(DialectOf, CherrySetsNoRulesForRecordingAndPlaysNoId24) {
    const Dialect::Traits cherry = named("cherry-doom");

    EXPECT_FALSE(cherry.has(Feature::recordsWithComplevel));
    EXPECT_FALSE(cherry.has(Feature::recordsWithSkill));
    EXPECT_FALSE(cherry.has(Feature::continueComplevel));
    EXPECT_TRUE(cherry.has(Feature::continues));
    EXPECT_FALSE(holds(Dialect::complevels(cherry.complevel), Dialect::ID24));
}

TEST(DialectOf, DsdaLoadsBySlotAndPlaysNoNetgame) {
    const Dialect::Traits traits = named("dsda-doom");

    EXPECT_EQ(traits.loads, SaveNames::slot);
    EXPECT_EQ(traits.lastSlot, 118);
    EXPECT_EQ(traits.netgame, Netgames::none);
    EXPECT_TRUE(traits.has(Feature::levelstat));
}

TEST(DialectOf, NyanDoomIsDsdaWithItsOwnConfig) {
    const Dialect::Traits traits = named("nyan-doom");

    EXPECT_TRUE(traits.has(Feature::recognised));
    EXPECT_EQ(traits.dialect, Dialect::Dialect::dsda);
    EXPECT_EQ(traits.complevel, Complevels::dsda);
    EXPECT_EQ(traits.loads, SaveNames::slot);
    EXPECT_EQ(traits.configName, "nyan");
    EXPECT_EQ(Dialect::of("/games/nyan-doom", Port::Port::unknown).configName, "nyan");
}

TEST(DialectOf, SkulltagTakesFullscreenAsACvarAndLoadsSavesByPath) {
    const Dialect::Traits traits = named("skulltag");

    EXPECT_TRUE(traits.has(Feature::recognised));
    EXPECT_EQ(traits.port, Port::Port::skulltag);
    EXPECT_EQ(traits.dialect, Dialect::Dialect::zandronum);
    EXPECT_EQ(traits.loads, SaveNames::path);
    EXPECT_EQ(spelled(traits, Feature::fullscreen), "+fullscreen");
    EXPECT_EQ(Dialect::arg(traits, Feature::fullscreen).word, "1");
    EXPECT_EQ(spelled(traits, Feature::windowed), "+fullscreen");
    EXPECT_EQ(Dialect::arg(traits, Feature::windowed).word, "0");
    EXPECT_EQ(spelled(traits, Feature::bex), "-deh");
    EXPECT_EQ(spelled(traits, Feature::cheats), "+sv_cheats");
    EXPECT_EQ(spelled(traits, Feature::alwaysApplyDmflags), "+alwaysapplydmflags");
    EXPECT_EQ(spelled(traits, Feature::stdoutSwitch), "-stdout");
    EXPECT_FALSE(traits.has(Feature::pistolStart));
    EXPECT_EQ(named("Skulltag.exe").dialect, Dialect::Dialect::zandronum);
}

TEST(DialectOf, SkulltagReadsDmflags3AndNoZandronumWords) {
    const Dialect::Traits traits = named("skulltag");

    EXPECT_EQ(traits.line, PortLine::skulltag);

    for (const Feature word : {Feature::dmflags,
                               Feature::dmflags2,
                               Feature::dmflags3,
                               Feature::compatflags,
                               Feature::compatflags2,
                               Feature::lmsWeapons,
                               Feature::lmsSpectators,
                               Feature::signedWords}) {
        EXPECT_TRUE(traits.has(word)) << static_cast<int>(word);
    }

    for (const Feature lacking : {Feature::zadmflags,
                                  Feature::zacompatflags,
                                  Feature::corpseGibs,
                                  Feature::keepsCompatflags,
                                  Feature::twoStateFreelook}) {
        EXPECT_FALSE(traits.has(lacking)) << static_cast<int>(lacking);
    }
}

TEST(DialectOf, SkulltagKeepsItsFilesBesideItselfOnWindowsAndInDotFoldersElsewhere) {
    const Dialect::Traits traits = named("skulltag");

    EXPECT_TRUE(traits.has(Feature::programFolderOnly));
    EXPECT_TRUE(traits.has(Feature::homeDotFolder));
    EXPECT_TRUE(traits.has(Feature::savesWhereRun));
    EXPECT_TRUE(traits.has(Feature::saveDirSetting));
    EXPECT_FALSE(named("zandronum").has(Feature::homeDotFolder));
    EXPECT_FALSE(named("zandronum").has(Feature::programFolderOnly));
}

TEST(DialectOf, ZandronumAndSkulltagHostAServerAndConnectToOne) {
    for (const char *file : {"zandronum", "skulltag"}) {
        const Dialect::Traits traits = named(file);

        EXPECT_TRUE(traits.has(Feature::hostsServer)) << file;
        EXPECT_EQ(spelled(traits, Feature::joins), "-connect") << file;
        EXPECT_EQ(spelled(traits, Feature::hostPlayers), "-host") << file;
        EXPECT_FALSE(traits.has(Feature::teamPick)) << file;

        for (const Feature tuning : {Feature::netmode, Feature::dup, Feature::extratic}) {
            EXPECT_FALSE(traits.has(tuning)) << file << " " << static_cast<int>(tuning);
        }
    }

    for (const char *file : {"gzdoom", "zdoom", "uzdoom", "vkdoom"}) {
        const Dialect::Traits traits = named(file);

        EXPECT_FALSE(traits.has(Feature::hostsServer)) << file;
        EXPECT_TRUE(traits.has(Feature::dup)) << file;
        EXPECT_TRUE(traits.has(Feature::extratic)) << file;
        EXPECT_EQ(spelled(traits, Feature::joins), "-join") << file;
    }
}

TEST(DialectOf, APlayerCountStopsAtThePlayerLimitOfThePort) {
    const auto most = [](const char *file) { return Dialect::arg(named(file), Feature::hostPlayers).range; };

    EXPECT_EQ(most("gzdoom"), (Dialect::Range{.least = 1, .most = 8}));
    EXPECT_EQ(most("zandronum"), (Dialect::Range{.least = 1, .most = 64}));
    EXPECT_EQ(most("skulltag"), (Dialect::Range{.least = 1, .most = 32}));
    EXPECT_EQ(most("vkdoom"), (Dialect::Range{.least = 1, .most = 8}));
    EXPECT_EQ(most("uzdoom"), (Dialect::Range{.least = 1, .most = 64}));

    for (const auto &[version, limit] : {std::pair{"5.0.0", 64}, std::pair{"4.14.3", 8}}) {
        const Dialect::Traits traits = Dialect::of(Engine{.name = "Port", .file = "/games/uzdoom", .version = version});

        EXPECT_EQ(Dialect::arg(traits, Feature::hostPlayers).range, (Dialect::Range{.least = 1, .most = limit}))
                << version;
    }
}

TEST(DialectOf, OnlySkulltagHasNoColorSet) {
    EXPECT_FALSE(named("skulltag").has(Feature::colorSet));

    for (const char *file : {"gzdoom", "uzdoom", "zdoom", "zandronum", "vkdoom"}) {
        EXPECT_TRUE(named(file).has(Feature::colorSet)) << file;
    }
}

TEST(DialectOf, VkdoomSpeaksTheNewestGzdoom) {
    const Dialect::Traits traits = named("vkdoom");
    const Dialect::Traits gzdoom = named("gzdoom");

    EXPECT_EQ(traits.port, Port::Port::vkdoom);
    EXPECT_EQ(traits.dialect, Dialect::Dialect::gzdoom);
    EXPECT_EQ(traits.line, PortLine::gzdoom);
    EXPECT_EQ(traits.features, gzdoom.features);
    EXPECT_EQ(traits.loads, SaveNames::name);
    EXPECT_EQ(spelled(traits, Feature::pistolStart), "+sv_pistolstart");
    EXPECT_EQ(spelled(traits, Feature::fullscreen), "+vid_fullscreen");
    EXPECT_EQ(spelled(traits, Feature::netmode), "-netmode");
    EXPECT_EQ(spelled(traits, Feature::joins), "-join");
}

TEST(DialectOf, ThePrboomLineOnlyJoinsAndPatchesBexThroughDeh) {
    for (const char *file : {"prboom-plus", "glboom-plus", "prboom", "glboom", "rude"}) {
        const Dialect::Traits traits = named(file);

        EXPECT_EQ(traits.netgame, Netgames::prboom) << file;
        EXPECT_EQ(traits.complevel, Complevels::prboom) << file;
        EXPECT_EQ(spelled(traits, Feature::bex), "-deh") << file;
        EXPECT_FALSE(traits.has(Feature::exec)) << file;
    }
}

TEST(DialectOf, BoomKeepsItsOwnConfigWhereMbfTakesOne) {
    EXPECT_FALSE(named("boom").has(Feature::configFile));
    EXPECT_TRUE(named("mbf").has(Feature::configFile));
    EXPECT_TRUE(named("winmbf").has(Feature::configFile));
    EXPECT_EQ(named("boom").complevel, Complevels::none);
}

TEST(DialectOf, HelionCannotRespawnOrTimeADemo) {
    const Dialect::Traits traits = named("helion");

    EXPECT_FALSE(traits.has(Feature::respawn));
    EXPECT_FALSE(traits.has(Feature::timedemo));
    EXPECT_EQ(traits.saveExt, ".hsg");
}

TEST(DialectOf, HelionWritesALevelstatAndRecordsToTheExactPath) {
    const Dialect::Traits traits = named("helion");

    EXPECT_TRUE(traits.has(Feature::levelstat));
    EXPECT_FALSE(traits.has(Feature::recordAddsLmp));
    EXPECT_EQ(traits.complevel, Complevels::helion);
    EXPECT_TRUE(named("gzdoom").has(Feature::recordAddsLmp));
}

TEST(DialectOf, EternityHasNoNetgame) {
    EXPECT_EQ(named("eternity").netgame, Netgames::none);
    EXPECT_EQ(spelled(named("eternity"), Feature::deh), "-deh");
    EXPECT_EQ(spelled(named("eternity"), Feature::bex), "-deh");
}

TEST(DialectOf, ThePlainDosReleasesTakeNoIwadNoPatchAndNoSaveFolder) {
    for (const char *file : {"doom", "doom2", "doomu", "doom95", "dosdoom"}) {
        const Dialect::Traits traits = named(file);

        EXPECT_FALSE(traits.has(Feature::iwad)) << file;
        EXPECT_FALSE(traits.has(Feature::deh)) << file;
        EXPECT_FALSE(traits.has(Feature::saveFolder)) << file;
        EXPECT_FALSE(traits.has(Feature::configFile)) << file;
    }
}

TEST(DialectOf, DoomLegacyPatchesThroughDehacked) {
    for (const char *file : {"doom3", "legacy", "doomlegacy"}) {
        const Dialect::Traits traits = named(file);

        EXPECT_EQ(spelled(traits, Feature::deh), "-dehacked") << file;
        EXPECT_EQ(spelled(traits, Feature::bex), "-dehacked") << file;
        EXPECT_FALSE(traits.has(Feature::iwad)) << file;
    }
}

TEST(DialectOf, ThePlainNamesMatchOnlyWhole) {
    EXPECT_FALSE(named("doom2").has(Feature::deh));
    EXPECT_FALSE(named("doom2-plus").has(Feature::recognised));
}

TEST(DialectOfConfig, AProfileWithNoPortGetsTheDefaults) {
    Config config = launch::single("/games/gzdoom");

    config.activeProfile().engine = "Nothing";

    const Dialect::Traits traits = Dialect::of(config);

    EXPECT_FALSE(traits.has(Feature::dos));
    EXPECT_TRUE(traits.has(Feature::recognised));
    EXPECT_EQ(traits.netgame, Netgames::zdoom);
}

TEST(DialectOfConfig, ANativePortIsAnsweredForByItsName) {
    const Dialect::Traits traits = Dialect::of(launch::single("/games/dsda-doom"));

    EXPECT_FALSE(traits.has(Feature::dos));
    EXPECT_EQ(traits.complevel, Complevels::dsda);
}

TEST(DialectOfConfig, UnderDosboxNothingAfterTheLastDosReleaseApplies) {
    const Dialect::Traits traits = Dialect::of(launch::single("/games/MBF.EXE", true));

    EXPECT_TRUE(traits.has(Feature::dos));
    EXPECT_TRUE(traits.has(Feature::recognised));
    EXPECT_TRUE(traits.has(Feature::configFile));
    EXPECT_FALSE(traits.has(Feature::saveFolder));
    EXPECT_FALSE(traits.has(Feature::exec));
    EXPECT_FALSE(traits.has(Feature::map));
    EXPECT_EQ(traits.netgame, Netgames::none);
    EXPECT_EQ(traits.complevel, Complevels::none);
    EXPECT_FALSE(traits.has(Feature::longtics));
    EXPECT_FALSE(traits.has(Feature::soloNet));
    EXPECT_FALSE(traits.has(Feature::levelstat));
    EXPECT_FALSE(traits.has(Feature::extraConfig));
}

TEST(DialectOfConfig, AnUnknownNameUnderDosboxIsVanilla) {
    const Dialect::Traits traits = Dialect::of(launch::single("/games/HACKED.EXE", true));

    EXPECT_TRUE(traits.has(Feature::dos));
    EXPECT_FALSE(traits.has(Feature::recognised));
    EXPECT_FALSE(traits.has(Feature::iwad));
    EXPECT_FALSE(traits.has(Feature::deh));
    EXPECT_FALSE(traits.has(Feature::configFile));
}

TEST(DialectOfConfig, TheProfileGivenIsTheOneAnsweredFor) {
    Config config = launch::single("/games/gzdoom");

    config.engines.push_back({.name = "Other", .file = "/games/woof"});

    Profile other = config.activeProfile();

    other.engine = "Other";

    EXPECT_EQ(Dialect::of(config, other).complevel, Complevels::woof);
    EXPECT_EQ(Dialect::of(config).complevel, Complevels::none);
}

TEST(DialectComplevels, EveryTableLeavesTheChoiceToThePort) {
    for (const Complevels which : {Complevels::woof,
                                   Complevels::woofToMbf21,
                                   Complevels::prboom,
                                   Complevels::dsda,
                                   Complevels::helion,
                                   Complevels::gameVersion19,
                                   Complevels::gameVersion1666,
                                   Complevels::gameVersion12,
                                   Complevels::gameVersion}) {
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

TEST(DialectComplevels, TheWoofLevelsUpToMbf21LeaveOnlyId24Out) {
    EXPECT_EQ(std::ranges::to<std::vector>(Dialect::complevels(Complevels::woofToMbf21)),
              (std::vector<int>{-1, 2, 3, 4, 9, 11, 21}));
    EXPECT_EQ(Dialect::pickedLevel(Complevels::woofToMbf21, Dialect::ID24), -1);
    EXPECT_EQ(Dialect::complevelWord(Complevels::woofToMbf21, Dialect::MBF21), "21");
}

TEST(DialectComplevels, DsdaAddsMbf21ToThePrboomTable) {
    EXPECT_TRUE(holds(Dialect::complevels(Complevels::dsda), 21));
    EXPECT_FALSE(holds(Dialect::complevels(Complevels::prboom), 21));
    EXPECT_TRUE(holds(Dialect::complevels(Complevels::prboom), 17));
}

TEST(DialectLevels, TheLevelsAreNamedAsTheyAreKnown) {
    EXPECT_EQ(Dialect::levelName(-1), "The port's own");
    EXPECT_EQ(Dialect::levelName(2), "Doom v1.9");
    EXPECT_EQ(Dialect::levelName(9), "Boom v2.02");
    EXPECT_EQ(Dialect::levelName(17), "PrBoom, current");
    EXPECT_EQ(Dialect::levelName(Dialect::MBF21), "MBF21");
    EXPECT_EQ(Dialect::levelName(Dialect::ID24), "id24");
}

TEST(DialectLevels, ANumberThatIsNoLevelHasNoName) {
    EXPECT_EQ(Dialect::levelName(18), "");
    EXPECT_EQ(Dialect::levelName(99), "");
    EXPECT_EQ(Dialect::levelName(Dialect::GAME_VERSIONS + 11), "");
}

TEST(DialectLevels, EveryLevelAPortOffersHasAName) {
    for (const Complevels which : {Complevels::woof,
                                   Complevels::woofToMbf21,
                                   Complevels::prboom,
                                   Complevels::dsda,
                                   Complevels::helion,
                                   Complevels::gameVersion19,
                                   Complevels::gameVersion1666,
                                   Complevels::gameVersion12,
                                   Complevels::gameVersion}) {
        for (const int level : Dialect::complevels(which)) {
            EXPECT_FALSE(Dialect::levelName(level).empty()) << level;
        }
    }
}

TEST(DialectLevels, AGameVersionIsNamedByItsRelease) {
    EXPECT_EQ(Dialect::levelName(Dialect::GAME_VERSIONS), "Doom 1.2");
    EXPECT_EQ(Dialect::levelName(Dialect::GAME_VERSIONS + 5), "Doom 1.9");
}

TEST(DialectLevels, APickedLevelIsOneThePortReads) {
    EXPECT_EQ(Dialect::pickedLevel(Complevels::dsda, 21), 21);
    EXPECT_EQ(Dialect::pickedLevel(Complevels::prboom, 17), 17);
    EXPECT_EQ(Dialect::pickedLevel(Complevels::gameVersion, Dialect::GAME_VERSIONS + 5), Dialect::GAME_VERSIONS + 5);
}

TEST(DialectLevels, AnUnlistedLevelIsNotPicked) {
    EXPECT_EQ(Dialect::pickedLevel(Complevels::prboom, 21), -1);
    EXPECT_EQ(Dialect::pickedLevel(Complevels::woof, 5), -1);
    EXPECT_EQ(Dialect::pickedLevel(Complevels::gameVersion, 9), -1);
    EXPECT_EQ(Dialect::pickedLevel(Complevels::none, 9), -1);
}

TEST(DialectLevels, MinusOneIsThePortsOwnAndNeverPicked) {
    EXPECT_EQ(Dialect::pickedLevel(Complevels::dsda, -1), -1);
    EXPECT_EQ(Dialect::pickedLevel(Complevels::dsda, -7), -1);
}

TEST(DialectLevels, TheLabelIsTheNameAndTheWordOnTheLine) {
    EXPECT_EQ(Dialect::levelLabel(Complevels::dsda, 9), "Boom v2.02 · 9");
    EXPECT_EQ(Dialect::levelLabel(Complevels::woof, Dialect::ID24), "id24 · 24");
    EXPECT_EQ(Dialect::levelLabel(Complevels::helion, 9), "Boom v2.02 · boom");
    EXPECT_EQ(Dialect::levelLabel(Complevels::gameVersion, Dialect::GAME_VERSIONS + 5), "Doom 1.9 · 1.9");
}

TEST(DialectLevels, ThePortsOwnAndALevelItDoesNotReadHaveNoLabel) {
    EXPECT_EQ(Dialect::levelLabel(Complevels::dsda, -1), "");
    EXPECT_EQ(Dialect::levelLabel(Complevels::woof, 5), "");
    EXPECT_EQ(Dialect::levelLabel(Complevels::none, 9), "");
}

TEST(DialectCompatibility, TheZdoomFamilyPicksAMode) {
    EXPECT_EQ(Dialect::compatibility(named("gzdoom")), Dialect::Compatibility::modes);
    EXPECT_EQ(Dialect::compatibility(named("uzdoom")), Dialect::Compatibility::modes);
    EXPECT_EQ(Dialect::compatibility(named("zandronum")), Dialect::Compatibility::modes);
}

TEST(DialectCompatibility, TheBoomFamilyPicksAComplevel) {
    for (const char *file : {"prboom-plus", "dsda-doom", "woof", "helion"}) {
        EXPECT_EQ(Dialect::compatibility(named(file)), Dialect::Compatibility::complevels) << file;
    }
}

TEST(DialectCompatibility, ChocolateAndCrispyPickAGameVersion) {
    EXPECT_EQ(Dialect::compatibility(named("chocolate-doom")), Dialect::Compatibility::versions);
    EXPECT_EQ(Dialect::compatibility(named("crispy-doom")), Dialect::Compatibility::versions);
}

TEST(DialectCompatibility, APortWithNeitherPicksNothing) {
    EXPECT_EQ(Dialect::compatibility(named("chocolate-hexen")), Dialect::Compatibility::none);
    EXPECT_EQ(Dialect::compatibility(named("doomlegacy")), Dialect::Compatibility::none);
}

TEST(DialectCompatibility, TheListIsTitledByWhatItHolds) {
    EXPECT_EQ(Dialect::levelTitle(Dialect::Compatibility::versions), "Game version");
    EXPECT_EQ(Dialect::levelTitle(Dialect::Compatibility::complevels), "Complevel");
    EXPECT_EQ(Dialect::levelTitle(Dialect::Compatibility::modes), "Complevel");
    EXPECT_EQ(Dialect::levelTitle(Dialect::Compatibility::none), "Complevel");
}

TEST(DialectCompatibility, EachListHasItsOwnHint) {
    const std::string_view modes = Dialect::levelHint(named("gzdoom"));
    const std::string_view versions = Dialect::levelHint(named("chocolate-doom"));
    const std::string_view complevels = Dialect::levelHint(named("dsda-doom"));

    EXPECT_NE(modes, versions);
    EXPECT_NE(modes, complevels);
    EXPECT_NE(versions, complevels);
    EXPECT_EQ(Dialect::levelHint(named("doom2")), complevels);
}

TEST(DialectCompatibility, TheModeHintNamesTheWordsTheModesFill) {
    EXPECT_TRUE(Dialect::levelHint(named("zandronum")).contains("compatflags and compatflags2"));
    EXPECT_TRUE(Dialect::levelHint(named("skulltag")).contains("A mode fills compatflags,"));
    EXPECT_FALSE(Dialect::levelHint(named("skulltag")).contains("compatflags2"));
}

TEST(DialectReplays, APortWithoutReplaysOffersNoneOfTheRest) {
    const Dialect::Traits traits = Dialect::defined({
            .features = Feature::timedemo | Feature::fastdemo | Feature::longtics | Feature::soloNet,
            .complevel = Complevels::dsda,
    });

    EXPECT_FALSE(traits.has(Feature::replays));
    EXPECT_FALSE(traits.has(Feature::timedemo));
    EXPECT_FALSE(traits.has(Feature::fastdemo));
    EXPECT_EQ(Dialect::replayComplevel(traits), Complevels::none);
    EXPECT_FALSE(traits.has(Feature::longtics));
    EXPECT_FALSE(traits.has(Feature::soloNet));
}

TEST(DialectReplays, DsdaRecordsWithEveryOption) {
    const Dialect::Traits traits = named("dsda-doom");

    EXPECT_TRUE(traits.has(Feature::replays));
    EXPECT_TRUE(traits.has(Feature::timedemo));
    EXPECT_TRUE(traits.has(Feature::fastdemo));
    EXPECT_EQ(Dialect::replayComplevel(traits), Complevels::dsda);
    EXPECT_TRUE(traits.has(Feature::longtics));
    EXPECT_TRUE(traits.has(Feature::soloNet));
}

TEST(DialectSaves, AFolderNeedsASaveSwitch) {
    EXPECT_TRUE(named("gzdoom").has(Feature::saveFolder));
    EXPECT_FALSE(named("doom2").has(Feature::saveFolder));
    EXPECT_TRUE(named("prboom-plus").loadsBy(SaveNames::slot));
}

TEST(DialectNet, TheZdoomFamilyHasEveryCvar) {
    const Dialect::Traits net = named("gzdoom");

    EXPECT_TRUE(net.has(Feature::hosts));
    EXPECT_TRUE(net.has(Feature::joins));
    EXPECT_TRUE(net.has(Feature::hostPlayers));
    EXPECT_TRUE(net.has(Feature::fragLimit));
    EXPECT_EQ(spelled(net, Feature::timeLimit), "+set");
    EXPECT_EQ(Dialect::arg(net, Feature::timeLimit).word, "timelimit");
    EXPECT_TRUE(net.has(Feature::savegame));
    EXPECT_TRUE(net.has(Feature::extratic));
    EXPECT_TRUE(net.has(Feature::netmode));
    EXPECT_TRUE(net.has(Feature::dup));
    EXPECT_FALSE(net.has(Feature::hostListed));
}

TEST(DialectNet, ChocolateHostsAndListsButSetsNoCvars) {
    const Dialect::Traits net = named("chocolate-doom");

    EXPECT_TRUE(net.has(Feature::hosts));
    EXPECT_TRUE(net.has(Feature::joins));
    EXPECT_TRUE(net.has(Feature::hostListed));
    EXPECT_TRUE(net.has(Feature::dup));
    EXPECT_FALSE(net.has(Feature::hostPlayers));
    EXPECT_EQ(spelled(net, Feature::timeLimit), "-timer");
    EXPECT_TRUE(Dialect::arg(net, Feature::timeLimit).word.empty());
    EXPECT_FALSE(net.has(Feature::extratic));
    EXPECT_FALSE(net.has(Feature::netmode));
}

TEST(DialectNet, PrboomOnlyJoins) {
    const Dialect::Traits net = named("prboom-plus");

    EXPECT_FALSE(net.has(Feature::hosts));
    EXPECT_TRUE(net.has(Feature::joins));
    EXPECT_FALSE(net.has(Feature::dup));
}

TEST(DialectNet, NoneOffersNothing) {
    const Dialect::Traits net = named("dsda-doom");

    EXPECT_FALSE(net.has(Feature::hosts));
    EXPECT_FALSE(net.has(Feature::joins));
    EXPECT_FALSE(net.has(Feature::hostListed));
}

TEST(DialectNet, OnlyUzdoomKeepsNetgameSavesInAFolderApart) {
    EXPECT_TRUE(named("uzdoom").has(Feature::netgameFolder));
    EXPECT_TRUE(named("uzdoom").has(Feature::netgameFolder));
    EXPECT_FALSE(named("gzdoom").has(Feature::netgameFolder));
    EXPECT_FALSE(named("zandronum").has(Feature::netgameFolder));
}

TEST(DialectPicks, EveryCatalogPortIsAPickThatAgreesOnDos) {
    for (const Catalog::Download &listed : Catalog::downloads()) {
        const std::string_view id = Catalog::id(listed);
        const std::span<const Port::Port> picks = Dialect::picks(listed.dos);

        ASSERT_TRUE(named(std::string(listed.executable).c_str()).has(Feature::recognised)) << id;
        EXPECT_NE(std::ranges::find(picks, listed.port), picks.end()) << id;
    }
}

TEST(DialectChoices, TheExecutableOfEveryCatalogPortNamesItsPort) {
    for (const Catalog::Download &listed : Catalog::downloads()) {
        EXPECT_EQ(named(std::string(listed.executable).c_str()).port, listed.port) << listed.executable;
    }
}

TEST(DialectPicks, EveryPickIsSpokenForAnUnknownName) {
    for (const bool dos : {false, true}) {
        for (const Port::Port each : Dialect::picks(dos)) {
            const std::string_view id = Port::known(each).id;
            const Dialect::Traits traits = Dialect::of("/games/mystery-engine", each);

            EXPECT_EQ(traits.port, each) << id;
            EXPECT_EQ(traits.dialect, Port::known(each).traits->dialect) << id;
            EXPECT_TRUE(traits.has(Feature::recognised)) << id;
        }
    }
}

TEST(DialectPicks, EveryPortWithAnIdIsListedOnce) {
    std::vector<Port::Port> listed;

    for (const bool dos : {false, true}) {
        const std::span<const Port::Port> picks = Dialect::picks(dos);

        listed.insert(listed.end(), picks.begin(), picks.end());
    }

    for (const Port::Known &known : Port::known()) {
        EXPECT_EQ(std::ranges::count(listed, known.port), known.id.empty() ? 0 : 1) << known.name;
        EXPECT_TRUE(known.id.empty() || !known.name.empty()) << known.id;
    }
}

TEST(DialectPicks, EachDialectIsLedByAPortThatSpeaksIt) {
    for (int at = 0; at <= static_cast<int>(Dialect::Dialect::doom); ++at) {
        const auto dialect = static_cast<Dialect::Dialect>(at);
        const Port::Known &lead = Port::known(Dialect::lead(dialect));

        EXPECT_EQ(lead.traits->dialect, dialect) << lead.name;
        EXPECT_FALSE(lead.id.empty()) << lead.name;
    }
}

TEST(DialectPicks, TheListShowsEachLeadThenTheOtherPortsOfItsDialect) {
    std::vector<std::string_view> native;
    std::vector<std::string_view> dos;

    for (const Port::Port each : Dialect::picks(false)) {
        native.push_back(Port::known(each).name);
    }

    for (const Port::Port each : Dialect::picks(true)) {
        dos.push_back(Port::known(each).name);
    }

    EXPECT_EQ(native,
              (std::vector<std::string_view>{"GZDoom",
                                             "VKDoom",
                                             "UZDoom",
                                             "ZDoom",
                                             "Zandronum",
                                             "Skulltag",
                                             "DSDA-Doom",
                                             "Nyan Doom",
                                             "PrBoom+",
                                             "Woof!",
                                             "Nugget Doom",
                                             "Cherry Doom",
                                             "Chocolate Doom",
                                             "Crispy Doom",
                                             "Eternity Engine",
                                             "Helion"}));
    EXPECT_EQ(dos, (std::vector<std::string_view>{"Boom", "MBF", "Doom Legacy", "Vanilla Doom"}));
}

TEST(DialectPicks, TheFileNameGivesItsPortOrTheLeadOfANamelessOne) {
    using Port::Port;

    const auto fromName = [](const char *file, const Port picked = Port::unknown) {
        return Dialect::fromName(Engine{.name = "Port", .file = file, .port = picked});
    };

    EXPECT_EQ(fromName("/games/nugget-doom"), Port::nugget);
    EXPECT_EQ(fromName("/games/nugget-doom", Port::woof), Port::nugget);
    EXPECT_EQ(fromName("/games/glboom-plus"), Port::glboomPlus);
    EXPECT_EQ(fromName("/games/lzdoom"), Port::gzdoom);
    EXPECT_EQ(fromName("/games/mystery-engine"), Port::gzdoom);
    EXPECT_EQ(fromName("/games/chocolate-heretic"), Port::chocolateHeretic);
    EXPECT_EQ(fromName("/games/crispy-strife", Port::crispy), Port::crispyStrife);
}

TEST(DialectPicked, APickedPortWinsOverTheName) {
    const Dialect::Traits traits = Dialect::of("/games/gzdoom", Port::Port::dsda);

    EXPECT_EQ(traits.dialect, Dialect::Dialect::dsda);
    EXPECT_EQ(traits.complevel, Complevels::dsda);
    EXPECT_EQ(traits.configName, "dsda");
}

TEST(DialectPicked, APickedPortWinsOverAnotherOfItsDialect) {
    EXPECT_EQ(Dialect::of("/games/nugget-doom", Port::Port::woof).configName, "woof");
    EXPECT_EQ(Dialect::of("/games/woof", Port::Port::nugget).configName, "nugget");
    EXPECT_FALSE(Dialect::of("/games/crispy-doom", Port::Port::chocolate).has(Feature::levelstat));
    EXPECT_EQ(Dialect::of("/games/dsda-doom", Port::Port::nyan).configName, "nyan");
}

TEST(DialectPicked, APickedPortKeepsItsOwnConfigName) {
    EXPECT_EQ(Dialect::of("/games/mystery-engine", Port::Port::woof).configName, "woof");
    EXPECT_EQ(Dialect::of("/games/mystery-engine", Port::Port::cherry).configName, "cherry");
    EXPECT_EQ(Dialect::of("/games/mystery-engine", Port::Port::prboomPlus).configName, "prboom");
    EXPECT_FALSE(Dialect::of("/games/mystery-engine", Port::Port::chocolate).has(Feature::levelstat));
    EXPECT_TRUE(Dialect::of("/games/mystery-engine", Port::Port::crispy).has(Feature::levelstat));
}

TEST(DialectPicked, NothingPickedLeavesItToTheName) {
    EXPECT_EQ(Dialect::of("/games/woof", Port::Port::unknown).dialect, Dialect::Dialect::woof);
    EXPECT_FALSE(Dialect::of("/games/mystery-engine", Port::Port::unknown).has(Feature::recognised));
}

TEST(DialectPicked, AnEngineHandsItsPortToTheProfile) {
    Config config;
    config.engines.push_back({.name = "Mine", .file = "/games/mystery-engine", .port = Port::Port::woof});
    config.addProfile("x");
    config.activeProfile().engine = "Mine";

    EXPECT_EQ(Dialect::of(config).dialect, Dialect::Dialect::woof);
    EXPECT_EQ(Dialect::of(config).complevel, Complevels::woof);
}

TEST(DialectPicked, AnEngineUnderDosboxKeepsItsPickedPort) {
    Config config;
    config.engines.push_back({.name = "Mine", .file = "/games/run.bat", .dosbox = true, .port = Port::Port::legacy});
    config.addProfile("x");
    config.activeProfile().engine = "Mine";

    const Dialect::Traits traits = Dialect::of(config);

    EXPECT_EQ(traits.dialect, Dialect::Dialect::legacy);
    EXPECT_TRUE(traits.has(Feature::dos));
    EXPECT_EQ(spelled(traits, Feature::deh), "-dehacked");
}

TEST(DialectPicked, AManagedEngineSpeaksTheCatalogPort) {
    const Engine engine{
            .name = "Woof!",
            .file = "/data/ports/woof/engine",
            .port = Port::Port::woof,
            .managed = true,
    };

    EXPECT_EQ(Dialect::of(engine).dialect, Dialect::Dialect::woof);
    EXPECT_EQ(Dialect::of(engine).port, Port::Port::woof);
}

TEST(DialectPicked, ANativeNameUnderDosboxIsTakenForVanilla) {
    const Dialect::Traits traits = Dialect::of(Engine{.name = "Old", .file = "/games/prboom.exe", .dosbox = true});

    EXPECT_EQ(traits.dialect, Dialect::Dialect::doom);
    EXPECT_TRUE(traits.has(Feature::dos));
    EXPECT_FALSE(traits.has(Feature::recognised));
}

TEST(DialectPicked, ADosNameUnderDosboxKeepsItsDialect) {
    EXPECT_EQ(Dialect::of(Engine{.name = "MBF", .file = "/games/mbf.exe", .dosbox = true}).dialect,
              Dialect::Dialect::boom);
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
    EXPECT_EQ(spelled(named("gzdoom"), Feature::pistolStart), "+sv_pistolstart");
    EXPECT_EQ(spelled(named("uzdoom"), Feature::pistolStart), "+sv_pistolstart");
    EXPECT_FALSE(named("zdoom").has(Feature::pistolStart));
    EXPECT_FALSE(named("zandronum").has(Feature::pistolStart));
    EXPECT_EQ(spelled(named("crispy-doom"), Feature::pistolStart), "-pistolstart");
    EXPECT_FALSE(named("chocolate-doom").has(Feature::pistolStart));
    EXPECT_EQ(spelled(named("helion"), Feature::pistolStart), "-pistolstart");

    for (const char *file : {"dsda-doom", "prboom-plus", "woof", "nugget-doom"}) {
        EXPECT_EQ(spelled(named(file), Feature::pistolStart), "-pistolstart") << file;
    }
}

TEST(DialectGameplay, OnlyTheZdoomFamilyNeedsCheatsTurnedOn) {
    for (const char *file : {"gzdoom", "uzdoom", "zdoom", "zandronum"}) {
        EXPECT_TRUE(named(file).has(Feature::cheats)) << file;
    }

    for (const char *file : {"dsda-doom", "woof", "chocolate-doom", "helion"}) {
        EXPECT_FALSE(named(file).has(Feature::cheats)) << file;
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
    for (const char *file : {"gzdoom", "uzdoom", "zandronum", "dsda-doom", "woof", "nugget-doom", "crispy-doom"}) {
        EXPECT_EQ(spelled(named(file), Feature::noAutoload), "-noautoload") << file;
    }

    EXPECT_EQ(spelled(named("prboom-plus"), Feature::noAutoload), "-noload");
    EXPECT_EQ(spelled(named("eternity"), Feature::noAutoload), "-noload");
    EXPECT_FALSE(named("helion").has(Feature::noAutoload));
}

TEST(DialectGameplay, OnlyHelionHasNoSoundEffectsSwitch) {
    for (const char *file : {"gzdoom", "zandronum", "dsda-doom", "woof", "chocolate-doom", "eternity", "helion"}) {
        EXPECT_TRUE(named(file).has(Feature::nomusic)) << file;
    }

    EXPECT_EQ(spelled(named("gzdoom"), Feature::nosfx), "-nosfx");
    EXPECT_EQ(spelled(named("eternity"), Feature::nosfx), "-nosfx");
    EXPECT_FALSE(named("helion").has(Feature::nosfx));
}

TEST(DialectGameplay, EachPortIsToldWindowOrFullscreenItsOwnWay) {
    EXPECT_EQ(spelled(named("gzdoom"), Feature::fullscreen), "+vid_fullscreen");
    EXPECT_EQ(spelled(named("uzdoom"), Feature::windowed), "+vid_fullscreen");
    EXPECT_EQ(spelled(named("zandronum"), Feature::fullscreen), "+fullscreen");
    EXPECT_EQ(spelled(named("prboom-plus"), Feature::fullscreen), "-nowindow");
    EXPECT_EQ(spelled(named("prboom-plus"), Feature::windowed), "-window");
    EXPECT_EQ(spelled(named("dsda-doom"), Feature::fullscreen), "-fullscreen");
    EXPECT_EQ(spelled(named("woof"), Feature::windowed), "-window");
    EXPECT_EQ(spelled(named("crispy-doom"), Feature::fullscreen), "-fullscreen");
    EXPECT_FALSE(named("eternity").has(Feature::fullscreen));
    EXPECT_FALSE(named("eternity").has(Feature::windowed));
    EXPECT_FALSE(named("helion").has(Feature::fullscreen));
    EXPECT_FALSE(named("helion").has(Feature::windowed));

    EXPECT_TRUE(named("gzdoom").has(Feature::displayKept));
    EXPECT_TRUE(named("woof").has(Feature::displayKept));
    EXPECT_FALSE(named("dsda-doom").has(Feature::displayKept));
    EXPECT_FALSE(named("prboom-plus").has(Feature::displayKept));
}

TEST(DialectAdd, WritesTheNameTheWordAndTheValueAsThePortSpellsThem) {
    std::vector<std::string> args;

    EXPECT_TRUE(Dialect::add(args, named("gzdoom"), Feature::fragLimit, "30"));
    EXPECT_TRUE(Dialect::add(args, named("woof"), Feature::windowed));
    EXPECT_TRUE(Dialect::add(args, named("gzdoom"), Feature::windowed));
    EXPECT_EQ(args, (std::vector<std::string>{"+set", "fraglimit", "30", "-window", "+vid_fullscreen", "0"}));
}

TEST(DialectAdd, AFeatureWithNoSpellingWritesNothing) {
    std::vector<std::string> args;

    ASSERT_TRUE(named("gzdoom").has(Feature::hosts));
    EXPECT_FALSE(Dialect::add(args, named("gzdoom"), Feature::hosts, "4"));
    EXPECT_TRUE(args.empty());
}

TEST(DialectAdd, AFeatureThePortLacksWritesNothing) {
    std::vector<std::string> args;

    EXPECT_FALSE(Dialect::add(args, named("helion"), Feature::nosfx));
    EXPECT_FALSE(Dialect::add(args, named("chocolate-doom"), Feature::hostPlayers, "4"));
    EXPECT_FALSE(Dialect::add(args, named("gzdoom"), Feature::hostListed));
    EXPECT_TRUE(args.empty());
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
    const Dialect::Traits traits = Dialect::of(Engine{.name = "MBF", .file = "/games/mbf.exe", .dosbox = true});

    EXPECT_FALSE(traits.has(Feature::pistolStart));
    EXPECT_FALSE(traits.has(Feature::cheats));
    EXPECT_FALSE(traits.has(Feature::shotdir));
    EXPECT_FALSE(traits.has(Feature::continues));
    EXPECT_FALSE(traits.has(Feature::fullscreen));
    EXPECT_FALSE(traits.has(Feature::windowed));
}

TEST(DialectGameplay, EachDosPortTurnsOffMusicAndEffectsItsOwnWay) {
    const auto dos = [](const char *file) { return Dialect::of(Engine{.name = "DOS", .file = file, .dosbox = true}); };

    EXPECT_TRUE(dos("/games/doom.exe").has(Feature::nomusic));
    EXPECT_EQ(spelled(dos("/games/doom.exe"), Feature::nosfx), "-nosfx");
    EXPECT_EQ(spelled(dos("/games/mbf.exe"), Feature::nosfx), "-nosfx");
    EXPECT_TRUE(dos("/games/legacy.exe").has(Feature::nomusic));
    EXPECT_EQ(spelled(dos("/games/legacy.exe"), Feature::nosfx), "-nosound");
}

TEST(DialectGameplay, OnlyMbfAndBoomSkipPreloadsUnderDosbox) {
    const auto dos = [](const char *file) { return Dialect::of(Engine{.name = "DOS", .file = file, .dosbox = true}); };

    EXPECT_EQ(spelled(dos("/games/mbf.exe"), Feature::noAutoload), "-noload");
    EXPECT_EQ(spelled(dos("/games/boom.exe"), Feature::noAutoload), "-noload");
    EXPECT_FALSE(dos("/games/legacy.exe").has(Feature::noAutoload));
    EXPECT_FALSE(dos("/games/doom.exe").has(Feature::noAutoload));
}

TEST(DialectOf, ChocolateAndCrispyListTheDoomReleases) {
    const std::span<const int> levels = Dialect::complevels(Dialect::Complevels::gameVersion);

    ASSERT_EQ(levels.size(), 12U);
    EXPECT_EQ(levels.front(), -1);
    EXPECT_EQ(Dialect::complevelWord(Dialect::Complevels::gameVersion, levels[1]), "1.2");
    EXPECT_EQ(Dialect::complevelWord(Dialect::Complevels::gameVersion, levels.back()), "chex");
    EXPECT_EQ(Dialect::levelName(levels[7]), "Ultimate Doom");
    EXPECT_EQ(Dialect::complevelWord(Dialect::Complevels::gameVersion, 9), "");
    EXPECT_EQ(Dialect::arg(named("chocolate-doom"), Feature::complevel).name, "-gameversion");
    EXPECT_EQ(Dialect::arg(named("crispy-doom"), Feature::complevel).name, "-gameversion");
    EXPECT_EQ(Dialect::arg(named("helion"), Feature::complevel).name, "+complevel");
    EXPECT_EQ(Dialect::arg(named("dsda-doom"), Feature::complevel).name, "-complevel");
    EXPECT_FALSE(named("chocolate-hexen").has(Feature::complevel));
    EXPECT_FALSE(named("boom").has(Feature::complevel));
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

Engine release(const std::string &version, const char *file = "dsda-doom") {
    return Engine{.name = "Port", .file = (std::filesystem::path("/games") / file).string(), .version = version};
}

std::vector<std::string_view> firsts(const std::span<const Dialect::VersionRange> ranges) {
    std::vector<std::string_view> out;

    for (const Dialect::VersionRange &each : ranges) {
        out.push_back(each.first);
    }

    return out;
}

std::vector<std::string_view> labels(const std::span<const Dialect::VersionRange> ranges) {
    std::vector<std::string_view> out;

    for (const Dialect::VersionRange &each : ranges) {
        out.push_back(each.label);
    }

    return out;
}

}

TEST(DialectVersions, DsdaDoomListsItsRangesNewestFirst) {
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release(""));

    EXPECT_EQ(firsts(ranges),
              (std::vector<std::string_view>{
                      "0.30.0", "0.28.3", "0.28.0", "0.27.0", "0.26.0", "0.25.0", "0.22.3", "0.21.2", "0.21.1"}));
    EXPECT_EQ(ranges.front().label, "0.30.0 and later");
    EXPECT_EQ(ranges[1].label, "0.28.3 to 0.29.4");
    EXPECT_EQ(ranges[2].label, "0.28.0 to 0.28.2");
    EXPECT_EQ(ranges[6].label, "0.22.3 to 0.24.3");
    EXPECT_EQ(ranges.back().label, "0.21.1 and earlier");
}

TEST(DialectVersions, TheRangesDoNotDependOnTheVersionStored) {
    EXPECT_EQ(firsts(Dialect::versions(release("0.25.0"))), firsts(Dialect::versions(release(""))));
    EXPECT_EQ(firsts(Dialect::versions(release("garbage"))), firsts(Dialect::versions(release(""))));
}

TEST(DialectVersions, EveryOtherPortHasNoRanges) {
    for (const char *file : {
                 "lzdoom",
                 "qzdoom",
                 "zdoom-fork",
                 "prboom",
                 "glboom",
                 "rude",
                 "mystery-engine",
         }) {
        EXPECT_TRUE(Dialect::versions(release("", file)).empty()) << file;
    }
}

TEST(DialectVersions, DsdaDoomAndNyanDoomPickedByHandHaveTheirOwnRangesOnAnyName) {
    Engine engine = release("", "mystery-engine");

    engine.port = Port::Port::dsda;

    EXPECT_EQ(Dialect::versions(engine).size(), 9U);

    engine.file = "/games/nyan-doom";

    EXPECT_EQ(Dialect::versions(engine).size(), 9U);

    engine.port = Port::Port::nyan;

    EXPECT_EQ(Dialect::versions(engine).size(), 6U);

    engine.file = "/games/mystery-engine";
    engine.port = Port::Port::helion;

    EXPECT_EQ(Dialect::versions(engine).size(), 7U);
}

TEST(DialectVersions, ADsdaDoomRunUnderDosboxHasNoRanges) {
    Engine engine = release("0.25.0");

    engine.dosbox = true;

    EXPECT_TRUE(Dialect::versions(engine).empty());
}

TEST(DialectVersions, AnEntryWithRangesAndNoVersionNeedsOne) {
    EXPECT_TRUE(Dialect::needsVersion(release("")));
}

TEST(DialectVersions, AVersionThatNamesARangeNeedsNoMore) {
    for (const Dialect::VersionRange &each : Dialect::versions(release(""))) {
        EXPECT_FALSE(Dialect::needsVersion(release(std::string(each.first)))) << each.first;
    }
}

TEST(DialectVersions, AVersionWithoutAReleaseStillNeedsOne) {
    for (const char *version : {"garbage", "and later", "v.", "-"}) {
        EXPECT_TRUE(Dialect::needsVersion(release(version))) << version;
    }
}

TEST(DialectVersions, AnyReleaseNeedsNoMore) {
    for (const char *version : {"0.29.4", "0.30.0 and later", "9.9.9", "0.1", "dsda-doom-0.29.0"}) {
        EXPECT_FALSE(Dialect::needsVersion(release(version))) << version;
    }
}

TEST(DialectVersions, APortWithNoRangesNeedsNoVersion) {
    EXPECT_FALSE(Dialect::needsVersion(release("", "lzdoom")));
    EXPECT_FALSE(Dialect::needsVersion(release("", "rude")));
}

TEST(DialectVersions, ADownloadedPortIsNeverAsked) {
    Engine engine = release("");

    engine.managed = true;

    EXPECT_FALSE(Dialect::needsVersion(engine));
}

TEST(DialectVersions, AFileInThePortsFolderDoesNotMakeAPortManaged) {
    Engine engine = release("0.29.0");

    engine.file = (Catalog::directory() / "dsda-doom" / "dsda-doom").generic_string();

    EXPECT_FALSE(Dialect::of(engine).has(Feature::loadgame));

    engine.version.clear();

    EXPECT_TRUE(Dialect::needsVersion(engine));
}

TEST(DialectVersions, ADosboxEngineIsNeverAsked) {
    Engine engine = release("");

    engine.dosbox = true;

    EXPECT_FALSE(Dialect::needsVersion(engine));
}

TEST(DialectVersions, AnEntryWithNoVersionSpeaksTheLatestRelease) {
    const Dialect::Traits latest = Dialect::of(release(""));
    const Dialect::Traits named = Dialect::of(std::filesystem::path("/games/dsda-doom"));

    EXPECT_EQ(latest.loads, SaveNames::slot);
    EXPECT_TRUE(latest.has(Feature::recordsWithSkill));
    EXPECT_FALSE(latest.has(Feature::continuesWithSkill));
    EXPECT_TRUE(latest.has(Feature::recordsWithComplevel));
    EXPECT_TRUE(latest.has(Feature::chainEpisodes));
    EXPECT_TRUE(latest.has(Feature::gameSpeed));
    EXPECT_TRUE(latest.has(Feature::pistolStart));
    EXPECT_EQ(Dialect::arg(latest, Feature::pistolStart).name, "-pistolstart");
    EXPECT_EQ(latest.loads, named.loads);
    EXPECT_EQ(latest.line, named.line);
    EXPECT_EQ(latest.lastSlot, named.lastSlot);
}

TEST(DialectVersions, TheNewestRangeSpeaksTheLatestRelease) {
    const Dialect::Traits latest = Dialect::of(release(""));
    const Dialect::Traits newest = Dialect::of(release("0.30.0"));

    EXPECT_EQ(newest.loads, latest.loads);
    EXPECT_EQ(newest.has(Feature::recordsWithSkill), latest.has(Feature::recordsWithSkill));
    EXPECT_EQ(newest.has(Feature::continuesWithSkill), latest.has(Feature::continuesWithSkill));
    EXPECT_EQ(newest.has(Feature::recordsWithComplevel), latest.has(Feature::recordsWithComplevel));
    EXPECT_EQ(newest.features & CHAIN_AND_SPEED, latest.features & CHAIN_AND_SPEED);
    EXPECT_TRUE(Dialect::arg(newest, Feature::pistolStart) == Dialect::arg(latest, Feature::pistolStart));
}

TEST(DialectVersions, AVersionWithoutAReleaseSpeaksTheLatestRelease) {
    for (const char *version : {"garbage", "and later", "v."}) {
        const Dialect::Traits traits = Dialect::of(release(version));

        EXPECT_EQ(traits.loads, SaveNames::slot) << version;
        EXPECT_TRUE(traits.has(Feature::recordsWithSkill)) << version;
        EXPECT_EQ(traits.features & CHAIN_AND_SPEED, CHAIN_AND_SPEED) << version;
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
    for (const char *version : {"0.30.0", "0.28.0", "0.27.0", "0.26.0"}) {
        EXPECT_TRUE(Dialect::of(release(version)).has(Feature::chainEpisodes)) << version;
        EXPECT_TRUE(Dialect::of(release(version)).has(Feature::gameSpeed)) << version;
    }

    for (const char *version : {"0.25.0", "0.22.3", "0.21.2", "0.21.1"}) {
        const Dialect::Traits traits = Dialect::of(release(version));

        EXPECT_FALSE(traits.has(Feature::chainEpisodes)) << version;
        EXPECT_FALSE(traits.has(Feature::gameSpeed)) << version;
        EXPECT_TRUE(traits.has(Feature::turbo)) << version;
        EXPECT_TRUE(traits.has(Feature::coopSpawns)) << version;
        EXPECT_TRUE(traits.has(Feature::stroller)) << version;
        EXPECT_TRUE(traits.has(Feature::noCheats)) << version;
    }
}

TEST(DialectVersions, PistolStartIsAnOptionFrom0_21_2) {
    const Dialect::Traits first = Dialect::of(release("0.21.2"));

    EXPECT_TRUE(first.has(Feature::pistolStart));
    EXPECT_EQ(Dialect::arg(first, Feature::pistolStart).name, "-pistolstart");
    EXPECT_FALSE(Dialect::of(release("0.21.1")).has(Feature::pistolStart));
}

TEST(DialectVersions, AnOlderReleaseKeepsWhatNoBreakChanges) {
    const Dialect::Traits latest = Dialect::of(release(""));
    const Dialect::Traits oldest = Dialect::of(release("0.21.1"));

    EXPECT_EQ(oldest.dialect, latest.dialect);
    EXPECT_EQ(oldest.line, latest.line);
    EXPECT_EQ(spelled(oldest, Feature::saveFolder), spelled(latest, Feature::saveFolder));
    EXPECT_EQ(oldest.saveExt, latest.saveExt);
    EXPECT_EQ(oldest.lastSlot, latest.lastSlot);
    EXPECT_EQ(oldest.configName, latest.configName);
    EXPECT_EQ(oldest.complevel, latest.complevel);
    EXPECT_EQ(oldest.has(Feature::continues), latest.has(Feature::continues));
    EXPECT_EQ(oldest.has(Feature::continueComplevel), latest.has(Feature::continueComplevel));
    EXPECT_EQ(spelled(oldest, Feature::fullscreen), spelled(latest, Feature::fullscreen));
    EXPECT_EQ(spelled(oldest, Feature::windowed), spelled(latest, Feature::windowed));
}

TEST(DialectVersions, AVersionOnAPortWithoutRangesChangesNothing) {
    for (const char *file : {"lzdoom", "qzdoom", "prboom", "rude"}) {
        const Dialect::Traits without = Dialect::of(release("", file));
        const Dialect::Traits with = Dialect::of(release("0.25.0", file));

        ASSERT_TRUE(Dialect::versions(release("", file)).empty()) << file;
        EXPECT_EQ(with.features, without.features) << file;
        EXPECT_EQ(with.loads, without.loads) << file;
        EXPECT_TRUE(Dialect::arg(with, Feature::pistolStart) == Dialect::arg(without, Feature::pistolStart)) << file;
    }
}

TEST(DialectVersions, ADsdaDoomRunUnderDosboxSpeaksVanillaWhateverTheVersion) {
    Engine engine = release("0.28.0");

    engine.dosbox = true;

    const Dialect::Traits traits = Dialect::of(engine);

    EXPECT_TRUE(traits.has(Feature::dos));
    EXPECT_EQ(traits.dialect, Dialect::Dialect::doom);
    EXPECT_EQ(traits.loads, SaveNames::slot);
    EXPECT_FALSE(traits.has(Feature::continuesWithSkill));
}

TEST(DialectVersions, ChangingTheVersionOfTheSameEntryChangesWhatItSpeaks) {
    Engine engine = release("0.28.0");

    EXPECT_FALSE(Dialect::of(engine).has(Feature::loadgame));

    engine.version = "0.30.0";

    EXPECT_EQ(Dialect::of(engine).loads, SaveNames::slot);

    engine.version = "0.27.0";

    EXPECT_TRUE(Dialect::of(engine).has(Feature::continuesWithSkill));

    engine.version.clear();

    EXPECT_FALSE(Dialect::of(engine).has(Feature::continuesWithSkill));
    EXPECT_EQ(Dialect::of(engine).loads, SaveNames::slot);
}

TEST(DialectVersions, TheConfigSpeaksTheVersionOfThePortTheProfileRuns) {
    Config config = launch::single("/games/dsda-doom");

    config.engines.front().version = "0.27.0";

    EXPECT_TRUE(Dialect::of(config).has(Feature::continuesWithSkill));
    EXPECT_TRUE(Dialect::of(config, config.activeProfile()).has(Feature::continuesWithSkill));
}

namespace {

// The first release of the range a version of the port falls in. Empty for none.
std::string_view rowFor(const char *file, const std::string_view version) {
    const Dialect::VersionRange *found = Dialect::rangeOf(Dialect::versions(release("", file)), version);

    return found == nullptr ? std::string_view() : found->first;
}

constexpr std::array<const char *, 16> VERSIONED = {"gzdoom",
                                                    "uzdoom",
                                                    "vkdoom",
                                                    "skulltag",
                                                    "dsda-doom",
                                                    "nyan-doom",
                                                    "woof",
                                                    "nugget-doom",
                                                    "cherry-doom",
                                                    "helion",
                                                    "chocolate-doom",
                                                    "crispy-doom",
                                                    "prboom-plus",
                                                    "glboom-plus",
                                                    "zdoom",
                                                    "zandronum"};

constexpr std::array FOUR_PARTS = std::to_array<Dialect::VersionRange>({
        {.first = "1.1.0.0", .label = "", .lacks = {}, .gains = {}, .changes = {}, .special = nullptr},
        {.first = "0.9.5.2", .label = "", .lacks = {}, .gains = {}, .changes = {}, .special = nullptr},
});

}

TEST(DialectVersionMap, EveryStoredRangeKeyMapsToItsOwnRow) {
    for (const char *file : VERSIONED) {
        const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", file));

        ASSERT_FALSE(ranges.empty()) << file;

        for (const Dialect::VersionRange &each : ranges) {
            EXPECT_EQ(Dialect::rangeOf(ranges, each.first), &each) << file << " " << each.first;
        }
    }
}

TEST(DialectVersionMap, AReleaseBetweenRowsFallsInTheRowBelowIt) {
    EXPECT_EQ(rowFor("dsda-doom", "0.29.4"), "0.28.3");
    EXPECT_EQ(rowFor("dsda-doom", "0.29.0"), "0.28.3");
    EXPECT_EQ(rowFor("dsda-doom", "0.28.2"), "0.28.0");
    EXPECT_EQ(rowFor("dsda-doom", "0.27.5"), "0.27.0");
    EXPECT_EQ(rowFor("dsda-doom", "0.24.3"), "0.22.3");
    EXPECT_EQ(rowFor("gzdoom", "4.12.0"), "4.11.0");
    EXPECT_EQ(rowFor("gzdoom", "4.10.0"), "4.9.0");
    EXPECT_EQ(rowFor("gzdoom", "4.8.2"), "4.8.0");
    EXPECT_EQ(rowFor("gzdoom", "4.7.1"), "4.4.0");
    EXPECT_EQ(rowFor("woof", "14.5.0"), "10.2.0");
    EXPECT_EQ(rowFor("woof", "8.0.0"), "6.1.0");
    EXPECT_EQ(rowFor("nugget-doom", "5.1.0"), "4.0.0");
    EXPECT_EQ(rowFor("nugget-doom", "1.9.1"), "1.9.0");
    EXPECT_EQ(rowFor("cherry-doom", "2.0.0"), "1.0.0");
    EXPECT_EQ(rowFor("vkdoom", "1.0"), "0.9");
}

TEST(DialectVersionMap, AReleaseNewerThanEveryRowFallsInTheNewest) {
    EXPECT_EQ(rowFor("dsda-doom", "0.30.1"), "0.30.0");
    EXPECT_EQ(rowFor("dsda-doom", "1.0"), "0.30.0");
    EXPECT_EQ(rowFor("gzdoom", "4.14.2"), "4.12.1");
    EXPECT_EQ(rowFor("uzdoom", "5.1.0"), "5.0.0");
    EXPECT_EQ(rowFor("vkdoom", "2025.6.0"), "25.6.0");
    EXPECT_EQ(rowFor("woof", "16.0.0"), "16.0.0");
    EXPECT_EQ(rowFor("woof", "16.2.1"), "16.0.0");
    EXPECT_EQ(rowFor("cherry-doom", "2.2.0"), "2.1.0");
    EXPECT_EQ(rowFor("skulltag", "0.98e"), "0.98c");
}

TEST(DialectVersionMap, AReleaseOlderThanEveryRowFallsInTheOldest) {
    EXPECT_EQ(rowFor("dsda-doom", "0.21.0"), "0.21.1");
    EXPECT_EQ(rowFor("dsda-doom", "0.2"), "0.21.1");
    EXPECT_EQ(rowFor("gzdoom", "4.3.2"), "4.3.3");
    EXPECT_EQ(rowFor("gzdoom", "3.0.0"), "4.3.3");
    EXPECT_EQ(rowFor("uzdoom", "4.14.2"), "4.14.3");
    EXPECT_EQ(rowFor("vkdoom", "0.8"), "0.9");
    EXPECT_EQ(rowFor("woof", "5.0.0"), "6.1.0");
    EXPECT_EQ(rowFor("nugget-doom", "0.9.0"), "1.0.0");
    EXPECT_EQ(rowFor("skulltag", "0.97c3"), "0.97d");
}

TEST(DialectVersionMap, ATableKeyedByItsLastReleaseHoldsThatReleaseAndTheOnesBefore) {
    EXPECT_EQ(rowFor("gzdoom", "4.3.3"), "4.3.3");
    EXPECT_EQ(rowFor("gzdoom", "4.3.0"), "4.3.3");
    EXPECT_EQ(rowFor("gzdoom", "4.4.0"), "4.4.0");
    EXPECT_EQ(rowFor("dsda-doom", "0.21.1"), "0.21.1");
    EXPECT_EQ(rowFor("dsda-doom", "0.20.0"), "0.21.1");
    EXPECT_EQ(rowFor("dsda-doom", "0.21.2"), "0.21.2");
    EXPECT_EQ(rowFor("uzdoom", "4.14.3"), "4.14.3");
    EXPECT_EQ(rowFor("uzdoom", "4.99.0"), "4.14.3");
}

TEST(DialectVersionMap, ALetterCountsAsOneMorePart) {
    EXPECT_EQ(rowFor("skulltag", "0.98c"), "0.98c");
    EXPECT_EQ(rowFor("skulltag", "0.98d"), "0.98c");
    EXPECT_EQ(rowFor("skulltag", "0.98b"), "0.98a");
    EXPECT_EQ(rowFor("skulltag", "0.98a"), "0.98a");
    EXPECT_EQ(rowFor("skulltag", "0.97d5"), "0.97d");
    EXPECT_EQ(rowFor("skulltag", "0.98"), "0.97d");
    EXPECT_EQ(Dialect::leadingRelease("Skulltag 0.98d-beta"), "0.98d");
}

TEST(DialectVersionMap, AFourPartReleaseComparesPartByPart) {
    EXPECT_EQ(Dialect::leadingRelease("1.1.0.0"), "1.1.0.0");
    EXPECT_EQ(Dialect::rangeOf(FOUR_PARTS, "1.1.0.0"), &FOUR_PARTS[0]);
    EXPECT_EQ(Dialect::rangeOf(FOUR_PARTS, "1.1.0.1"), &FOUR_PARTS[0]);
    EXPECT_EQ(Dialect::rangeOf(FOUR_PARTS, "1.1"), &FOUR_PARTS[0]);
    EXPECT_EQ(Dialect::rangeOf(FOUR_PARTS, "1.0.9.9"), &FOUR_PARTS[1]);
    EXPECT_EQ(Dialect::rangeOf(FOUR_PARTS, "0.9.5.10"), &FOUR_PARTS[1]);
    EXPECT_EQ(Dialect::rangeOf(FOUR_PARTS, "0.9.5.1"), &FOUR_PARTS[1]);
}

TEST(DialectVersionMap, OnlyTheLeadingReleaseOfTheTextIsRead) {
    EXPECT_EQ(Dialect::leadingRelease("g4.14.2-0-g1234abc"), "4.14.2");
    EXPECT_EQ(Dialect::leadingRelease("0.30.1-12-gabc123"), "0.30.1");
    EXPECT_EQ(Dialect::leadingRelease("v0.29.0"), "0.29.0");
    EXPECT_EQ(Dialect::leadingRelease("dsda-doom-0.29.0"), "0.29.0");
    EXPECT_EQ(Dialect::leadingRelease("GZDoom 4.14.2 (64-bit)"), "4.14.2");
    EXPECT_EQ(Dialect::leadingRelease("4.13.1"), "4.13.1");
    EXPECT_EQ(Dialect::leadingRelease("2.02"), "2.02");
    EXPECT_EQ(Dialect::leadingRelease("16.0."), "16.0");
    EXPECT_EQ(Dialect::leadingRelease("garbage"), "");
    EXPECT_EQ(Dialect::leadingRelease(""), "");
}

TEST(DialectVersionMap, AVersionWithoutAReleaseOrWithoutRangesFallsInNoRow) {
    EXPECT_EQ(rowFor("dsda-doom", ""), "");
    EXPECT_EQ(rowFor("dsda-doom", "garbage"), "");
    EXPECT_EQ(Dialect::rangeOf({}, "4.14.2"), nullptr);
    EXPECT_EQ(rowFor("lzdoom", "4.14.2"), "");
}

TEST(DialectVersionMap, AFileVersionStringFallsInItsRange) {
    EXPECT_EQ(rowFor("gzdoom", "g4.14.2-0-g1234abc"), "4.12.1");
    EXPECT_EQ(rowFor("gzdoom", "4.13.1"), "4.12.1");
    EXPECT_EQ(rowFor("gzdoom", "g4.10.0-24-gfe3c1d0"), "4.9.0");
    EXPECT_EQ(rowFor("gzdoom", "g4.8.2"), "4.8.0");
}

namespace {

int reads = 0;

std::string fileVersion(const std::filesystem::path & /*file*/) {
    ++reads;

    return "g4.10.0-24-gfe3c1d0";
}

std::string noVersion(const std::filesystem::path & /*file*/) {
    ++reads;

    return "Doom engine";
}

}

TEST(DialectSuggestion, AnEntryThatNeedsAVersionTakesTheReleaseTheFileNames) {
    reads = 0;

    EXPECT_EQ(Dialect::suggestedVersion(release("", "gzdoom"), fileVersion), "4.10.0");
    EXPECT_EQ(reads, 1);
}

TEST(DialectSuggestion, TheReleaseIsStoredAndNotItsRange) {
    const std::string suggested = Dialect::suggestedVersion(release("", "gzdoom"), fileVersion);

    EXPECT_EQ(suggested, "4.10.0");
    EXPECT_EQ(Dialect::rangeOf(Dialect::versions(release("", "gzdoom")), suggested)->first, "4.9.0");
}

TEST(DialectSuggestion, TextWithoutAReleaseSuggestsNothing) {
    EXPECT_EQ(Dialect::suggestedVersion(release("", "gzdoom"), noVersion), "");
}

namespace {

std::string fixedNumbers(const std::filesystem::path & /*file*/) {
    return "4, 14, 2, 0";
}

}

TEST(DialectSuggestion, ALoneNumberIsNoRelease) {
    EXPECT_EQ(Dialect::suggestedVersion(release("", "gzdoom"), fixedNumbers), "");
    EXPECT_EQ(Dialect::rangeOf(Dialect::versions(release("", "gzdoom")), "4")->first, "4.3.3");
}

TEST(DialectSuggestion, AnEntryThatNeedsNoVersionIsNotRead) {
    Engine dos = release("", "gzdoom");
    Engine managed = release("", "gzdoom");

    dos.dosbox = true;
    managed.managed = true;
    reads = 0;

    EXPECT_EQ(Dialect::suggestedVersion(dos, fileVersion), "");
    EXPECT_EQ(Dialect::suggestedVersion(managed, fileVersion), "");
    EXPECT_EQ(Dialect::suggestedVersion(release("4.8.0", "gzdoom"), fileVersion), "");
    EXPECT_EQ(Dialect::suggestedVersion(release("", "lzdoom"), fileVersion), "");
    EXPECT_EQ(reads, 0);
}

TEST(DialectVersions, AReleaseBetweenRowsSpeaksItsRange) {
    EXPECT_FALSE(Dialect::of(release("0.29.4")).has(Feature::loadgame));
    EXPECT_TRUE(Dialect::of(release("0.27.3")).has(Feature::continuesWithSkill));
    EXPECT_FALSE(Dialect::of(release("0.2")).has(Feature::pistolStart));
    EXPECT_EQ(Dialect::of(release("0.31.0")).loads, SaveNames::slot);
}

TEST(DialectVersions, AManagedPortSpeaksTheLatestReleaseWhateverItStores) {
    Engine engine = release("0.29.0");

    engine.managed = true;

    EXPECT_FALSE(Dialect::needsVersion(engine));
    EXPECT_TRUE(Dialect::of(engine).has(Feature::loadgame));
    EXPECT_EQ(Dialect::of(engine).loads, SaveNames::slot);

    engine.version = "0.27.2";

    EXPECT_FALSE(Dialect::of(engine).has(Feature::continuesWithSkill));

    engine.managed = false;

    EXPECT_TRUE(Dialect::of(engine).has(Feature::continuesWithSkill));
}

TEST(DialectVersions, AManagedPortWithoutAReleaseSpeaksTheLatest) {
    Engine engine = release("");

    engine.managed = true;

    EXPECT_FALSE(Dialect::needsVersion(engine));
    EXPECT_EQ(Dialect::of(engine).loads, SaveNames::slot);
}

namespace {

static_assert(static_cast<unsigned>(Feature::count) <= 128);
static_assert((Feature::replays | Feature::longtics).has(Feature::longtics));
static_assert(!(Feature::replays | Feature::longtics).has(Feature::soloNet));
static_assert(((Feature::replays | Feature::longtics) & ~Dialect::Features(Feature::replays)).has(Feature::longtics));
static_assert(!((Feature::replays | Feature::longtics) & ~Dialect::Features(Feature::replays)).has(Feature::replays));

constexpr Dialect::Features DSDA_VERSIONED = Feature::loadgame | Feature::recordsWithSkill | Feature::continuesWithSkill
                                             | Feature::recordsWithComplevel | Feature::pistolStart | CHAIN_AND_SPEED
                                             | Feature::organizedSavesWin | Feature::homeDotFolder
                                             | Feature::dataRootTakesSlot;

struct Expected {
    std::string_view first;
    bool loadgame;
    bool recordsWithSkill;
    bool continuesWithSkill;
    bool recordsWithComplevel;
    bool pistolStart;
    bool lacksChainAndSpeed;
    bool organizedSavesWin;
    bool homeDotFolder;
    bool dataRootTakesSlot;
};

constexpr std::array DSDA_EXPECTED = std::to_array<Expected>({
        {"0.30.0", true, true, false, true, true, false, false, false, false},
        {"0.28.3", false, true, false, true, true, false, false, false, false},
        {"0.28.0", false, true, false, true, true, false, true, false, false},
        {"0.27.0", false, true, true, true, true, false, true, true, false},
        {"0.26.0", false, false, false, true, true, false, true, true, false},
        {"0.25.0", false, false, false, true, true, true, true, true, true},
        {"0.22.3", false, false, false, false, true, true, true, true, true},
        {"0.21.2", true, false, false, false, true, true, true, true, true},
        {"0.21.1", true, false, false, false, false, true, true, true, true},
});

void markSpecial(Dialect::Traits &traits) {
    traits.lastSlot = 7;
}

}

TEST(DialectFeatures, MasksCombineAndTakeAwayInAConstantExpression) {
    constexpr Dialect::Features both = Feature::levelstat | Feature::cheats;

    static_assert(both.has(Feature::levelstat));
    static_assert(both.has(Feature::cheats));
    static_assert(!both.has(Feature::shotdir));
    static_assert(Dialect::Features() == (both & ~both));

    EXPECT_EQ((both & ~Dialect::Features(Feature::cheats)), Dialect::Features(Feature::levelstat));
}

TEST(DialectFeatures, ARangeTakesAwayWhatItLacksAndAddsWhatItGains) {
    const Dialect::Traits base{.features = Feature::loadgame | Feature::levelstat | Feature::cheats | CHAIN_AND_SPEED};
    const Dialect::VersionRange range{
            .first = "1",
            .label = "one",
            .lacks = Feature::loadgame | Feature::shotdir | CHAIN_AND_SPEED,
            .gains = Feature::continuesWithSkill,
            .changes = {},
            .special = nullptr,
    };

    const Dialect::Traits at = Dialect::atRelease(base, range);

    EXPECT_FALSE(at.has(Feature::loadgame));
    EXPECT_TRUE(at.has(Feature::levelstat));
    EXPECT_TRUE(at.has(Feature::cheats));
    EXPECT_TRUE(at.has(Feature::continuesWithSkill));
    EXPECT_FALSE(at.has(Feature::shotdir));
    EXPECT_EQ(at.features & CHAIN_AND_SPEED, Dialect::Features());
    EXPECT_EQ(at.lastSlot, base.lastSlot);
}

TEST(DialectFeatures, ARangeTakesAwayOnlyWhatItLacks) {
    const Dialect::Traits base{.features = Feature::turbo | Feature::gameSpeed};
    const Dialect::VersionRange range{
            .first = "1",
            .label = "one",
            .lacks = Feature::gameSpeed,
            .gains = {},
            .changes = {},
            .special = nullptr,
    };

    EXPECT_EQ(Dialect::atRelease(base, range).features & (CHAIN_AND_SPEED | Feature::turbo),
              Dialect::Features(Feature::turbo));
}

TEST(DialectFeatures, ARangeRunsItsSpecialCaseAfterTheMasks) {
    const Dialect::VersionRange range{
            .first = "1",
            .label = "one",
            .lacks = Feature::cheats,
            .gains = {},
            .changes = {},
            .special = markSpecial,
    };
    const Dialect::Traits at = Dialect::atRelease(Dialect::Traits{.features = Feature::cheats}, range);

    EXPECT_EQ(at.lastSlot, 7);
    EXPECT_FALSE(at.has(Feature::cheats));
}

TEST(DialectFeatures, EveryDialectAgreesWithTheValuesItDerivesFrom) {
    std::vector<Dialect::Traits> all;

    for (const char *file : {
                 "gzdoom",
                 "uzdoom",
                 "zdoom",
                 "zandronum",
                 "dsda-doom",
                 "nyan-doom",
                 "prboom-plus",
                 "woof",
                 "nugget-doom",
                 "cherry-doom",
                 "chocolate-doom",
                 "crispy-doom",
                 "crispy-heretic",
                 "crispy-hexen",
                 "crispy-strife",
                 "chocolate-heretic",
                 "chocolate-hexen",
                 "chocolate-strife",
                 "eternity",
                 "helion",
                 "boom",
                 "mbf",
                 "legacy",
                 "doom2",
                 "mystery-engine",
         }) {
        all.push_back(named(file));
    }

    for (const bool dos : {false, true}) {
        for (const Port::Port each : Dialect::picks(dos)) {
            all.push_back(Dialect::of("/games/mystery-engine", each));
        }
    }

    for (const char *file : {"mbf.exe", "doom.exe", "legacy.exe", "mystery.exe", "gzdoom.exe"}) {
        all.push_back(Dialect::of(launch::single(std::string("/games/") + file, true)));
    }

    for (const char *file : {"gzdoom",
                             "uzdoom",
                             "dsda-doom",
                             "nyan-doom",
                             "woof",
                             "nugget-doom",
                             "cherry-doom",
                             "helion",
                             "chocolate-doom",
                             "crispy-doom",
                             "chocolate-heretic",
                             "chocolate-hexen",
                             "chocolate-strife",
                             "crispy-heretic",
                             "crispy-hexen",
                             "crispy-strife"}) {
        for (const Dialect::VersionRange &each : Dialect::versions(release("", file))) {
            all.push_back(Dialect::of(release(std::string(each.first), file)));
        }
    }

    for (const Dialect::Traits &each : all) {
        EXPECT_EQ(Dialect::defined(each).features, each.features) << each.configName;
        EXPECT_EQ(each.has(Feature::record), each.has(Feature::replays)) << each.configName;
        EXPECT_EQ(each.has(Feature::hosts), each.netgame == Netgames::zdoom || each.netgame == Netgames::chocolate)
                << each.configName;
        EXPECT_EQ(each.has(Feature::joins), each.netgame != Netgames::none) << each.configName;
        EXPECT_EQ(each.has(Feature::hosts), each.has(Feature::deathmatch)) << each.configName;
        EXPECT_EQ(each.has(Feature::hostListed), each.netgame == Netgames::chocolate) << each.configName;
        EXPECT_EQ(each.has(Feature::hostPlayers), each.netgame == Netgames::zdoom) << each.configName;
        EXPECT_TRUE(!each.has(Feature::deathmatch3) || each.netgame == Netgames::chocolate) << each.configName;

        if (each.netgame != Netgames::zdoom) {
            EXPECT_FALSE(each.has(Feature::netmode) || each.has(Feature::teamPick) || each.has(Feature::netgameFolder))
                    << each.configName;
        }

        if (!each.has(Feature::replays)) {
            EXPECT_FALSE(each.has(Feature::timedemo) || each.has(Feature::fastdemo) || each.has(Feature::continues)
                         || each.has(Feature::longtics) || each.has(Feature::soloNet))
                    << each.configName;
        }
    }
}

TEST(DialectFeatures, TheNamesAreTriedInTheOrderTheDialectsNeedIt) {
    EXPECT_EQ(named("gzdoom").dialect, Dialect::Dialect::gzdoom);
    EXPECT_EQ(named("uzdoom").dialect, Dialect::Dialect::uzdoom);
    EXPECT_EQ(named("zdoom").dialect, Dialect::Dialect::zdoom);
    EXPECT_EQ(named("zandronum").dialect, Dialect::Dialect::zandronum);
    EXPECT_EQ(named("boom").dialect, Dialect::Dialect::boom);
    EXPECT_EQ(named("prboom-plus").dialect, Dialect::Dialect::prboom);
    EXPECT_EQ(named("crispy-doom").dialect, Dialect::Dialect::chocolate);
    EXPECT_EQ(named("crispy-doom").line, PortLine::crispy);
    EXPECT_EQ(named("nugget-doom").dialect, Dialect::Dialect::woof);
    EXPECT_EQ(named("nyan-doom").dialect, Dialect::Dialect::dsda);
    EXPECT_EQ(named("doom").dialect, Dialect::Dialect::doom);
}

TEST(DialectVersions, EveryRangeOfDsdaDoomSpeaksWhatItDidBeforeTheTable) {
    const Dialect::Traits latest = Dialect::of(release(""));
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release(""));

    ASSERT_EQ(ranges.size(), DSDA_EXPECTED.size());

    for (size_t at = 0; at < ranges.size(); ++at) {
        const Expected &want = DSDA_EXPECTED[at];
        const Dialect::Traits traits = Dialect::of(release(std::string(want.first)));

        EXPECT_EQ(ranges[at].first, want.first);
        EXPECT_EQ(traits.has(Feature::loadgame), want.loadgame) << want.first;
        EXPECT_EQ(traits.has(Feature::recordsWithSkill), want.recordsWithSkill) << want.first;
        EXPECT_EQ(traits.has(Feature::continuesWithSkill), want.continuesWithSkill) << want.first;
        EXPECT_EQ(traits.has(Feature::recordsWithComplevel), want.recordsWithComplevel) << want.first;
        EXPECT_EQ(traits.has(Feature::pistolStart), want.pistolStart) << want.first;
        EXPECT_EQ(traits.has(Feature::chainEpisodes), !want.lacksChainAndSpeed) << want.first;
        EXPECT_EQ(traits.has(Feature::gameSpeed), !want.lacksChainAndSpeed) << want.first;
        EXPECT_EQ(traits.has(Feature::organizedSavesWin), want.organizedSavesWin) << want.first;
        EXPECT_EQ(traits.has(Feature::homeDotFolder), want.homeDotFolder) << want.first;
        EXPECT_EQ(traits.has(Feature::dataRootTakesSlot), want.dataRootTakesSlot) << want.first;

        EXPECT_EQ(traits.features & ~DSDA_VERSIONED, latest.features & ~DSDA_VERSIONED) << want.first;
        EXPECT_EQ(traits.loads, latest.loads) << want.first;
        EXPECT_TRUE(Dialect::arg(traits, Feature::pistolStart) == Dialect::arg(latest, Feature::pistolStart))
                << want.first;
        EXPECT_EQ(traits.lastSlot, latest.lastSlot) << want.first;
        EXPECT_EQ(traits.line, latest.line) << want.first;
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

TEST(DialectVersions, EachNameGivesItsPort) {
    using Port::Port;

    EXPECT_EQ(named("gzdoom").port, Port::gzdoom);
    EXPECT_EQ(named("lzdoom").port, Port::zdoomFork);
    EXPECT_EQ(named("qzdoom").port, Port::zdoomFork);
    EXPECT_EQ(named("vkdoom").port, Port::vkdoom);
    EXPECT_EQ(named("uzdoom").port, Port::uzdoom);
    EXPECT_EQ(named("zdoom-fork").port, Port::zdoomFork);
    EXPECT_EQ(named("nyan-doom").port, Port::nyan);
    EXPECT_EQ(named("nugget-doom").port, Port::nugget);
    EXPECT_EQ(named("cherry-doom").port, Port::cherry);
    EXPECT_EQ(named("prboom-plus").port, Port::prboomPlus);
    EXPECT_EQ(named("glboom-plus").port, Port::glboomPlus);
    EXPECT_EQ(named("prboom").port, Port::prboom);
    EXPECT_EQ(named("crispy-heretic").port, Port::crispyHeretic);
    EXPECT_EQ(named("crispy-hexen").port, Port::crispyHexen);
    EXPECT_EQ(named("crispy-strife").port, Port::crispyStrife);
    EXPECT_EQ(named("chocolate-heretic").port, Port::chocolateHeretic);
    EXPECT_EQ(named("chocolate-hexen").port, Port::chocolateHexen);
    EXPECT_EQ(named("chocolate-strife").port, Port::chocolateStrife);
    EXPECT_EQ(named("skulltag").port, Port::skulltag);
    EXPECT_EQ(named("mystery-engine").port, Port::unknown);
    EXPECT_EQ(named("mystery-engine").line, PortLine::gzdoom);
}

TEST(DialectPorts, EveryWordOfAPortGivesItUnderItsRule) {
    using Port::Match;

    for (const Port::Known &known : Port::known()) {
        for (const std::string_view word : known.words) {
            if (word.empty()) {
                continue;
            }

            const std::string longer = std::string(word) + "-fork";

            if (known.match == Match::contains) {
                EXPECT_EQ(named(longer.c_str()).port, known.port) << word;

                continue;
            }

            EXPECT_EQ(named(std::string(word).c_str()).port, known.port) << word;
            EXPECT_NE(named(longer.c_str()).port, known.port) << word;
        }
    }
}

TEST(DialectPorts, TheOtherGameWordsNeedTheHyphen) {
    using Port::Port;

    EXPECT_EQ(named("crispy-heretic").port, Port::crispyHeretic);
    EXPECT_EQ(named("chocolate-hexen-x64").port, Port::chocolateHexen);
    EXPECT_EQ(named("crispyheretic").port, Port::crispy);
    EXPECT_EQ(named("chocolate_strife").port, Port::chocolate);
}

TEST(DialectPicked, APickedPortIsThePortSpoken) {
    using Port::Port;

    EXPECT_EQ(Dialect::of("/games/gzdoom", Port::dsda).port, Port::dsda);
    EXPECT_EQ(Dialect::of("/games/mystery-engine", Port::prboomPlus).port, Port::prboomPlus);
    EXPECT_EQ(Dialect::of("/games/mystery-engine", Port::nugget).port, Port::nugget);
    EXPECT_EQ(Dialect::of("/games/mystery-engine", Port::skulltag).port, Port::skulltag);
    EXPECT_EQ(Dialect::of("/games/mystery-engine", Port::doom).port, Port::doom);
}

TEST(DialectVersions, GzdoomAndUzdoomListTheirRangesNewestFirst) {
    const std::span<const Dialect::VersionRange> gzdoom = Dialect::versions(release("", "gzdoom"));
    const std::span<const Dialect::VersionRange> uzdoom = Dialect::versions(release("", "uzdoom"));

    EXPECT_EQ(firsts(gzdoom), (std::vector<std::string_view>{"4.12.1", "4.11.0", "4.9.0", "4.8.0", "4.4.0", "4.3.3"}));
    EXPECT_EQ(gzdoom.front().label, "4.12.1 and later");
    EXPECT_EQ(gzdoom[3].label, "4.8.0 to 4.8.2");
    EXPECT_EQ(gzdoom.back().label, "4.3.3 and earlier");
    EXPECT_EQ(firsts(uzdoom), (std::vector<std::string_view>{"5.0.0", "4.14.3"}));
    EXPECT_EQ(uzdoom.back().label, "4.14.3");
    EXPECT_TRUE(Dialect::needsVersion(release("", "gzdoom")));
    EXPECT_TRUE(Dialect::needsVersion(release("", "uzdoom")));
}

TEST(DialectVersions, AZdoomPortPickedByHandHasItsRanges) {
    Engine engine = release("", "mystery-engine");

    EXPECT_TRUE(Dialect::versions(engine).empty());

    engine.port = Port::Port::gzdoom;

    EXPECT_EQ(Dialect::versions(engine).size(), 6U);

    engine.port = Port::Port::uzdoom;

    EXPECT_EQ(Dialect::versions(engine).size(), 2U);

    for (const char *fork : {"/games/lzdoom", "/games/zdoom-fork"}) {
        engine.file = fork;
        engine.port = Port::Port::gzdoom;

        EXPECT_EQ(Dialect::versions(engine).size(), 6U) << fork;

        engine.port = Port::Port::unknown;

        EXPECT_TRUE(Dialect::versions(engine).empty()) << fork;
    }

    engine.file = "/games/vkdoom";
    engine.port = Port::Port::gzdoom;

    EXPECT_EQ(Dialect::versions(engine).size(), 6U);

    engine.port = Port::Port::unknown;

    EXPECT_EQ(Dialect::versions(engine).size(), 2U);

    engine.file = "/games/lzdoom";
    engine.port = Port::Port::vkdoom;

    EXPECT_EQ(Dialect::versions(engine).size(), 2U);

    engine.port = Port::Port::skulltag;

    EXPECT_EQ(Dialect::versions(engine).size(), 3U);
}

namespace {

constexpr Dialect::Features GZDOOM_VERSIONED = Feature::pistolStart | Feature::dmflags3 | Feature::savegamesSubfolder
                                               | Feature::folderPerGame | Feature::portableNeedsIni
                                               | Feature::windowsSaveSubfolder | Feature::corpseGibs;

struct GzdoomExpected {
    std::string_view first;

    // Which of GZDOOM_VERSIONED the range has.
    Dialect::Features has;

    SaveNames loads;
    std::string_view fullscreen;
};

constexpr Dialect::Features GZDOOM_4_9 =
        Feature::folderPerGame | Feature::portableNeedsIni | Feature::windowsSaveSubfolder;

constexpr std::array GZDOOM_EXPECTED = std::to_array<GzdoomExpected>({
        {"4.12.1", GZDOOM_VERSIONED & ~Dialect::Features(Feature::corpseGibs), SaveNames::name, "+vid_fullscreen"},
        {"4.11.0", Feature::dmflags3 | Feature::savegamesSubfolder | GZDOOM_4_9, SaveNames::name, "+vid_fullscreen"},
        {"4.9.0", GZDOOM_4_9, SaveNames::name, "+vid_fullscreen"},
        {"4.8.0", Feature::windowsSaveSubfolder, SaveNames::path, "+vid_fullscreen"},
        {"4.4.0", Feature::windowsSaveSubfolder | Feature::corpseGibs, SaveNames::path, "+vid_fullscreen"},
        {"4.3.3", Feature::corpseGibs, SaveNames::path, "+fullscreen"},
});

}

TEST(DialectVersions, EveryRangeOfGzdoomSpeaksItsRelease) {
    const Dialect::Traits latest = Dialect::of(release("", "gzdoom"));
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", "gzdoom"));

    ASSERT_EQ(ranges.size(), GZDOOM_EXPECTED.size());

    for (size_t at = 0; at < ranges.size(); ++at) {
        const GzdoomExpected &want = GZDOOM_EXPECTED[at];
        const Dialect::Traits traits = Dialect::of(release(std::string(want.first), "gzdoom"));

        EXPECT_EQ(ranges[at].first, want.first);
        EXPECT_EQ(traits.features & GZDOOM_VERSIONED, want.has) << want.first;
        EXPECT_EQ(traits.loads, want.loads) << want.first;
        EXPECT_EQ(spelled(traits, Feature::fullscreen), want.fullscreen) << want.first;
        EXPECT_EQ(spelled(traits, Feature::windowed), want.fullscreen) << want.first;
        EXPECT_EQ(Dialect::arg(traits, Feature::fullscreen).word, "1") << want.first;
        EXPECT_EQ(Dialect::arg(traits, Feature::windowed).word, "0") << want.first;

        EXPECT_EQ(traits.features & ~GZDOOM_VERSIONED, latest.features & ~GZDOOM_VERSIONED) << want.first;
        EXPECT_TRUE(traits.has(Feature::stdoutSwitch)) << want.first;
        EXPECT_TRUE(traits.has(Feature::portableIni)) << want.first;
        EXPECT_EQ(spelled(traits, Feature::bex), "-deh") << want.first;
        EXPECT_EQ(traits.line, latest.line) << want.first;
        EXPECT_EQ(traits.port, latest.port) << want.first;
        EXPECT_EQ(traits.saveExt, latest.saveExt) << want.first;
    }
}

TEST(DialectVersions, TheNewestGzdoomRangeSpeaksTheLatestRelease) {
    const Dialect::Traits latest = Dialect::of(release("", "gzdoom"));
    const Dialect::Traits newest = Dialect::of(release("4.12.1", "gzdoom"));

    EXPECT_EQ(newest.features, latest.features);
    EXPECT_EQ(newest.loads, latest.loads);
    EXPECT_EQ(newest.changes.data(), latest.changes.data());
    EXPECT_EQ(spelled(newest, Feature::pistolStart), "+sv_pistolstart");
}

TEST(DialectVersions, AGzdoomReleaseNewerThanEveryRowSpeaksTheLatestRelease) {
    for (const char *version : {"4.14.2", "4.12.1", "garbage"}) {
        const Dialect::Traits traits = Dialect::of(release(version, "gzdoom"));

        EXPECT_EQ(traits.features, Dialect::of(release("", "gzdoom")).features) << version;
        EXPECT_EQ(traits.loads, SaveNames::name) << version;
    }
}

TEST(DialectVersions, AGzdoomReleaseBetweenRowsSpeaksTheRowBelowIt) {
    EXPECT_EQ(Dialect::of(release("4.10.0", "gzdoom")).features, Dialect::of(release("4.9.0", "gzdoom")).features);
    EXPECT_EQ(Dialect::of(release("4.7.1", "gzdoom")).loads, SaveNames::path);
    EXPECT_EQ(Dialect::of(release("3.8.2", "gzdoom")).features, Dialect::of(release("4.3.3", "gzdoom")).features);
}

TEST(DialectVersions, AGzdoomVersionLeavesLzdoomAlone) {
    const Dialect::Traits without = Dialect::of(release("", "lzdoom"));
    const Dialect::Traits with = Dialect::of(release("4.3.3", "lzdoom"));

    EXPECT_EQ(with.features, without.features);
    EXPECT_EQ(with.loads, SaveNames::name);
    EXPECT_EQ(spelled(with, Feature::fullscreen), "+vid_fullscreen");
}

TEST(DialectVersions, VkdoomReadsAGzdoomNumberAsAVkdoomRelease) {
    EXPECT_EQ(Dialect::rangeOf(Dialect::versions(release("", "vkdoom")), "4.3.3")->first, "0.9");
}

TEST(DialectVersions, VkdoomListsItsTwoReleasesNewestFirst) {
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", "vkdoom"));

    EXPECT_EQ(firsts(ranges), (std::vector<std::string_view>{"25.6.0", "0.9"}));
    ASSERT_EQ(ranges.size(), 2U);
    EXPECT_EQ(ranges[0].label, "25.6.0 and later");
    EXPECT_EQ(ranges[1].label, "0.9");
    EXPECT_TRUE(Dialect::needsVersion(release("", "vkdoom")));
    EXPECT_FALSE(Dialect::needsVersion(release("0.9", "vkdoom")));
}

TEST(DialectVersions, Vkdoom0_9HasNoPistolStartAndSavesBesideItsIni) {
    const Dialect::Traits latest = Dialect::of(release("", "vkdoom"));
    const Dialect::Traits newest = Dialect::of(release("25.6.0", "vkdoom"));
    const Dialect::Traits first = Dialect::of(release("0.9", "vkdoom"));

    EXPECT_EQ(newest.features, latest.features);
    EXPECT_TRUE(latest.has(Feature::pistolStart));
    EXPECT_TRUE(latest.has(Feature::savegamesSubfolder));

    EXPECT_FALSE(first.has(Feature::pistolStart));
    EXPECT_FALSE(first.has(Feature::savegamesSubfolder));
    EXPECT_TRUE(first.has(Feature::dmflags3));
    EXPECT_TRUE(first.has(Feature::folderPerGame));
    EXPECT_TRUE(first.has(Feature::netmode));
    EXPECT_EQ(first.loads, SaveNames::name);
    EXPECT_EQ(first.features, latest.features & ~(Feature::pistolStart | Feature::savegamesSubfolder));
}

TEST(DialectVersions, SkulltagListsItsReleasesFrom0_97d) {
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", "skulltag"));

    EXPECT_EQ(firsts(ranges), (std::vector<std::string_view>{"0.98c", "0.98a", "0.97d"}));
    ASSERT_EQ(ranges.size(), 3U);
    EXPECT_EQ(ranges[0].label, "0.98c to 0.98d");
    EXPECT_EQ(ranges[1].label, "0.98a to 0.98b");
    EXPECT_EQ(ranges[2].label, "0.97d to 0.97d5");
}

TEST(DialectVersions, ALetterAfterTheNumberCountsAsOneMorePart) {
    static_assert(Dialect::later("0.98d", "0.98c"));
    static_assert(Dialect::later("0.98c", "0.98b"));
    static_assert(!Dialect::later("0.98a", "0.98b"));
    static_assert(Dialect::later("0.98a", "0.97d2"));
    static_assert(Dialect::later("0.97d2", "0.97d"));
    static_assert(Dialect::later("0.97d5", "0.97d2"));
    static_assert(!Dialect::later("0.97d", "0.97d"));
}

TEST(DialectVersions, ALetterCountsTheSameInEitherCase) {
    static_assert(Dialect::later("0.98D", "0.98c"));
    static_assert(!Dialect::later("0.98C", "0.98c"));
    static_assert(!Dialect::later("0.98c", "0.98C"));
    EXPECT_EQ(Dialect::rangeOf(Dialect::versions(release("", "skulltag")), "0.98D")->first, "0.98c");
}

TEST(DialectVersions, ALongRunOfDigitsStopsGrowingInsteadOfOverflowing) {
    static_assert(Dialect::later("4294967296", "4.0"));
    static_assert(Dialect::later("4294967296", "99999998"));
    static_assert(!Dialect::later("4294967296", "4294967297"));
    EXPECT_EQ(Dialect::rangeOf(Dialect::versions(release("", "gzdoom")), "4294967296")->first, "4.12.1");
}

TEST(DialectVersions, TenDigitsAreNeverEarlierThanNine) {
    static_assert(!Dialect::later("999999999", "1000000000"));
    static_assert(!Dialect::later("123456789", "1234567890"));
    static_assert(Dialect::later("1000000000", "99999998"));
}

TEST(DialectVersions, Skulltag0_98aReadsNoDmflags3OrSaveDirAndSavesInItsHomeFolder) {
    const Dialect::Traits latest = Dialect::of(release("", "skulltag"));
    const Dialect::Traits newest = Dialect::of(release("0.98c", "skulltag"));
    const Dialect::Traits older = Dialect::of(release("0.98a", "skulltag"));

    EXPECT_EQ(newest.features, latest.features);
    EXPECT_TRUE(latest.has(Feature::saveDirSetting));
    EXPECT_FALSE(older.has(Feature::saveDirSetting));
    EXPECT_FALSE(older.has(Feature::dmflags3));
    EXPECT_FALSE(older.has(Feature::savesWhereRun));
    EXPECT_TRUE(older.has(Feature::homeDotFolder));
    EXPECT_TRUE(older.has(Feature::compatflags2));
    EXPECT_EQ(older.features,
              latest.features & ~(Feature::dmflags3 | Feature::savesWhereRun | Feature::saveDirSetting));
    EXPECT_EQ(spelled(older, Feature::joins), "-connect");
    EXPECT_EQ(older.line, PortLine::skulltag);
}

TEST(DialectVersions, Skulltag0_97dTakesNoAutoloadShotdirStdoutOrCompatflags2) {
    const Dialect::Traits latest = Dialect::of(release("", "skulltag"));
    const Dialect::Traits d = Dialect::of(release("0.97d", "skulltag"));
    const Dialect::Features lacking = Feature::dmflags3 | Feature::savesWhereRun | Feature::saveDirSetting
                                      | Feature::noAutoload | Feature::shotdir | Feature::stdoutSwitch
                                      | Feature::compatflags2;

    EXPECT_EQ(d.features, latest.features & ~lacking);
    EXPECT_FALSE(d.has(Feature::keepsCompatflags));
    EXPECT_EQ(d.loads, SaveNames::path);
}

TEST(DialectVersions, UzdoomBefore5_0_0TakesNetmodeAndHasNoNetgameFolderOrCoopPreset) {
    const Dialect::Traits latest = Dialect::of(release("", "uzdoom"));
    const Dialect::Traits newest = Dialect::of(release("5.0.0", "uzdoom"));
    const Dialect::Traits older = Dialect::of(release("4.14.3", "uzdoom"));
    const Dialect::Features versioned = Feature::netmode | Feature::netgameFolder | Feature::coopPreset;

    EXPECT_EQ(newest.features, latest.features);
    EXPECT_FALSE(latest.has(Feature::netmode));
    EXPECT_TRUE(latest.has(Feature::netgameFolder));
    EXPECT_TRUE(latest.has(Feature::coopPreset));

    EXPECT_TRUE(older.has(Feature::netmode));
    EXPECT_EQ(spelled(older, Feature::netmode), "-netmode");
    EXPECT_FALSE(older.has(Feature::netgameFolder));
    EXPECT_FALSE(older.has(Feature::coopPreset));
    EXPECT_EQ(older.features & ~versioned, latest.features & ~versioned);
    EXPECT_EQ(older.loads, latest.loads);
    EXPECT_EQ(older.line, latest.line);
}

TEST(DialectVersions, ZdoomZandronumAndGzdoomBefore4_8ReadBit25AsGibs) {
    EXPECT_TRUE(named("zdoom").has(Feature::corpseGibs));
    EXPECT_TRUE(named("zandronum").has(Feature::corpseGibs));
    EXPECT_FALSE(named("gzdoom").has(Feature::corpseGibs));
    EXPECT_FALSE(named("uzdoom").has(Feature::corpseGibs));
    EXPECT_FALSE(Dialect::of(release("4.8.0", "gzdoom")).has(Feature::corpseGibs));
    EXPECT_TRUE(Dialect::of(release("4.4.0", "gzdoom")).has(Feature::corpseGibs));
}

TEST(DialectVersions, ReleasesCompareNumberByNumber) {
    static_assert(Dialect::later("0.30.0", "0.27.1"));
    static_assert(Dialect::later("10.2.0", "9.0.0"));
    static_assert(Dialect::later("1.10.0", "1.9.1"));
    static_assert(Dialect::later("4.12.1", "4.12"));
    static_assert(!Dialect::later("1.9.1", "1.10.0"));
    static_assert(!Dialect::later("16.0.0", "16.0.0"));
    static_assert(!Dialect::later("4.12", "4.12.0"));

    EXPECT_TRUE(Dialect::later("16.0.0", "15.3.0"));
}

TEST(DialectVersions, WoofNuggetAndCherryListTheirRangesNewestFirst) {
    const std::span<const Dialect::VersionRange> woof = Dialect::versions(release("", "woof"));
    const std::span<const Dialect::VersionRange> nugget = Dialect::versions(release("", "nugget-doom"));
    const std::span<const Dialect::VersionRange> cherry = Dialect::versions(release("", "cherry-doom"));

    EXPECT_EQ(firsts(woof),
              (std::vector<std::string_view>{"16.0.0", "15.0.0", "10.2.0", "10.0.0", "9.0.0", "8.1.0", "6.1.0"}));
    EXPECT_EQ(labels(woof),
              (std::vector<std::string_view>{"16.0.0 and later",
                                             "15.0.0 to 15.3.0",
                                             "10.2.0 to 14.5.0",
                                             "10.0.0 to 10.1.0",
                                             "9.0.0",
                                             "8.1.0",
                                             "6.1.0 to 8.0.0"}));

    EXPECT_EQ(firsts(nugget),
              (std::vector<std::string_view>{"6.0.0", "4.0.0", "1.10.0", "1.9.0", "1.8.0", "1.6.1", "1.0.0"}));
    EXPECT_EQ(labels(nugget),
              (std::vector<std::string_view>{"6.0.0 and later",
                                             "4.0.0 to 5.1.0",
                                             "1.10.0 to 3.3.0",
                                             "1.9.0 to 1.9.1",
                                             "1.8.0",
                                             "1.6.1 to 1.7.0",
                                             "1.0.0 to 1.6.0"}));

    EXPECT_EQ(firsts(cherry), (std::vector<std::string_view>{"2.1.0", "1.0.0"}));
    EXPECT_EQ(cherry[0].label, "2.1.0 and later");
    EXPECT_EQ(cherry[1].label, "1.0.0 to 2.0.0");

    for (const char *file : {"woof", "nugget-doom", "cherry-doom"}) {
        EXPECT_TRUE(Dialect::needsVersion(release("", file))) << file;
    }
}

TEST(DialectVersions, WoofNuggetAndCherryPickedByHandHaveTheirOwnRanges) {
    Engine engine = release("", "mystery-engine");

    engine.port = Port::Port::woof;

    EXPECT_EQ(Dialect::versions(engine).size(), 7U);

    engine.port = Port::Port::nugget;

    EXPECT_EQ(Dialect::versions(engine).size(), 7U);

    engine.port = Port::Port::cherry;

    EXPECT_EQ(Dialect::versions(engine).size(), 2U);
}

namespace {

constexpr Dialect::Features RECORDING_RULES =
        Feature::recordsWithComplevel | Feature::recordsWithSkill | Feature::continueComplevel;

constexpr Dialect::Features WOOF_VERSIONED = RECORDING_RULES | Feature::continues | Feature::deathmatch3
                                             | Feature::shotdir | Feature::coopSpawns | Feature::soloNet
                                             | Feature::noAutoload | Dialect::NETGAME_CHOCOLATE;

constexpr int ANY_SLOT = Dialect::Traits{}.lastSlot;

struct WoofExpected {
    std::string_view first;

    // Which of WOOF_VERSIONED the range has.
    Dialect::Features has;

    Complevels complevel;
    std::string_view autosave;
    int lastSlot;
    Netgames netgame;
};

constexpr Dialect::Features WOOF_15 = WOOF_VERSIONED & ~RECORDING_RULES;
constexpr Dialect::Features WOOF_10 = WOOF_15 & ~Dialect::Features(Feature::continues);
constexpr Dialect::Features WOOF_9 = WOOF_10 & ~(Feature::deathmatch3 | Feature::shotdir);
constexpr Dialect::Features WOOF_8_1 = Feature::noAutoload;

constexpr std::array WOOF_EXPECTED = std::to_array<WoofExpected>({
        {"16.0.0", WOOF_VERSIONED, Complevels::woof, "autosave.dsg", 77, Netgames::chocolate},
        {"15.0.0", WOOF_15, Complevels::woofToMbf21, "autosave.dsg", 77, Netgames::chocolate},
        {"10.2.0", WOOF_15, Complevels::woofToMbf21, "", ANY_SLOT, Netgames::chocolate},
        {"10.0.0", WOOF_10, Complevels::woofToMbf21, "", ANY_SLOT, Netgames::chocolate},
        {"9.0.0", WOOF_9, Complevels::woofToMbf21, "", ANY_SLOT, Netgames::chocolate},
        {"8.1.0", WOOF_8_1, Complevels::woofToMbf21, "", ANY_SLOT, Netgames::none},
        {"6.1.0", {}, Complevels::woofToMbf21, "", ANY_SLOT, Netgames::none},
});

constexpr std::array NUGGET_EXPECTED = std::to_array<WoofExpected>({
        {"6.0.0", WOOF_VERSIONED, Complevels::woof, "autosave.dsg", 77, Netgames::chocolate},
        {"4.0.0", WOOF_15, Complevels::woofToMbf21, "autosave.dsg", 77, Netgames::chocolate},
        {"1.10.0", WOOF_15, Complevels::woofToMbf21, "", ANY_SLOT, Netgames::chocolate},
        {"1.9.0", WOOF_10, Complevels::woofToMbf21, "", ANY_SLOT, Netgames::chocolate},
        {"1.8.0", WOOF_9, Complevels::woofToMbf21, "", ANY_SLOT, Netgames::chocolate},
        {"1.6.1", WOOF_8_1, Complevels::woofToMbf21, "", ANY_SLOT, Netgames::none},
        {"1.0.0", {}, Complevels::woofToMbf21, "", ANY_SLOT, Netgames::none},
});

constexpr std::array CHERRY_EXPECTED = std::to_array<WoofExpected>({
        {"2.1.0", WOOF_15, Complevels::woofToMbf21, "autosave.dsg", 77, Netgames::chocolate},
        {"1.0.0", WOOF_15, Complevels::woofToMbf21, "", ANY_SLOT, Netgames::chocolate},
});

void speaksEachRelease(const char *file, const std::span<const WoofExpected> expected) {
    const Dialect::Traits latest = Dialect::of(release("", file));
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", file));

    ASSERT_EQ(ranges.size(), expected.size()) << file;

    for (size_t at = 0; at < ranges.size(); ++at) {
        const WoofExpected &want = expected[at];
        const Dialect::Traits traits = Dialect::of(release(std::string(want.first), file));

        EXPECT_EQ(ranges[at].first, want.first) << file;
        EXPECT_EQ(traits.features & WOOF_VERSIONED, want.has) << file << " " << want.first;
        EXPECT_EQ(traits.complevel, want.complevel) << file << " " << want.first;
        EXPECT_EQ(traits.autosave, want.autosave) << file << " " << want.first;
        EXPECT_EQ(traits.lastSlot, want.lastSlot) << file << " " << want.first;
        EXPECT_EQ(traits.netgame, want.netgame) << file << " " << want.first;

        EXPECT_EQ(traits.features & ~WOOF_VERSIONED, latest.features & ~WOOF_VERSIONED) << file << " " << want.first;
        EXPECT_EQ(traits.loads, latest.loads) << file << " " << want.first;
        EXPECT_EQ(traits.changes.data(), latest.changes.data()) << file << " " << want.first;
        EXPECT_EQ(traits.configName, latest.configName) << file << " " << want.first;
        EXPECT_EQ(traits.port, latest.port) << file << " " << want.first;
    }

    const Dialect::Traits newest = Dialect::of(release(std::string(ranges.front().first), file));

    EXPECT_EQ(newest.features, latest.features) << file;
}

}

TEST(DialectVersions, EveryRangeOfWoofSpeaksItsRelease) {
    speaksEachRelease("woof", WOOF_EXPECTED);
}

TEST(DialectVersions, EveryRangeOfNuggetDoomSpeaksItsRelease) {
    speaksEachRelease("nugget-doom", NUGGET_EXPECTED);
}

TEST(DialectVersions, EveryRangeOfCherryDoomSpeaksItsRelease) {
    speaksEachRelease("cherry-doom", CHERRY_EXPECTED);
}

TEST(DialectPicked, APickedPortIsSpokenWhateverTheName) {
    for (const bool dos : {false, true}) {
        for (const Port::Port each : Dialect::picks(dos)) {
            for (const char *file : {"/games/mystery-engine", "/games/gzdoom", "/games/doom2.exe"}) {
                Engine engine = release("", file);

                engine.dosbox = dos;
                engine.port = each;

                EXPECT_EQ(Dialect::of(engine).port, each) << Port::known(each).id << " on " << file;
            }
        }
    }
}

TEST(DialectVersions, DsdaDoomObeysTheSaveFolderOverOrganizedSavesFrom0_28_3) {
    for (const char *version : {"", "0.29.4", "0.28.3"}) {
        const Dialect::Traits traits = Dialect::of(release(version));

        EXPECT_TRUE(traits.has(Feature::saveFolder)) << version;
        EXPECT_FALSE(traits.has(Feature::organizedSavesWin)) << version;
    }

    for (const char *version : {"0.28.2", "0.28.0", "0.21.1"}) {
        const Dialect::Traits traits = Dialect::of(release(version));

        EXPECT_TRUE(traits.has(Feature::saveFolder)) << version;
        EXPECT_TRUE(traits.has(Feature::organizedSavesWin)) << version;
    }
}

TEST(DialectVersions, DsdaDoomDataRootTakesAFolderSlotBefore0_26_0) {
    EXPECT_FALSE(Dialect::of(release("")).has(Feature::dataRootTakesSlot));
    EXPECT_FALSE(Dialect::of(release("0.26.0")).has(Feature::dataRootTakesSlot));
    EXPECT_TRUE(Dialect::of(release("0.25.6")).has(Feature::dataRootTakesSlot));
    EXPECT_TRUE(Dialect::of(release("0.21.1")).has(Feature::dataRootTakesSlot));
}

TEST(DialectVersions, DsdaDoomAlwaysKeepsItsHomeDotFolderBefore0_28_0) {
    EXPECT_FALSE(Dialect::of(release("0.28.0")).has(Feature::homeDotFolder));
    EXPECT_TRUE(Dialect::of(release("0.27.5")).has(Feature::homeDotFolder));
    EXPECT_TRUE(Dialect::of(release("0.21.1")).has(Feature::homeDotFolder));
}

TEST(DialectVersions, NyanDoomListsItsRangesNewestFirst) {
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", "nyan-doom"));

    EXPECT_EQ(firsts(ranges), (std::vector<std::string_view>{"1.6.1", "1.2.1", "1.1.8", "1.1.5", "1.1.1", "1.1.0"}));
    EXPECT_EQ(labels(ranges),
              (std::vector<std::string_view>{"1.6.1 and later",
                                             "1.2.1 to 1.6.0",
                                             "1.1.8 to 1.2.0",
                                             "1.1.5 to 1.1.7",
                                             "1.1.1 to 1.1.4",
                                             "1.1.0"}));
    EXPECT_TRUE(Dialect::needsVersion(release("", "nyan-doom")));
}

namespace {

constexpr Dialect::Features NYAN_VERSIONED = Feature::loadgame | Feature::organizedSavesWin
                                             | Feature::continuesWithSkill | Feature::homeDotFolder
                                             | Feature::limitRemoving;

struct NyanExpected {
    std::string_view first;

    // Which of NYAN_VERSIONED the range has.
    Dialect::Features has;
};

constexpr Dialect::Features HAS_NYAN_1_1_8 = Feature::organizedSavesWin | Feature::limitRemoving;
constexpr Dialect::Features HAS_NYAN_1_1_1 = HAS_NYAN_1_1_8 | Feature::homeDotFolder;

constexpr std::array NYAN_EXPECTED = std::to_array<NyanExpected>({
        {"1.6.1", Feature::loadgame | Feature::limitRemoving},
        {"1.2.1", Feature::limitRemoving},
        {"1.1.8", HAS_NYAN_1_1_8},
        {"1.1.5", HAS_NYAN_1_1_1 | Feature::continuesWithSkill},
        {"1.1.1", HAS_NYAN_1_1_1},
        {"1.1.0", Feature::organizedSavesWin | Feature::homeDotFolder},
});

}

TEST(DialectVersions, EveryRangeOfNyanDoomSpeaksItsRelease) {
    const Dialect::Traits latest = Dialect::of(release("", "nyan-doom"));
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", "nyan-doom"));

    ASSERT_EQ(ranges.size(), NYAN_EXPECTED.size());

    for (size_t at = 0; at < ranges.size(); ++at) {
        const NyanExpected &want = NYAN_EXPECTED[at];
        const Dialect::Traits traits = Dialect::of(release(std::string(want.first), "nyan-doom"));

        EXPECT_EQ(ranges[at].first, want.first);
        EXPECT_EQ(traits.features & NYAN_VERSIONED, want.has) << want.first;

        EXPECT_EQ(traits.features & ~NYAN_VERSIONED, latest.features & ~NYAN_VERSIONED) << want.first;
        EXPECT_EQ(traits.loads, latest.loads) << want.first;
        EXPECT_EQ(traits.changes.data(), latest.changes.data()) << want.first;
        EXPECT_EQ(traits.configName, latest.configName) << want.first;
        EXPECT_EQ(traits.lastSlot, latest.lastSlot) << want.first;
        EXPECT_EQ(traits.complevel, latest.complevel) << want.first;
        EXPECT_EQ(traits.line, PortLine::nyan) << want.first;
    }

    EXPECT_EQ(Dialect::of(release("1.6.1", "nyan-doom")).features, latest.features);
}

TEST(DialectVersions, NyanDoomBreaksFallBetweenTheReleasesEitherSide) {
    const auto at = [](const char *version) { return Dialect::of(release(version, "nyan-doom")); };

    EXPECT_TRUE(at("1.6.2").has(Feature::loadgame));
    EXPECT_TRUE(at("1.6.1").has(Feature::loadgame));
    EXPECT_FALSE(at("1.6.0").has(Feature::loadgame));

    EXPECT_FALSE(at("1.2.1").has(Feature::organizedSavesWin));
    EXPECT_TRUE(at("1.2.0").has(Feature::organizedSavesWin));
    EXPECT_TRUE(at("1.2.0").has(Feature::saveFolder));

    EXPECT_FALSE(at("1.1.8").has(Feature::homeDotFolder));
    EXPECT_TRUE(at("1.1.7").has(Feature::homeDotFolder));

    EXPECT_FALSE(at("1.1.8").has(Feature::continuesWithSkill));
    EXPECT_TRUE(at("1.1.7").has(Feature::continuesWithSkill));
    EXPECT_TRUE(at("1.1.5").has(Feature::continuesWithSkill));
    EXPECT_FALSE(at("1.1.4").has(Feature::continuesWithSkill));

    EXPECT_TRUE(at("1.1.1").has(Feature::limitRemoving));
    EXPECT_FALSE(at("1.1.0").has(Feature::limitRemoving));
    EXPECT_FALSE(at("1.1").has(Feature::limitRemoving));
    EXPECT_FALSE(at("1.1.9.1").has(Feature::homeDotFolder));
}

TEST(DialectVersions, HelionListsItsRangesNewestFirst) {
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", "helion"));

    EXPECT_EQ(firsts(ranges),
              (std::vector<std::string_view>{
                      "0.9.9.0", "0.9.8.0", "0.9.7.0", "0.9.4.0", "0.9.2.9", "0.9.2.4", "0.9.0.0"}));
    EXPECT_EQ(labels(ranges),
              (std::vector<std::string_view>{"0.9.9.0 and later",
                                             "0.9.8.0",
                                             "0.9.7.0",
                                             "0.9.4.0 to 0.9.6.1",
                                             "0.9.2.9 to 0.9.3.0",
                                             "0.9.2.4 to 0.9.2.8",
                                             "0.9.0.0 to 0.9.2.3"}));
    EXPECT_TRUE(Dialect::needsVersion(release("", "helion")));
}

namespace {

constexpr Dialect::Features HELION_VERSIONED = Feature::complevel | Feature::soloNet | Feature::savesBesideProgram
                                               | Feature::pistolStart | Feature::nomusic | Feature::saveFolder
                                               | Feature::configFile;

struct HelionExpected {
    std::string_view first;

    // Which of HELION_VERSIONED the range has.
    Dialect::Features has;

    Complevels complevel;
};

constexpr Dialect::Features HELION_FOLDERS = Feature::saveFolder | Feature::configFile;
constexpr Dialect::Features HAS_HELION_0_9_7 = HELION_FOLDERS | Feature::pistolStart | Feature::nomusic;
constexpr Dialect::Features HAS_HELION_0_9_4 = HAS_HELION_0_9_7 | Feature::savesBesideProgram;

constexpr std::array HELION_EXPECTED = std::to_array<HelionExpected>({
        {"0.9.9.0", HAS_HELION_0_9_7 | Feature::complevel | Feature::soloNet, Complevels::helion},
        {"0.9.8.0", HAS_HELION_0_9_7 | Feature::soloNet, Complevels::none},
        {"0.9.7.0", HAS_HELION_0_9_7, Complevels::none},
        {"0.9.4.0", HAS_HELION_0_9_4, Complevels::none},
        {"0.9.2.9", HELION_FOLDERS | Feature::savesBesideProgram, Complevels::none},
        {"0.9.2.4", Feature::configFile | Feature::savesBesideProgram, Complevels::none},
        {"0.9.0.0", Feature::savesBesideProgram, Complevels::none},
});

}

TEST(DialectVersions, EveryRangeOfHelionSpeaksItsRelease) {
    const Dialect::Traits latest = Dialect::of(release("", "helion"));
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", "helion"));

    ASSERT_EQ(ranges.size(), HELION_EXPECTED.size());

    for (size_t at = 0; at < ranges.size(); ++at) {
        const HelionExpected &want = HELION_EXPECTED[at];
        const Dialect::Traits traits = Dialect::of(release(std::string(want.first), "helion"));

        EXPECT_EQ(ranges[at].first, want.first);
        EXPECT_EQ(traits.features & HELION_VERSIONED, want.has) << want.first;
        EXPECT_EQ(traits.complevel, want.complevel) << want.first;
        EXPECT_EQ(spelled(traits, Feature::fast), "+sv_fastmonsters") << want.first;
        EXPECT_EQ(Dialect::arg(traits, Feature::fast).word, "1") << want.first;
        EXPECT_EQ(traits.changes.data(), latest.changes.data()) << want.first;
        EXPECT_EQ(Dialect::compatibility(traits) == Dialect::Compatibility::none, want.complevel == Complevels::none)
                << want.first;

        EXPECT_EQ(traits.features & ~HELION_VERSIONED, latest.features & ~HELION_VERSIONED) << want.first;
        EXPECT_TRUE(traits.has(Feature::onePatch)) << want.first;
        EXPECT_EQ(traits.loads, latest.loads) << want.first;
        EXPECT_EQ(traits.saveExt, latest.saveExt) << want.first;
        EXPECT_EQ(traits.line, latest.line) << want.first;
    }

    EXPECT_EQ(Dialect::of(release("0.9.9.0", "helion")).features, latest.features);
}

TEST(DialectVersions, HelionTakesTheComplevelBySwitchOnlyWhereItReadsIt) {
    const Dialect::Traits complevel = Dialect::of(release("0.9.9.0", "helion"));
    const Dialect::Traits before = Dialect::of(release("0.9.8.0", "helion"));

    EXPECT_EQ(spelled(complevel, Feature::complevel), "+complevel");
    EXPECT_EQ(spelled(before, Feature::complevel), "");
    EXPECT_TRUE(Dialect::complevels(before.complevel).empty());
}

TEST(DialectVersions, HelionBreaksFallBetweenTheReleasesEitherSide) {
    const auto at = [](const char *version) { return Dialect::of(release(version, "helion")); };

    EXPECT_TRUE(at("0.9.9.0").has(Feature::complevel));
    EXPECT_FALSE(at("0.9.8.0").has(Feature::complevel));

    EXPECT_TRUE(at("0.9.8.0").has(Feature::soloNet));
    EXPECT_FALSE(at("0.9.7.0").has(Feature::soloNet));

    EXPECT_FALSE(at("0.9.7.0").has(Feature::savesBesideProgram));
    EXPECT_TRUE(at("0.9.6.1").has(Feature::savesBesideProgram));

    EXPECT_TRUE(at("0.9.4.0").has(Feature::pistolStart));
    EXPECT_TRUE(at("0.9.4.0").has(Feature::nomusic));
    EXPECT_FALSE(at("0.9.3.0").has(Feature::pistolStart));
    EXPECT_FALSE(at("0.9.3.0").has(Feature::nomusic));

    EXPECT_TRUE(at("0.9.2.9").has(Feature::saveFolder));
    EXPECT_FALSE(at("0.9.2.8").has(Feature::saveFolder));

    EXPECT_TRUE(at("0.9.2.4").has(Feature::configFile));
    EXPECT_FALSE(at("0.9.2.3").has(Feature::configFile));
    EXPECT_FALSE(at("0.9.1.0").has(Feature::configFile));
}

TEST(DialectVersions, EternityListsItsRangesNewestFirst) {
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", "eternity"));

    EXPECT_EQ(firsts(ranges),
              (std::vector<std::string_view>{"4.02.00", "4.01.00", "3.40.46", "3.40.25", "3.40.10", "3.40.00"}));
    EXPECT_EQ(labels(ranges),
              (std::vector<std::string_view>{"4.02.00 and later",
                                             "4.01.00",
                                             "3.40.46 to 4.00.00",
                                             "3.40.25 to 3.40.37",
                                             "3.40.10 to 3.40.20",
                                             "3.40.00"}));
    EXPECT_TRUE(Dialect::needsVersion(release("", "eternity")));
}

namespace {

constexpr Dialect::Features ETERNITY_VERSIONED =
        Feature::map | Feature::continues | Feature::fullscreen | Feature::windowed | Feature::userFolder
        | Feature::userFolderInConfigHome | Feature::installedBaseFolder | Feature::vanillaMode;

struct EternityExpected {
    std::string_view first;

    // Which of ETERNITY_VERSIONED the range has.
    Dialect::Features has;
};

constexpr Dialect::Features ETERNITY_FOLDERS =
        Feature::userFolder | Feature::userFolderInConfigHome | Feature::installedBaseFolder;
constexpr Dialect::Features ETERNITY_DISPLAY = Feature::fullscreen | Feature::windowed;
constexpr Dialect::Features HAS_ETERNITY_3_40_46 = ETERNITY_FOLDERS | ETERNITY_DISPLAY | Feature::vanillaMode;

constexpr std::array ETERNITY_EXPECTED = std::to_array<EternityExpected>({
        {"4.02.00", ETERNITY_FOLDERS | Feature::vanillaMode | Feature::map | Feature::continues},
        {"4.01.00", HAS_ETERNITY_3_40_46 | Feature::map | Feature::continues},
        {"3.40.46", HAS_ETERNITY_3_40_46},
        {"3.40.25", ETERNITY_DISPLAY | Feature::vanillaMode | Feature::userFolder},
        {"3.40.10", ETERNITY_DISPLAY | Feature::vanillaMode},
        {"3.40.00", ETERNITY_DISPLAY},
});

}

TEST(DialectVersions, EveryRangeOfEternitySpeaksItsRelease) {
    const Dialect::Traits latest = Dialect::of(release("", "eternity"));
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", "eternity"));

    ASSERT_EQ(ranges.size(), ETERNITY_EXPECTED.size());

    for (size_t at = 0; at < ranges.size(); ++at) {
        const EternityExpected &want = ETERNITY_EXPECTED[at];
        const Dialect::Traits traits = Dialect::of(release(std::string(want.first), "eternity"));

        EXPECT_EQ(ranges[at].first, want.first);
        EXPECT_EQ(traits.features & ETERNITY_VERSIONED, want.has) << want.first;
        EXPECT_EQ(traits.features & ~ETERNITY_VERSIONED, latest.features & ~ETERNITY_VERSIONED) << want.first;
        EXPECT_EQ(traits.changes.data(), latest.changes.data()) << want.first;
        EXPECT_EQ(traits.loads, latest.loads) << want.first;
        EXPECT_EQ(traits.saveExt, latest.saveExt) << want.first;
        EXPECT_EQ(traits.line, latest.line) << want.first;
        EXPECT_FALSE(traits.has(Feature::displayKept)) << want.first;
    }

    EXPECT_EQ(Dialect::of(release("4.06.00", "eternity")).features, latest.features);
}

TEST(DialectVersions, EternitySpellsTheMapAndTheDisplayTheWayEachReleaseReadsThem) {
    const Dialect::Traits latest = Dialect::of(release("4.06.00", "eternity"));
    const Dialect::Traits display = Dialect::of(release("4.01.00", "eternity"));

    EXPECT_EQ(spelled(latest, Feature::map), "-warp");
    EXPECT_EQ(spelled(latest, Feature::warp), "-warp");
    EXPECT_EQ(spelled(latest, Feature::continues), "-recordfromto");
    EXPECT_EQ(spelled(latest, Feature::fullscreen), "");
    EXPECT_EQ(spelled(display, Feature::fullscreen), "-fullscreen");
    EXPECT_EQ(spelled(display, Feature::windowed), "-window");
}

TEST(DialectVersions, EternityBreaksFallBetweenTheReleasesEitherSide) {
    const auto at = [](const char *version) { return Dialect::of(release(version, "eternity")); };

    EXPECT_FALSE(at("4.02.00").has(Feature::fullscreen));
    EXPECT_TRUE(at("4.01.00").has(Feature::fullscreen));
    EXPECT_TRUE(at("4.01.00").has(Feature::windowed));

    EXPECT_TRUE(at("4.01.00").has(Feature::map));
    EXPECT_TRUE(at("4.01.00").has(Feature::continues));
    EXPECT_FALSE(at("4.00.00").has(Feature::map));
    EXPECT_FALSE(at("4.00.00").has(Feature::continues));
    EXPECT_TRUE(at("4.00.00").has(Feature::fullscreen));

    EXPECT_TRUE(at("3.42.03a").has(Feature::userFolderInConfigHome));
    EXPECT_TRUE(at("3.40.46").has(Feature::userFolderInConfigHome));
    EXPECT_FALSE(at("3.40.37").has(Feature::userFolderInConfigHome));
    EXPECT_TRUE(at("3.40.46").has(Feature::installedBaseFolder));
    EXPECT_FALSE(at("3.40.37").has(Feature::installedBaseFolder));
    EXPECT_TRUE(at("3.40.37").has(Feature::userFolder));

    EXPECT_TRUE(at("3.40.25").has(Feature::userFolder));
    EXPECT_FALSE(at("3.40.20").has(Feature::userFolder));

    EXPECT_TRUE(at("3.40.10").has(Feature::vanillaMode));
    EXPECT_FALSE(at("3.40.00").has(Feature::vanillaMode));
    EXPECT_FALSE(at("3.39.20").has(Feature::vanillaMode));
}

TEST(DialectFeatures, ARangeWithoutTheComplevelOffersNoLevels) {
    const Dialect::Traits base{.features = Feature::complevel, .complevel = Complevels::dsda};
    const Dialect::VersionRange range{
            .first = "1",
            .label = "one",
            .lacks = Feature::complevel,
            .gains = {},
            .changes = {},
            .special = nullptr,
    };

    EXPECT_EQ(Dialect::atRelease(base, range).complevel, Complevels::none);
    EXPECT_EQ(Dialect::atRelease(base, Dialect::VersionRange{}).complevel, Complevels::dsda);
}

TEST(DialectFeatures, OnlyHelionTakesOnePatch) {
    EXPECT_TRUE(named("helion").has(Feature::onePatch));

    for (const char *file : {"gzdoom", "dsda-doom", "nyan-doom", "prboom-plus", "woof", "eternity", "chocolate-doom"}) {
        EXPECT_FALSE(named(file).has(Feature::onePatch)) << file;
    }
}

TEST(DialectVersions, ChocolateDoomListsItsRangesNewestFirst) {
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", "chocolate-doom"));

    EXPECT_EQ(firsts(ranges), (std::vector<std::string_view>{"3.1.1", "3.1.0", "3.0.0", "2.1.0", "2.0.0"}));
    EXPECT_EQ(labels(ranges),
              (std::vector<std::string_view>{"3.1.1 and later", "3.1.0", "3.0.0 to 3.0.1", "2.1.0 to 2.3.0", "2.0.0"}));
    EXPECT_TRUE(Dialect::needsVersion(release("", "chocolate-doom")));
}

TEST(DialectVersions, CrispyDoomListsItsRangesNewestFirst) {
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", "crispy-doom"));

    EXPECT_EQ(firsts(ranges),
              (std::vector<std::string_view>{"7.1",
                                             "7.0",
                                             "5.11",
                                             "5.9.2",
                                             "5.9.0",
                                             "5.6.4",
                                             "5.4",
                                             "5.3",
                                             "5.0",
                                             "4.1",
                                             "4.0",
                                             "1.5",
                                             "1.3",
                                             "1.1",
                                             "1.0"}));
    EXPECT_EQ(labels(ranges),
              (std::vector<std::string_view>{"7.1 and later",
                                             "7.0",
                                             "5.11 to 6.0",
                                             "5.9.2 to 5.10.3",
                                             "5.9.0 to 5.9.1",
                                             "5.6.4 to 5.8.0",
                                             "5.4 to 5.6.3",
                                             "5.3",
                                             "5.0 to 5.2",
                                             "4.1 to 4.3",
                                             "4.0",
                                             "1.5 to 3.5",
                                             "1.3 to 1.4",
                                             "1.1 to 1.2",
                                             "1.0"}));
    EXPECT_TRUE(Dialect::needsVersion(release("", "crispy-doom")));
}

namespace {

// The features in which the Heretic, Hexen and Strife builds differ from each other or from the Doom build.
constexpr Dialect::Features OTHER_GAME_VERSIONED =
        Feature::fast | Feature::turbo | Feature::deathmatch | Feature::altDeathmatch | Feature::alwaysDeathmatch
        | Feature::noCheats | Feature::deh | Feature::bex | Feature::warp | Feature::pistolStart | Feature::levelstat
        | Feature::coopSpawns | Feature::doubleAmmo | Feature::flipWeapons | Feature::flipLevels | Feature::deathmatch3
        | Feature::noAutoload | Feature::saveFolder | Feature::homeDotFolder | Feature::longtics
        | Feature::freedoomFolders | Feature::complevel;

constexpr Dialect::Features ALL_OTHER_GAMES = Feature::noAutoload | Feature::saveFolder | Feature::longtics;
constexpr Dialect::Features HERETIC_READS =
        ALL_OTHER_GAMES | Feature::deathmatch | Feature::deh | Feature::bex | Feature::warp;
constexpr Dialect::Features HEXEN_READS = ALL_OTHER_GAMES | Feature::deathmatch;
constexpr Dialect::Features STRIFE_READS = ALL_OTHER_GAMES | Feature::fast | Feature::turbo | Feature::deathmatch
                                           | Feature::altDeathmatch | Feature::alwaysDeathmatch | Feature::noCheats
                                           | Feature::deh | Feature::bex | Feature::warp;

struct OtherGame {
    const char *file;
    Port::Port port;
    std::string_view name;

    // The Doom build of the same package.
    const char *doom;

    Dialect::Features reads;
};

constexpr std::array OTHER_GAMES = std::to_array<OtherGame>({
        {"chocolate-heretic", Port::Port::chocolateHeretic, "Chocolate Heretic", "chocolate-doom", HERETIC_READS},
        {"chocolate-hexen", Port::Port::chocolateHexen, "Chocolate Hexen", "chocolate-doom", HEXEN_READS},
        {"chocolate-strife", Port::Port::chocolateStrife, "Chocolate Strife", "chocolate-doom", STRIFE_READS},
        {"crispy-heretic",
         Port::Port::crispyHeretic,
         "Crispy Heretic",
         "crispy-doom",
         HERETIC_READS | Feature::fast | Feature::pistolStart | Feature::levelstat | Feature::coopSpawns},
        {"crispy-hexen", Port::Port::crispyHexen, "Crispy Hexen", "crispy-doom", HEXEN_READS | Feature::fast},
        {"crispy-strife",
         Port::Port::crispyStrife,
         "Crispy Strife",
         "crispy-doom",
         STRIFE_READS | Feature::doubleAmmo | Feature::flipWeapons},
});

}

TEST(DialectOtherGames, EachGameIsASeparatePortThatNobodyPicks) {
    for (const OtherGame &game : OTHER_GAMES) {
        const Port::Known &known = Port::known(game.port);

        EXPECT_EQ(named(game.file).port, game.port) << game.file;
        EXPECT_EQ(known.name, game.name) << game.file;
        EXPECT_TRUE(known.id.empty()) << game.file;
        EXPECT_TRUE(known.dataName.empty()) << game.file;
        EXPECT_FALSE(std::ranges::contains(Dialect::picks(false), game.port)) << game.file;
        EXPECT_FALSE(std::ranges::contains(Dialect::picks(true), game.port)) << game.file;
    }
}

TEST(DialectOtherGames, EachGameReadsItsOwnSwitchesAtTheLatestRelease) {
    for (const OtherGame &game : OTHER_GAMES) {
        const Dialect::Traits latest = named(game.file);
        const Dialect::Traits doom = named(game.doom);

        EXPECT_EQ(latest.features & OTHER_GAME_VERSIONED, game.reads) << game.file;
        EXPECT_EQ(latest.features & ~OTHER_GAME_VERSIONED, doom.features & ~OTHER_GAME_VERSIONED) << game.file;
        EXPECT_EQ(latest.complevel, Complevels::none) << game.file;
        EXPECT_EQ(Dialect::compatibility(latest), Dialect::Compatibility::none) << game.file;
        EXPECT_EQ(latest.line, PortLine::none) << game.file;
        EXPECT_EQ(latest.dialect, Dialect::Dialect::chocolate) << game.file;
        EXPECT_EQ(latest.netgame, Netgames::chocolate) << game.file;
        EXPECT_EQ(latest.loads, SaveNames::slot) << game.file;
        EXPECT_EQ(latest.configName, doom.configName) << game.file;
        EXPECT_EQ(spelled(latest, Feature::pwads), spelled(doom, Feature::pwads)) << game.file;
        EXPECT_EQ(Dialect::arg(latest, Feature::turbo), Dialect::arg(doom, Feature::turbo)) << game.file;
        EXPECT_TRUE(Dialect::sound(latest)) << game.file;
    }
}

TEST(DialectOtherGames, CrispyHereticStartsWithTheWandAndStopsOnItInADemoOrANetgame) {
    const Dialect::Traits heretic = named("crispy-heretic");
    const Dialect::Arg &wand = Dialect::arg(heretic, Feature::pistolStart);

    EXPECT_EQ(wand.name, "-wandstart");
    EXPECT_EQ(wand.refused, Dialect::RECORDING | Dialect::PLAYBACK | Dialect::NETGAME);
    EXPECT_TRUE(Dialect::describe(heretic, Feature::pistolStart).starts_with("Starts every map with only the staff "));
    EXPECT_EQ(Dialect::arg(named("crispy-doom"), Feature::pistolStart).name, "-pistolstart");
}

TEST(DialectOtherGames, CrispyHereticAndHexenStopOnFastInADemoOrANetgame) {
    constexpr std::string_view LEFT_OUT =
            "Left out while a demo is recorded or played and in a netgame, since the port stops on it there.";

    for (const char *file : {"crispy-heretic", "crispy-hexen"}) {
        const Dialect::Arg &fast = Dialect::arg(named(file), Feature::fast);

        EXPECT_EQ(fast.name, "-fast") << file;
        EXPECT_EQ(fast.refused, Dialect::RECORDING | Dialect::PLAYBACK | Dialect::NETGAME) << file;
        EXPECT_TRUE(Dialect::describe(named(file), Feature::fast).ends_with(LEFT_OUT)) << file;
    }

    EXPECT_TRUE(Dialect::describe(named("crispy-doom"), Feature::pistolStart).ends_with(LEFT_OUT));
    EXPECT_TRUE(Dialect::describe(named("crispy-doom"), Feature::fast).empty());

    for (const char *file : {"crispy-strife", "chocolate-strife", "crispy-doom"}) {
        EXPECT_EQ(Dialect::arg(named(file), Feature::fast).refused, 0U) << file;
    }
}

TEST(DialectOtherGames, StrifeSpellsDeathmatchAsNothingAtEveryRelease) {
    for (const char *file : {"chocolate-strife", "crispy-strife"}) {
        for (const Dialect::VersionRange &range : Dialect::versions(release("", file))) {
            const Dialect::Traits traits = Dialect::of(release(std::string(range.first), file));
            std::vector<std::string> args;

            EXPECT_TRUE(traits.has(Feature::deathmatch)) << file << ' ' << range.first;
            EXPECT_TRUE(traits.has(Feature::alwaysDeathmatch)) << file << ' ' << range.first;
            EXPECT_FALSE(Dialect::add(args, traits, Feature::deathmatch)) << file << ' ' << range.first;
            EXPECT_TRUE(args.empty()) << file << ' ' << range.first;
            EXPECT_EQ(spelled(traits, Feature::altDeathmatch), "-altdeath") << file << ' ' << range.first;
        }
    }

    for (const char *file : {"chocolate-doom", "crispy-doom", "chocolate-heretic", "crispy-hexen", "woof", "gzdoom"}) {
        EXPECT_FALSE(named(file).has(Feature::alwaysDeathmatch)) << file;
    }
}

TEST(DialectOtherGames, CrispyStrifeMirrorsTheWeaponsWithNoMirroredMaps) {
    const Dialect::Traits strife = named("crispy-strife");

    EXPECT_EQ(spelled(strife, Feature::flipWeapons), "-flipweapons");
    EXPECT_EQ(Dialect::describe(strife, Feature::flipWeapons), "Mirrors the weapon sprites.");
    EXPECT_FALSE(strife.has(Feature::flipLevels));
}

namespace {

struct OtherRanges {
    const char *file;
    std::vector<std::string_view> firsts;
    std::vector<std::string_view> labels;
};

}

TEST(DialectOtherGames, EachGameListsTheReleaseRangesWhereItBreaks) {
    const std::vector<OtherRanges> all = {
            {"chocolate-heretic",
             {"3.1.0", "3.0.0", "2.3.0", "2.0.0"},
             {"3.1.0 and later", "3.0.0 to 3.0.1", "2.3.0", "2.0.0 to 2.2.1"}},
            {"chocolate-hexen",
             {"3.1.0", "3.0.0", "2.3.0", "2.0.0"},
             {"3.1.0 and later", "3.0.0 to 3.0.1", "2.3.0", "2.0.0 to 2.2.1"}},
            {"chocolate-strife", {"3.1.0", "3.0.0", "2.0.0"}, {"3.1.0 and later", "3.0.0 to 3.0.1", "2.0.0 to 2.3.0"}},
            {"crispy-heretic",
             {"7.0", "5.11", "5.9.2", "5.9.0", "5.4", "5.0", "4.2", "1.5", "1.0"},
             {"7.0 and later",
              "5.11 to 6.0",
              "5.9.2 to 5.10.3",
              "5.9.0 to 5.9.1",
              "5.4 to 5.8.0",
              "5.0 to 5.3",
              "4.2 to 4.3",
              "1.5 to 3.3",
              "1.0 to 1.4"}},
            {"crispy-hexen",
             {"5.11", "5.4", "5.0", "4.2", "1.5", "1.0"},
             {"5.11 and later", "5.4 to 5.10.3", "5.0 to 5.3", "4.2 to 4.3", "1.5 to 3.3", "1.0 to 1.4"}},
            {"crispy-strife",
             {"7.0", "5.11", "5.4", "5.0", "4.2", "1.5", "1.0"},
             {"7.0 and later", "5.11 to 6.0", "5.4 to 5.10.3", "5.0 to 5.3", "4.2 to 4.3", "1.5 to 3.3", "1.0 to 1.4"}},
    };

    for (const OtherRanges &each : all) {
        const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", each.file));
        const Dialect::Traits latest = Dialect::of(release("", each.file));

        EXPECT_EQ(firsts(ranges), each.firsts) << each.file;
        EXPECT_EQ(labels(ranges), each.labels) << each.file;
        EXPECT_TRUE(Dialect::needsVersion(release("", each.file))) << each.file;
        EXPECT_EQ(Dialect::of(release(std::string(each.firsts.front()), each.file)).features, latest.features)
                << each.file;

        for (const Dialect::VersionRange &range : ranges) {
            const Dialect::Traits traits = Dialect::of(release(std::string(range.first), each.file));

            EXPECT_EQ(traits.complevel, Complevels::none) << each.file << ' ' << range.first;
            EXPECT_FALSE(traits.has(Feature::freedoomFolders)) << each.file << ' ' << range.first;

            // Crispy merges every -file in from 1.5, and the older releases are handed -merge as Chocolate is.
            if (range.first != "1.0") {
                EXPECT_EQ(traits.changes.data(), latest.changes.data()) << each.file << ' ' << range.first;
            }
        }
    }
}

namespace {

struct Break {
    const char *file;

    // The first release with the feature, then the last without it.
    const char *from;
    const char *before;

    Feature feature;
};

}

TEST(DialectOtherGames, EachBreakFallsBetweenTheReleasesEitherSide) {
    const std::vector<Break> gains = {
            {"chocolate-heretic", "3.1.0", "3.0.1", Feature::noAutoload},
            {"chocolate-heretic", "3.0.0", "2.3.0", Feature::saveFolder},
            {"chocolate-heretic", "2.3.0", "2.2.1", Feature::longtics},
            {"chocolate-hexen", "3.1.0", "3.0.1", Feature::noAutoload},
            {"chocolate-hexen", "3.0.0", "2.3.0", Feature::saveFolder},
            {"chocolate-hexen", "2.3.0", "2.2.1", Feature::longtics},
            {"chocolate-strife", "3.1.0", "3.0.1", Feature::noAutoload},
            {"chocolate-strife", "3.0.0", "2.3.0", Feature::saveFolder},
            {"crispy-heretic", "7.0", "6.0", Feature::coopSpawns},
            {"crispy-heretic", "5.11", "5.10.3", Feature::fast},
            {"crispy-heretic", "5.9.2", "5.9.1", Feature::pistolStart},
            {"crispy-heretic", "5.9.0", "5.8.0", Feature::levelstat},
            {"crispy-heretic", "5.4", "5.3", Feature::noAutoload},
            {"crispy-hexen", "5.11", "5.10.3", Feature::fast},
            {"crispy-hexen", "5.4", "5.3", Feature::noAutoload},
            {"crispy-strife", "7.0", "6.0", Feature::flipWeapons},
            {"crispy-strife", "5.11", "5.10.3", Feature::doubleAmmo},
            {"crispy-strife", "5.4", "5.3", Feature::noAutoload},
            {"crispy-heretic", "4.2", "3.3", Feature::saveFolder},
            {"crispy-hexen", "4.2", "3.3", Feature::saveFolder},
            {"crispy-strife", "4.2", "3.3", Feature::saveFolder},
            {"crispy-heretic", "4.2", "3.3", Feature::longtics},
            {"crispy-hexen", "4.2", "3.3", Feature::longtics},
    };

    for (const Break &each : gains) {
        EXPECT_TRUE(Dialect::of(release(each.from, each.file)).has(each.feature)) << each.file << ' ' << each.from;
        EXPECT_FALSE(Dialect::of(release(each.before, each.file)).has(each.feature)) << each.file << ' ' << each.before;
    }

    // The settings and saves move out of the dot folder in the home folder.
    const std::vector<Break> leaves = {
            {"chocolate-heretic", "3.0.0", "2.3.0", Feature::homeDotFolder},
            {"chocolate-hexen", "3.0.0", "2.3.0", Feature::homeDotFolder},
            {"chocolate-strife", "3.0.0", "2.3.0", Feature::homeDotFolder},
            {"crispy-heretic", "5.0", "4.3", Feature::homeDotFolder},
            {"crispy-hexen", "5.0", "4.3", Feature::homeDotFolder},
            {"crispy-strife", "5.0", "4.3", Feature::homeDotFolder},
    };

    for (const Break &each : leaves) {
        EXPECT_FALSE(Dialect::of(release(each.from, each.file)).has(each.feature)) << each.file << ' ' << each.from;
        EXPECT_TRUE(Dialect::of(release(each.before, each.file)).has(each.feature)) << each.file << ' ' << each.before;
    }

    EXPECT_TRUE(Dialect::of(release("2.0.0", "chocolate-strife")).has(Feature::longtics));
    EXPECT_TRUE(Dialect::of(release("1.0", "crispy-strife")).has(Feature::longtics));

    for (const char *file : {"crispy-heretic", "crispy-hexen", "crispy-strife"}) {
        EXPECT_EQ(spelled(Dialect::of(release("1.5", file)), Feature::pwads), "-file") << file;
        EXPECT_EQ(spelled(Dialect::of(release("1.4", file)), Feature::pwads), "-merge") << file;
        EXPECT_TRUE(Dialect::of(release("3.3", file)).has(Feature::homeDotFolder)) << file;
    }
}

// Each era is chained from the one above it, so a slip anywhere in the chain shows in the oldest row.
TEST(DialectOtherGames, TheOldestRowReadsWhatTheFirstReleaseRead) {
    constexpr Dialect::Features HERETIC =
            Feature::deathmatch | Feature::deh | Feature::bex | Feature::warp | Feature::homeDotFolder;
    constexpr Dialect::Features HEXEN = Feature::deathmatch | Feature::homeDotFolder;
    constexpr Dialect::Features STRIFE = Feature::fast | Feature::turbo | Feature::deathmatch | Feature::altDeathmatch
                                         | Feature::alwaysDeathmatch | Feature::noCheats | Feature::deh | Feature::bex
                                         | Feature::warp | Feature::longtics | Feature::homeDotFolder;

    const std::vector<std::pair<const char *, Dialect::Features>> oldest = {
            {"chocolate-heretic", HERETIC},
            {"chocolate-hexen", HEXEN},
            {"chocolate-strife", STRIFE},
            {"crispy-heretic", HERETIC},
            {"crispy-hexen", HEXEN},
            {"crispy-strife", STRIFE},
    };

    for (const auto &[file, expected] : oldest) {
        const std::string first(Dialect::versions(release("", file)).back().first);
        const Dialect::Traits traits = Dialect::of(release(first, file));

        EXPECT_EQ(traits.features & OTHER_GAME_VERSIONED, expected) << file << ' ' << first;
        EXPECT_EQ(spelled(traits, Feature::pwads), "-merge") << file;
        EXPECT_TRUE(Dialect::sound(traits)) << file;
    }
}

namespace {

constexpr Dialect::Features CHOCOLATE_VERSIONED =
        Feature::noAutoload | Feature::freedoomFolders | Feature::saveFolder | Feature::homeDotFolder;

struct ChocolateExpected {
    std::string_view first;

    // Which of the versioned features the range has.
    Dialect::Features has;

    Complevels complevel;
};

constexpr Dialect::Features HAS_CHOCOLATE_3_1 = Feature::noAutoload | Feature::freedoomFolders | Feature::saveFolder;

constexpr std::array CHOCOLATE_EXPECTED = std::to_array<ChocolateExpected>({
        {"3.1.1", HAS_CHOCOLATE_3_1, Complevels::gameVersion},
        {"3.1.0", HAS_CHOCOLATE_3_1, Complevels::gameVersion12},
        {"3.0.0", Feature::saveFolder, Complevels::gameVersion1666},
        {"2.1.0", Feature::homeDotFolder, Complevels::gameVersion1666},
        {"2.0.0", Feature::homeDotFolder, Complevels::gameVersion19},
});

}

TEST(DialectVersions, EveryRangeOfChocolateDoomSpeaksItsRelease) {
    const Dialect::Traits latest = Dialect::of(release("", "chocolate-doom"));
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", "chocolate-doom"));

    ASSERT_EQ(ranges.size(), CHOCOLATE_EXPECTED.size());

    for (size_t at = 0; at < ranges.size(); ++at) {
        const ChocolateExpected &want = CHOCOLATE_EXPECTED[at];
        const Dialect::Traits traits = Dialect::of(release(std::string(want.first), "chocolate-doom"));

        EXPECT_EQ(ranges[at].first, want.first);
        EXPECT_EQ(traits.features & CHOCOLATE_VERSIONED, want.has) << want.first;
        EXPECT_EQ(traits.complevel, want.complevel) << want.first;
        EXPECT_EQ(Dialect::compatibility(traits), Dialect::Compatibility::versions) << want.first;

        EXPECT_EQ(traits.features & ~CHOCOLATE_VERSIONED, latest.features & ~CHOCOLATE_VERSIONED) << want.first;
        EXPECT_EQ(traits.changes.data(), latest.changes.data()) << want.first;
        EXPECT_EQ(traits.loads, latest.loads) << want.first;
        EXPECT_EQ(traits.netgame, latest.netgame) << want.first;
        EXPECT_EQ(traits.configName, latest.configName) << want.first;
        EXPECT_EQ(traits.line, PortLine::chocolate) << want.first;
    }

    EXPECT_EQ(Dialect::of(release("3.1.1", "chocolate-doom")).features, latest.features);
}

TEST(DialectVersions, ChocolateDoomBreaksFallBetweenTheReleasesEitherSide) {
    const auto at = [](const char *version) { return Dialect::of(release(version, "chocolate-doom")); };

    EXPECT_EQ(at("3.1.1").complevel, Complevels::gameVersion);
    EXPECT_EQ(at("3.1.0").complevel, Complevels::gameVersion12);

    EXPECT_TRUE(at("3.1.0").has(Feature::noAutoload));
    EXPECT_TRUE(at("3.1.0").has(Feature::freedoomFolders));
    EXPECT_FALSE(at("3.0.1").has(Feature::noAutoload));
    EXPECT_FALSE(at("3.0.1").has(Feature::freedoomFolders));
    EXPECT_EQ(at("3.0.1").complevel, Complevels::gameVersion1666);

    EXPECT_TRUE(at("3.0.0").has(Feature::saveFolder));
    EXPECT_FALSE(at("3.0.0").has(Feature::homeDotFolder));
    EXPECT_FALSE(at("2.3.0").has(Feature::saveFolder));
    EXPECT_TRUE(at("2.3.0").has(Feature::homeDotFolder));

    EXPECT_EQ(at("2.1.0").complevel, Complevels::gameVersion1666);
    EXPECT_EQ(at("2.0.0").complevel, Complevels::gameVersion19);
    EXPECT_EQ(at("1.7.0").complevel, Complevels::gameVersion19);
}

namespace {

constexpr Dialect::Features CRISPY_VERSIONED = Feature::coopSpawns | Feature::doubleAmmo | Feature::pistolStart
                                               | Feature::levelstat | Feature::freedoomFolders | Feature::noAutoload
                                               | Feature::flipWeapons | Feature::flipLevels | Feature::deathmatch3
                                               | Feature::saveFolder | Feature::homeDotFolder;

struct CrispyExpected {
    std::string_view first;

    // Which of the versioned features the range has.
    Dialect::Features has;

    Complevels complevel;

    // How the add-ons go on the line.
    std::string_view pwads;
};

constexpr Dialect::Features HAS_CRISPY_1_3 = Feature::flipLevels | Feature::homeDotFolder;
constexpr Dialect::Features HAS_CRISPY_4_0 = HAS_CRISPY_1_3 | Feature::saveFolder;
constexpr Dialect::Features HAS_CRISPY_4_1 = HAS_CRISPY_4_0 | Feature::deathmatch3;
constexpr Dialect::Features HAS_CRISPY_5_0 = Feature::flipLevels | Feature::saveFolder | Feature::deathmatch3;
constexpr Dialect::Features HAS_CRISPY_5_3 = HAS_CRISPY_5_0 | Feature::flipWeapons;
constexpr Dialect::Features HAS_CRISPY_5_4 = HAS_CRISPY_5_3 | Feature::noAutoload;
constexpr Dialect::Features HAS_CRISPY_5_9_0 = HAS_CRISPY_5_4 | Feature::levelstat | Feature::freedoomFolders;
constexpr Dialect::Features HAS_CRISPY_5_9_2 = HAS_CRISPY_5_9_0 | Feature::pistolStart;
constexpr Dialect::Features HAS_CRISPY_5_11 = HAS_CRISPY_5_9_2 | Feature::doubleAmmo;

constexpr std::array CRISPY_EXPECTED = std::to_array<CrispyExpected>({
        {"7.1", HAS_CRISPY_5_11 | Feature::coopSpawns, Complevels::gameVersion, "-file"},
        {"7.0", HAS_CRISPY_5_11 | Feature::coopSpawns, Complevels::gameVersion12, "-file"},
        {"5.11", HAS_CRISPY_5_11, Complevels::gameVersion12, "-file"},
        {"5.9.2", HAS_CRISPY_5_9_2, Complevels::gameVersion12, "-file"},
        {"5.9.0", HAS_CRISPY_5_9_0, Complevels::gameVersion12, "-file"},
        {"5.6.4", HAS_CRISPY_5_4, Complevels::gameVersion12, "-file"},
        {"5.4", HAS_CRISPY_5_4, Complevels::gameVersion1666, "-file"},
        {"5.3", HAS_CRISPY_5_3, Complevels::gameVersion1666, "-file"},
        {"5.0", HAS_CRISPY_5_0, Complevels::gameVersion1666, "-file"},
        {"4.1", HAS_CRISPY_4_1, Complevels::gameVersion1666, "-file"},
        {"4.0", HAS_CRISPY_4_0, Complevels::gameVersion1666, "-file"},
        {"1.5", HAS_CRISPY_1_3, Complevels::gameVersion1666, "-file"},
        {"1.3", HAS_CRISPY_1_3, Complevels::gameVersion1666, "-merge"},
        {"1.1", Feature::homeDotFolder, Complevels::gameVersion1666, "-merge"},
        {"1.0", Feature::homeDotFolder, Complevels::gameVersion19, "-merge"},
});

}

TEST(DialectVersions, EveryRangeOfCrispyDoomSpeaksItsRelease) {
    const Dialect::Traits latest = Dialect::of(release("", "crispy-doom"));
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", "crispy-doom"));

    ASSERT_EQ(ranges.size(), CRISPY_EXPECTED.size());

    for (size_t at = 0; at < ranges.size(); ++at) {
        const CrispyExpected &want = CRISPY_EXPECTED[at];
        const Dialect::Traits traits = Dialect::of(release(std::string(want.first), "crispy-doom"));

        EXPECT_EQ(ranges[at].first, want.first);
        EXPECT_EQ(traits.features & CRISPY_VERSIONED, want.has) << want.first;
        EXPECT_EQ(traits.complevel, want.complevel) << want.first;
        EXPECT_EQ(spelled(traits, Feature::pwads), want.pwads) << want.first;
        EXPECT_EQ(spelled(traits, Feature::complevel), "-gameversion") << want.first;

        EXPECT_EQ(traits.features & ~CRISPY_VERSIONED, latest.features & ~CRISPY_VERSIONED) << want.first;
        EXPECT_EQ(traits.loads, latest.loads) << want.first;
        EXPECT_EQ(traits.netgame, latest.netgame) << want.first;
        EXPECT_EQ(traits.configName, latest.configName) << want.first;
        EXPECT_EQ(traits.line, PortLine::crispy) << want.first;
    }

    EXPECT_EQ(Dialect::of(release("7.1", "crispy-doom")).features, latest.features);
}

// The flip switches look the same on screen at every release, so no row changes how they are described.
TEST(DialectVersions, EveryRangeOfCrispyDoomKeepsTheLatestChanges) {
    const Dialect::Traits latest = Dialect::of(release("", "crispy-doom"));

    for (const Dialect::VersionRange &range : Dialect::versions(release("", "crispy-doom"))) {
        const Dialect::Traits traits = Dialect::of(release(std::string(range.first), "crispy-doom"));

        for (const Dialect::Spelling &each : latest.changes) {
            EXPECT_EQ(Dialect::arg(traits, each.feature), each.arg) << range.first;
        }

        for (const Feature feature : {Feature::flipLevels, Feature::flipWeapons}) {
            EXPECT_EQ(Dialect::arg(traits, feature), Dialect::dialects::baseline(feature)) << range.first;
        }
    }
}

TEST(DialectVersions, CrispyDoomBreaksFallBetweenTheReleasesEitherSide) {
    const auto at = [](const char *version) { return Dialect::of(release(version, "crispy-doom")); };

    EXPECT_EQ(at("7.2").complevel, Complevels::gameVersion);
    EXPECT_EQ(at("7.1").complevel, Complevels::gameVersion);
    EXPECT_EQ(at("7.0").complevel, Complevels::gameVersion12);

    EXPECT_TRUE(at("7.0").has(Feature::coopSpawns));
    EXPECT_FALSE(at("6.0").has(Feature::coopSpawns));

    EXPECT_TRUE(at("5.11").has(Feature::doubleAmmo));
    EXPECT_TRUE(at("5.11.1").has(Feature::doubleAmmo));
    EXPECT_FALSE(at("5.10.3").has(Feature::doubleAmmo));

    EXPECT_TRUE(at("5.9.2").has(Feature::pistolStart));
    EXPECT_FALSE(at("5.9.1").has(Feature::pistolStart));

    EXPECT_TRUE(at("5.9.0").has(Feature::levelstat));
    EXPECT_TRUE(at("5.9.0").has(Feature::freedoomFolders));
    EXPECT_FALSE(at("5.8.0").has(Feature::levelstat));
    EXPECT_FALSE(at("5.8.0").has(Feature::freedoomFolders));

    EXPECT_EQ(at("5.6.4").complevel, Complevels::gameVersion12);
    EXPECT_EQ(at("5.6.3").complevel, Complevels::gameVersion1666);

    EXPECT_TRUE(at("5.4").has(Feature::noAutoload));
    EXPECT_FALSE(at("5.3").has(Feature::noAutoload));

    EXPECT_TRUE(at("5.3").has(Feature::flipWeapons));
    EXPECT_FALSE(at("5.2").has(Feature::flipWeapons));
    EXPECT_TRUE(at("5.2").has(Feature::flipLevels));

    EXPECT_FALSE(at("5.0").has(Feature::homeDotFolder));
    EXPECT_TRUE(at("4.3").has(Feature::homeDotFolder));
    EXPECT_EQ(at("4.3").complevel, Complevels::gameVersion1666);

    EXPECT_TRUE(at("4.1").has(Feature::deathmatch3));
    EXPECT_FALSE(at("4.0").has(Feature::deathmatch3));

    EXPECT_TRUE(at("4.0").has(Feature::saveFolder));
    EXPECT_FALSE(at("3.5").has(Feature::saveFolder));

    EXPECT_EQ(spelled(at("1.5"), Feature::pwads), "-file");
    EXPECT_EQ(spelled(at("1.4"), Feature::pwads), "-merge");

    EXPECT_TRUE(at("1.3").has(Feature::flipLevels));
    EXPECT_FALSE(at("1.2").has(Feature::flipLevels));

    EXPECT_EQ(at("1.1").complevel, Complevels::gameVersion1666);
    EXPECT_EQ(at("1.0").complevel, Complevels::gameVersion19);
}

TEST(DialectComplevels, EachGameVersionListHoldsTheListBeforeItAndItsOwn) {
    const auto words = [](const Complevels which) {
        std::vector<std::string> out;

        for (const int level : Dialect::complevels(which)) {
            out.push_back(Dialect::complevelWord(which, level));
        }

        return out;
    };

    const std::vector<std::string> newest = {
            "", "1.2", "1.5", "1.666", "1.7", "1.8", "1.9", "ultimate", "final", "final2", "hacx", "chex"};

    EXPECT_EQ(words(Complevels::gameVersion), newest);
    EXPECT_EQ(words(Complevels::gameVersion12),
              (std::vector<std::string>{
                      "", "1.2", "1.666", "1.7", "1.8", "1.9", "ultimate", "final", "final2", "hacx", "chex"}));
    EXPECT_EQ(words(Complevels::gameVersion1666),
              (std::vector<std::string>{
                      "", "1.666", "1.7", "1.8", "1.9", "ultimate", "final", "final2", "hacx", "chex"}));
    EXPECT_EQ(words(Complevels::gameVersion19),
              (std::vector<std::string>{"", "1.9", "ultimate", "final", "final2", "hacx", "chex"}));
}

TEST(DialectComplevels, AGameVersionKeepsItsNumberOnEveryList) {
    constexpr int V1_2 = Dialect::GAME_VERSIONS;
    constexpr int V1_5 = Dialect::GAME_VERSIONS + 1;
    constexpr int V1_666 = Dialect::GAME_VERSIONS + 2;
    constexpr int V1_9 = Dialect::GAME_VERSIONS + 5;

    for (const Complevels which :
         {Complevels::gameVersion, Complevels::gameVersion12, Complevels::gameVersion1666, Complevels::gameVersion19}) {
        EXPECT_EQ(Dialect::pickedLevel(which, V1_9), V1_9);
        EXPECT_EQ(Dialect::levelLabel(which, V1_9), "Doom 1.9 · 1.9");
    }

    EXPECT_EQ(Dialect::pickedLevel(Complevels::gameVersion, V1_5), V1_5);
    EXPECT_EQ(Dialect::pickedLevel(Complevels::gameVersion12, V1_5), -1);
    EXPECT_EQ(Dialect::levelLabel(Complevels::gameVersion12, V1_5), "");

    EXPECT_EQ(Dialect::pickedLevel(Complevels::gameVersion12, V1_2), V1_2);
    EXPECT_EQ(Dialect::pickedLevel(Complevels::gameVersion1666, V1_2), -1);

    EXPECT_EQ(Dialect::pickedLevel(Complevels::gameVersion1666, V1_666), V1_666);
    EXPECT_EQ(Dialect::pickedLevel(Complevels::gameVersion19, V1_666), -1);
}

TEST(DialectCompatibility, EveryGameVersionListOffersVersions) {
    Dialect::Traits traits{};

    traits.line = PortLine::none;

    for (const Complevels which :
         {Complevels::gameVersion19, Complevels::gameVersion1666, Complevels::gameVersion12, Complevels::gameVersion}) {
        traits.complevel = which;

        EXPECT_EQ(Dialect::compatibility(traits), Dialect::Compatibility::versions);
    }

    for (const Complevels which :
         {Complevels::woof, Complevels::woofToMbf21, Complevels::prboom, Complevels::dsda, Complevels::helion}) {
        traits.complevel = which;

        EXPECT_EQ(Dialect::compatibility(traits), Dialect::Compatibility::complevels);
    }

    traits.complevel = Complevels::none;

    EXPECT_EQ(Dialect::compatibility(traits), Dialect::Compatibility::none);
}

TEST(DialectVersions, ANumberAboveZeroComesAfterALetterInTheSamePlace) {
    static_assert(Dialect::later("2.6.1um", "2.6um"));
    static_assert(Dialect::later("2.6.2", "2.6.1um"));
    static_assert(Dialect::later("2.6.66", "2.6.2"));
    static_assert(Dialect::later("2.6.66", "2.6um"));
    static_assert(!Dialect::later("2.6um", "2.6.66"));
    static_assert(!Dialect::later("2.6um", "2.6.1um"));
    static_assert(Dialect::later("2.6um", "2.5.1.5"));
    static_assert(Dialect::later("1.2a", "1.2.0"));
    static_assert(!Dialect::later("1.2.0", "1.2a"));
}

TEST(DialectVersions, AZeroEqualsAMissingPartAndBothComeBeforeALetter) {
    static_assert(!Dialect::later("2.6", "2.6.0"));
    static_assert(!Dialect::later("2.6.0", "2.6"));
    static_assert(Dialect::later("2.6um", "2.6"));
    static_assert(Dialect::later("2.6um", "2.6.0"));
    static_assert(!Dialect::later("2.6.0", "2.6um"));
    static_assert(Dialect::later("0.98a", "0.98"));
    static_assert(!Dialect::later("4.12.0", "4.12"));
}

namespace {

// The keys of every release table, and strings that would break a weaker order.
std::vector<std::string_view> orderedKeys() {
    std::vector<std::string_view> out = {"2.6", "2.6.0", "2.5.1.5um", "4.12pre", "1a.5", "1au", "1a50", "0.98"};

    for (const Port::Known &known : Port::known()) {
        for (const Dialect::VersionRange &each : known.releases) {
            out.push_back(each.first);
        }
    }

    return out;
}

}

TEST(DialectVersions, LaterIsAStrictWeakOrderOverEveryRowKey) {
    const std::vector<std::string_view> keys = orderedKeys();
    const auto same = [](const std::string_view one, const std::string_view other) {
        return !Dialect::later(one, other) && !Dialect::later(other, one);
    };

    for (const std::string_view one : keys) {
        EXPECT_FALSE(Dialect::later(one, one)) << one;

        for (const std::string_view two : keys) {
            EXPECT_FALSE(Dialect::later(one, two) && Dialect::later(two, one)) << one << ' ' << two;

            for (const std::string_view three : keys) {
                if (Dialect::later(one, two) && Dialect::later(two, three)) {
                    EXPECT_TRUE(Dialect::later(one, three)) << one << ' ' << two << ' ' << three;
                }

                if (same(one, two) && same(two, three)) {
                    EXPECT_TRUE(same(one, three)) << one << ' ' << two << ' ' << three;
                }
            }
        }
    }
}

TEST(DialectVersionMap, APreBuildSortsAfterTheReleaseWithItsNumber) {
    EXPECT_EQ(rowFor("gzdoom", "4.12pre"), "4.11.0");
    EXPECT_EQ(rowFor("gzdoom", "4.11pre"), "4.11.0");
    EXPECT_EQ(rowFor("uzdoom", "5.0pre"), "5.0.0");
}

TEST(DialectVersionMap, APrboomPlusReleaseWithALetterFallsInItsRow) {
    EXPECT_EQ(rowFor("prboom-plus", "2.6um"), "2.6");
    EXPECT_EQ(rowFor("prboom-plus", "2.6.1um"), "2.6.1");
    EXPECT_EQ(rowFor("prboom-plus", "2.6.1"), "2.6.1");
    EXPECT_EQ(rowFor("prboom-plus", "2.6.2"), "2.6.1");
    EXPECT_EQ(rowFor("prboom-plus", "2.6.66"), "2.6.66");
    EXPECT_EQ(rowFor("prboom-plus", "2.6.70"), "2.6.66");
    EXPECT_EQ(rowFor("prboom-plus", "2.5.1.5"), "2.5.0.8");
    EXPECT_EQ(rowFor("prboom-plus", "2.5.1.4"), "2.5.0.8");
    EXPECT_EQ(rowFor("prboom-plus", "2.5.1.5um"), "2.5.0.8");
    EXPECT_EQ(rowFor("prboom-plus", "2.5.1.7um"), "2.5.0.8");
    EXPECT_EQ(rowFor("prboom-plus", "2.6"), "2.6");
    EXPECT_EQ(rowFor("prboom-plus", "2.6.0"), "2.6");
    EXPECT_EQ(rowFor("prboom-plus", "2.5.0.7"), "2.5.0.4");
    EXPECT_EQ(rowFor("prboom-plus", "2.5.0.3"), "2.4.8.2");
    EXPECT_EQ(rowFor("prboom-plus", "2.4.8.5"), "2.4.8.2");
    EXPECT_EQ(rowFor("prboom-plus", "2.4.8.1"), "2.4.6.1");
    EXPECT_EQ(rowFor("prboom-plus", "2.2.6.29"), "2.4.3.1");
    EXPECT_EQ(rowFor("glboom-plus", "2.5.1.5"), "2.5.0.8");
}

TEST(DialectVersions, PrboomPlusAndGlboomPlusListOneTableNewestFirst) {
    for (const char *file : {"prboom-plus", "glboom-plus"}) {
        const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", file));

        EXPECT_EQ(firsts(ranges),
                  (std::vector<std::string_view>{
                          "2.6.66", "2.6.1", "2.6", "2.5.0.8", "2.5.0.4", "2.4.8.2", "2.4.6.1", "2.4.3.1"}))
                << file;
        EXPECT_EQ(labels(ranges),
                  (std::vector<std::string_view>{"2.6.66 and later",
                                                 "2.6.1um to 2.6.2",
                                                 "2.6um",
                                                 "2.5.0.8 to 2.5.1.7",
                                                 "2.5.0.4 to 2.5.0.7",
                                                 "2.4.8.2 to 2.5.0.3",
                                                 "2.4.6.1 to 2.4.8.1",
                                                 "2.4.3.1 and earlier"}))
                << file;
        EXPECT_TRUE(Dialect::needsVersion(release("", file))) << file;
    }
}

namespace {

constexpr Dialect::Features PRBOOM_PLUS_VERSIONED = Feature::homeDotFolder | Feature::pistolStart | Feature::coopSpawns
                                                    | Feature::stroller | Feature::shotdir | Feature::noCheats
                                                    | Feature::continues | Feature::soloNet;

struct PrboomPlusExpected {
    std::string_view first;

    // Which of the versioned features the range has.
    Dialect::Features has;
};

constexpr Dialect::Features HAS_PRBOOM_PLUS_2_4_6_1 = Feature::homeDotFolder | Feature::soloNet;
constexpr Dialect::Features HAS_PRBOOM_PLUS_2_4_8_2 = HAS_PRBOOM_PLUS_2_4_6_1 | Feature::continues;
constexpr Dialect::Features HAS_PRBOOM_PLUS_2_5_0_4 = HAS_PRBOOM_PLUS_2_4_8_2 | Feature::noCheats;
constexpr Dialect::Features HAS_PRBOOM_PLUS_2_5_0_8 = HAS_PRBOOM_PLUS_2_5_0_4 | Feature::shotdir;
constexpr Dialect::Features HAS_PRBOOM_PLUS_2_6 = HAS_PRBOOM_PLUS_2_5_0_8 | Feature::stroller;
constexpr Dialect::Features HAS_PRBOOM_PLUS_2_6_1 = HAS_PRBOOM_PLUS_2_6 | Feature::pistolStart | Feature::coopSpawns;

constexpr std::array PRBOOM_PLUS_EXPECTED = std::to_array<PrboomPlusExpected>({
        {"2.6.66", PRBOOM_PLUS_VERSIONED & ~Dialect::Features(Feature::homeDotFolder)},
        {"2.6.1", HAS_PRBOOM_PLUS_2_6_1},
        {"2.6", HAS_PRBOOM_PLUS_2_6},
        {"2.5.0.8", HAS_PRBOOM_PLUS_2_5_0_8},
        {"2.5.0.4", HAS_PRBOOM_PLUS_2_5_0_4},
        {"2.4.8.2", HAS_PRBOOM_PLUS_2_4_8_2},
        {"2.4.6.1", HAS_PRBOOM_PLUS_2_4_6_1},
        {"2.4.3.1", Feature::homeDotFolder},
});

}

TEST(DialectVersions, EveryRangeOfPrboomPlusSpeaksItsRelease) {
    for (const char *file : {"prboom-plus", "glboom-plus"}) {
        const Dialect::Traits latest = Dialect::of(release("", file));
        const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", file));

        ASSERT_EQ(ranges.size(), PRBOOM_PLUS_EXPECTED.size()) << file;

        for (size_t at = 0; at < ranges.size(); ++at) {
            const PrboomPlusExpected &want = PRBOOM_PLUS_EXPECTED[at];
            const Dialect::Traits traits = Dialect::of(release(std::string(want.first), file));

            EXPECT_EQ(ranges[at].first, want.first) << file;
            EXPECT_EQ(traits.features & PRBOOM_PLUS_VERSIONED, want.has) << file << ' ' << want.first;

            EXPECT_EQ(traits.features & ~PRBOOM_PLUS_VERSIONED, latest.features & ~PRBOOM_PLUS_VERSIONED)
                    << file << ' ' << want.first;
            EXPECT_EQ(traits.changes.data(), latest.changes.data()) << file << ' ' << want.first;
            EXPECT_EQ(traits.loads, SaveNames::slot) << file << ' ' << want.first;
            EXPECT_EQ(traits.lastSlot, 255) << file << ' ' << want.first;
            EXPECT_EQ(traits.netgame, Netgames::prboom) << file << ' ' << want.first;
            EXPECT_EQ(traits.complevel, Complevels::prboom) << file << ' ' << want.first;
            EXPECT_EQ(traits.configName, latest.configName) << file << ' ' << want.first;
            EXPECT_EQ(traits.line, PortLine::prboom) << file << ' ' << want.first;
        }

        EXPECT_EQ(Dialect::of(release("2.6.66", file)).features, latest.features) << file;
    }
}

TEST(DialectVersions, PrboomPlusBreaksFallBetweenTheReleasesEitherSide) {
    const auto at = [](const char *version) { return Dialect::of(release(version, "prboom-plus")); };

    EXPECT_FALSE(at("2.6.66").has(Feature::homeDotFolder));
    EXPECT_TRUE(at("2.6.2").has(Feature::homeDotFolder));

    EXPECT_TRUE(at("2.6.1um").has(Feature::pistolStart));
    EXPECT_TRUE(at("2.6.1um").has(Feature::coopSpawns));
    EXPECT_FALSE(at("2.6um").has(Feature::pistolStart));
    EXPECT_FALSE(at("2.6um").has(Feature::coopSpawns));

    EXPECT_TRUE(at("2.6um").has(Feature::stroller));
    EXPECT_FALSE(at("2.5.1.5").has(Feature::stroller));

    EXPECT_TRUE(at("2.5.0.8").has(Feature::shotdir));
    EXPECT_FALSE(at("2.5.0.7").has(Feature::shotdir));

    EXPECT_TRUE(at("2.5.0.4").has(Feature::noCheats));
    EXPECT_FALSE(at("2.5.0.3").has(Feature::noCheats));

    EXPECT_TRUE(at("2.4.8.3").has(Feature::continues));
    EXPECT_TRUE(at("2.4.8.2").has(Feature::continues));
    EXPECT_FALSE(at("2.4.8.1").has(Feature::continues));
    EXPECT_FALSE(at("2.4.6.1").has(Feature::continues));

    EXPECT_TRUE(at("2.4.6.1").has(Feature::soloNet));
    EXPECT_FALSE(at("2.4.3.1").has(Feature::soloNet));
}

TEST(DialectOf, ThePrboomLineLoadsSlotsUpTo255) {
    for (const char *file : {"prboom-plus", "glboom-plus", "prboom", "glboom", "prboomx", "rude"}) {
        EXPECT_EQ(named(file).lastSlot, 255) << file;
    }
}

TEST(DialectOf, ThePrboomLineTakesDogsFromTheMbfComplevel) {
    for (const char *file : {"prboom-plus", "glboom-plus", "prboom", "glboom", "prboomx", "rude"}) {
        const Dialect::Arg &dogs = Dialect::arg(named(file), Feature::dogs);

        EXPECT_FALSE(Dialect::worksAt(dogs, Dialect::MBF - 1)) << file;
        EXPECT_TRUE(Dialect::worksAt(dogs, Dialect::MBF)) << file;
        EXPECT_TRUE(Dialect::worksAt(dogs, 17)) << file;
        EXPECT_TRUE(Dialect::worksAt(dogs, -1)) << file;
    }
}

TEST(DialectOf, ThePrboomLineTakesLongticsUpToTheBoomComplevel) {
    for (const char *file : {"prboom-plus", "glboom-plus", "prboom", "glboom", "prboomx", "rude"}) {
        const Dialect::Arg &longtics = Dialect::arg(named(file), Feature::longtics);

        EXPECT_EQ(longtics.name, "-longtics") << file;
        EXPECT_TRUE(Dialect::worksAt(longtics, 0)) << file;
        EXPECT_TRUE(Dialect::worksAt(longtics, 7)) << file;
        EXPECT_FALSE(Dialect::worksAt(longtics, 8)) << file;
        EXPECT_TRUE(Dialect::worksAt(longtics, -1)) << file;
    }
}

TEST(DialectOf, TheOriginalPrboomLacksWhatOnlyPrboomPlusReads) {
    const Dialect::Traits plus = named("prboom-plus");
    constexpr Dialect::Features ONLY_PLUS = Feature::levelstat | Feature::pistolStart | Feature::coopSpawns
                                            | Feature::stroller | Feature::noCheats | Feature::shotdir
                                            | Feature::continues;

    for (const char *file : {"prboom", "glboom"}) {
        const Dialect::Traits traits = named(file);

        EXPECT_EQ(traits.features & ONLY_PLUS, Dialect::Features()) << file;
        EXPECT_TRUE(traits.has(Feature::homeDotFolder)) << file;
        EXPECT_EQ(traits.features & ~(ONLY_PLUS | Feature::homeDotFolder), plus.features & ~ONLY_PLUS) << file;
        EXPECT_TRUE(traits.has(Feature::soloNet)) << file;
        EXPECT_TRUE(traits.has(Feature::longtics)) << file;
        EXPECT_EQ(traits.loads, SaveNames::slot) << file;
        EXPECT_EQ(traits.dialect, Dialect::Dialect::prboom) << file;
        EXPECT_TRUE(Dialect::versions(release("", file)).empty()) << file;
        EXPECT_TRUE(Dialect::sound(traits)) << file;
    }

    EXPECT_EQ(named("prboom").configName, "prboom");
    EXPECT_EQ(named("glboom").configName, "glboom");
    EXPECT_EQ(named("rude").features, plus.features);
}

TEST(DialectPorts, OnlyTheWholeNamePrboomOrGlboomRunsTheOriginal) {
    using Port::Port;

    EXPECT_EQ(named("prboom").port, Port::prboom);
    EXPECT_EQ(named("prboom.exe").port, Port::prboom);
    EXPECT_EQ(named("PrBoom.exe").port, Port::prboom);
    EXPECT_EQ(named("glboom").port, Port::glboom);
    EXPECT_EQ(named("prboom-plus").port, Port::prboomPlus);
    EXPECT_EQ(named("glboom-plus").port, Port::glboomPlus);
    EXPECT_EQ(named("prboom-plus-x64").port, Port::prboomPlus);
    EXPECT_EQ(named("prboom-plus").configName, "prboom");
    EXPECT_EQ(named("glboom-plus").configName, "glboom");

    for (const char *fork : {"prboomX", "prboom_plus", "glboomx"}) {
        const Dialect::Traits traits = named(fork);
        const bool gl = std::string_view(fork).starts_with("gl");
        const Dialect::Traits plus = named(gl ? "glboom-plus" : "prboom-plus");

        EXPECT_EQ(traits.port, gl ? Port::glboomFork : Port::prboomFork) << fork;
        EXPECT_EQ(traits.features, plus.features) << fork;
        EXPECT_EQ(traits.configName, plus.configName) << fork;
        EXPECT_EQ(Dialect::versions(release("", fork)).size(), 8U) << fork;
        EXPECT_FALSE(Dialect::of(release("2.6um", fork)).has(Feature::pistolStart)) << fork;
        EXPECT_EQ(Dialect::fromName(release("", fork)), gl ? Port::glboomPlus : Port::prboomPlus) << fork;
        EXPECT_EQ(traits.configName, gl ? "glboom" : "prboom") << fork;
    }
}

TEST(DialectVersions, ZdoomListsItsRangesNewestFirst) {
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", "zdoom"));

    EXPECT_EQ(firsts(ranges), (std::vector<std::string_view>{"2.8.0", "2.7.0", "2.6.0", "2.5.0"}));
    EXPECT_EQ(labels(ranges),
              (std::vector<std::string_view>{
                      "2.8.0 and later", "2.7.0 to 2.7.1", "2.6.0 to 2.6.1", "2.5.0 and earlier"}));
    EXPECT_TRUE(Dialect::needsVersion(release("", "zdoom")));
}

namespace {

constexpr Dialect::Features ZDOOM_VERSIONED =
        Feature::twoStateFreelook | Feature::programFolderOnly | Feature::homeDotFolder | Feature::compatflags2;

struct ZdoomExpected {
    std::string_view first;

    // Which of the versioned features the range has.
    Dialect::Features has;
};

constexpr std::array ZDOOM_EXPECTED = std::to_array<ZdoomExpected>({
        {"2.8.0", Feature::compatflags2},
        {"2.7.0", Feature::twoStateFreelook | Feature::programFolderOnly | Feature::compatflags2},
        {"2.6.0", ZDOOM_VERSIONED},
        {"2.5.0", Feature::twoStateFreelook | Feature::programFolderOnly | Feature::homeDotFolder},
});

}

TEST(DialectVersions, EveryRangeOfZdoomSpeaksItsRelease) {
    const Dialect::Traits latest = Dialect::of(release("", "zdoom"));
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", "zdoom"));

    ASSERT_EQ(ranges.size(), ZDOOM_EXPECTED.size());

    for (size_t at = 0; at < ranges.size(); ++at) {
        const ZdoomExpected &want = ZDOOM_EXPECTED[at];
        const Dialect::Traits traits = Dialect::of(release(std::string(want.first), "zdoom"));

        EXPECT_EQ(ranges[at].first, want.first);
        EXPECT_EQ(traits.features & ZDOOM_VERSIONED, want.has) << want.first;
        EXPECT_EQ(traits.features & ~ZDOOM_VERSIONED, latest.features & ~ZDOOM_VERSIONED) << want.first;
        EXPECT_EQ(traits.changes.data(), latest.changes.data()) << want.first;
        EXPECT_EQ(traits.loads, SaveNames::path) << want.first;
        EXPECT_EQ(traits.line, PortLine::zdoom) << want.first;
        EXPECT_EQ(traits.dialect, Dialect::Dialect::zdoom) << want.first;
    }

    EXPECT_EQ(Dialect::of(release("2.8.1", "zdoom")).features, latest.features);
}

TEST(DialectVersions, ZdoomBreaksFallBetweenTheReleasesEitherSide) {
    const auto at = [](const char *version) { return Dialect::of(release(version, "zdoom")); };

    EXPECT_FALSE(at("2.8.0").has(Feature::twoStateFreelook));
    EXPECT_FALSE(at("2.8.0").has(Feature::programFolderOnly));
    EXPECT_TRUE(at("2.7.1").has(Feature::twoStateFreelook));
    EXPECT_TRUE(at("2.7.1").has(Feature::programFolderOnly));

    EXPECT_FALSE(at("2.7.0").has(Feature::homeDotFolder));
    EXPECT_TRUE(at("2.6.1").has(Feature::homeDotFolder));

    EXPECT_TRUE(at("2.6.0").has(Feature::compatflags2));
    EXPECT_FALSE(at("2.5.0").has(Feature::compatflags2));
    EXPECT_FALSE(at("2.4.1").has(Feature::compatflags2));
}

TEST(DialectVersions, TheModeHintOfZdoomNamesOnlyTheWordsTheReleaseReads) {
    const std::string_view before = Dialect::levelHint(Dialect::of(release("2.5.0", "zdoom")));

    EXPECT_TRUE(before.contains("A mode fills compatflags,"));
    EXPECT_FALSE(before.contains("compatflags2"));
    EXPECT_TRUE(Dialect::levelHint(Dialect::of(release("2.6.0", "zdoom"))).contains("compatflags and compatflags2"));
}

TEST(DialectVersionMap, AZdoomFileVersionFallsInItsRange) {
    EXPECT_EQ(rowFor("zdoom", "2.8.1"), "2.8.0");
    EXPECT_EQ(rowFor("zdoom", "2.8.0"), "2.8.0");
    EXPECT_EQ(rowFor("zdoom", "2.7.1"), "2.7.0");
    EXPECT_EQ(rowFor("zdoom", "2.7.0 (r4281)"), "2.7.0");
    EXPECT_EQ(rowFor("zdoom", "2.6.1 (r3439)"), "2.6.0");
    EXPECT_EQ(rowFor("zdoom", "2.6.0"), "2.6.0");
    EXPECT_EQ(rowFor("zdoom", "2.5.0 (r2768)"), "2.5.0");
    EXPECT_EQ(rowFor("zdoom", "2.4.1"), "2.5.0");
}

TEST(DialectOf, ZandronumReadsDmflagsBits18And19ApartAndSkulltagDoesNot) {
    EXPECT_TRUE(named("zandronum").has(Feature::twoStateFreelook));
    EXPECT_FALSE(named("skulltag").has(Feature::twoStateFreelook));
    EXPECT_FALSE(named("zdoom").has(Feature::twoStateFreelook));
    EXPECT_FALSE(named("gzdoom").has(Feature::twoStateFreelook));
}

TEST(DialectVersions, ZandronumTakesTheColorsetCvarFrom3_1) {
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(release("", "zandronum"));

    EXPECT_EQ(firsts(ranges), (std::vector<std::string_view>{"3.1", "3.0"}));
    EXPECT_EQ(labels(ranges), (std::vector<std::string_view>{"3.1 and later", "3.0 to 3.0.1"}));

    for (const char *version : {"", "3.2.1", "3.2", "3.1"}) {
        EXPECT_TRUE(Dialect::of(release(version, "zandronum")).has(Feature::colorSet)) << version;
    }

    for (const char *version : {"3.0.1", "3.0", "2.1.2"}) {
        EXPECT_FALSE(Dialect::of(release(version, "zandronum")).has(Feature::colorSet)) << version;
    }

    EXPECT_EQ(rowFor("zandronum", "3.2.1"), "3.1");
    EXPECT_EQ(rowFor("zandronum", "3.0.1"), "3.0");
    EXPECT_EQ(rowFor("zandronum", "v3.2.1-r230709-1914"), "3.1");
    EXPECT_EQ(rowFor("zandronum", "v3.1-r211228-0959"), "3.1");
    EXPECT_EQ(rowFor("zandronum", "v3.0.1-r180415-1553"), "3.0");
    EXPECT_EQ(rowFor("zandronum", "v3.0-r171219-0216"), "3.0");
    EXPECT_TRUE(Dialect::needsVersion(release("", "zandronum")));

    const Dialect::Traits latest = Dialect::of(release("", "zandronum"));
    const Dialect::Traits oldest = Dialect::of(release("3.0", "zandronum"));

    EXPECT_EQ(oldest.features | Feature::colorSet, latest.features);
    EXPECT_EQ(oldest.changes.data(), latest.changes.data());
    EXPECT_EQ(oldest.loads, latest.loads);
    EXPECT_EQ(oldest.line, latest.line);
}
