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
#include "core/launch/Arguments.h"
#include "core/launch/Storage.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

using launch::Strings;

std::string iwad() {
    return tests::Corpus::iwad().string();
}

bool contains(const Strings &args, const Strings &run) {
    return !std::ranges::search(args, run).empty();
}

bool has(const Strings &args, const std::string &one) {
    return std::ranges::find(args, one) != args.end();
}

std::ptrdiff_t at(const Strings &args, const std::string &one) {
    return std::ranges::find(args, one) - args.begin();
}

class ArgumentsOf : public ::testing::Test {
protected:
    void SetUp() override { fs::remove_all(launch::profiles()); }

    static Config on(const char *program) { return launch::single(fs::path("/games") / program); }

    static Profile &profile(Config &config) { return config.activeProfile(); }

    static void files(Config &config, const Strings &names) {
        for (const std::string &name : names) {
            profile(config).files.push_back({.file = name});
        }
    }

    static const fs::path &episodic() {
        static const fs::path path =
                launch::wad(tests::Sandbox::root() / "launch" / "episodes.wad", "IWAD", {"E1M1", "E2M1", "E2M3"});

        return path;
    }
};

}

TEST_F(ArgumentsOf, ABareProfileIsHandedOnlyTheGame) {
    EXPECT_EQ(Arguments::of(on("gzdoom")), (Strings{"-iwad", iwad()}));
}

TEST_F(ArgumentsOf, AProfileWithNoGameIsHandedNothing) {
    Config config = on("gzdoom");

    profile(config).iwad.clear();

    EXPECT_TRUE(Arguments::of(config).empty());
}

TEST_F(ArgumentsOf, APortThatSearchesDoomwaddirIsHandedNoIwad) {
    EXPECT_FALSE(has(Arguments::of(on("doom2")), "-iwad"));
    EXPECT_FALSE(has(Arguments::of(on("doomlegacy")), "-iwad"));
}

TEST_F(ArgumentsOf, SkillIsPassedWhenSet) {
    Config config = on("gzdoom");

    profile(config).skill = 4;

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-skill", "4"}));
}

TEST_F(ArgumentsOf, MonstersOneIsNoMonsters) {
    Config config = on("gzdoom");

    profile(config).monsters = 1;

    EXPECT_TRUE(contains(Arguments::of(config), {"-nomonsters"}));
}

TEST_F(ArgumentsOf, MonstersTwoIsFast) {
    Config config = on("gzdoom");

    profile(config).monsters = 2;

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(has(args, "-fast"));
    EXPECT_FALSE(has(args, "-respawn"));
}

TEST_F(ArgumentsOf, MonstersThreeIsRespawn) {
    Config config = on("gzdoom");

    profile(config).monsters = 3;

    const Strings args = Arguments::of(config);

    EXPECT_FALSE(has(args, "-fast"));
    EXPECT_TRUE(has(args, "-respawn"));
}

TEST_F(ArgumentsOf, MonstersFourIsFastAndRespawn) {
    Config config = on("gzdoom");

    profile(config).monsters = 4;

    EXPECT_TRUE(contains(Arguments::of(config), {"-fast", "-respawn"}));
}

TEST_F(ArgumentsOf, APortThatCannotRespawnIsNotAskedTo) {
    Config config = on("helion");

    profile(config).monsters = 4;

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(has(args, "-fast"));
    EXPECT_FALSE(has(args, "-respawn"));
}

TEST_F(ArgumentsOf, AMapxxWarpOnAMapxxGameIsTheNumber) {
    Config config = on("gzdoom");

    profile(config).warp = "map07";

    EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "07"}));
}

TEST_F(ArgumentsOf, AnExmyWarpOnAnEpisodicGameIsTheEpisodeAndMap) {
    Config config = on("gzdoom");

    config.iwads.front().file = episodic().string();
    profile(config).warp = "E2M3";

    EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "2", "3"}));
}

TEST_F(ArgumentsOf, AWarpTheGameCannotNumberGoesToMapOnAZdoomPort) {
    Config config = on("gzdoom");

    profile(config).warp = "E2M3";

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(contains(args, {"+map", "E2M3"}));
    EXPECT_FALSE(has(args, "-warp"));
}

TEST_F(ArgumentsOf, AWarpTheGameCannotNumberIsDroppedOnAPortWithoutMap) {
    Config config = on("dsda-doom");

    profile(config).warp = "SECRET";

    const Strings args = Arguments::of(config);

    EXPECT_FALSE(has(args, "+map"));
    EXPECT_FALSE(has(args, "-warp"));
}

TEST_F(ArgumentsOf, AWarpWithNoGameGoesToMap) {
    Config config = on("gzdoom");

    profile(config).iwad.clear();
    profile(config).warp = "MAP01";

    EXPECT_EQ(Arguments::of(config), (Strings{"+map", "MAP01"}));
}

TEST_F(ArgumentsOf, OnlyEnabledFilesAreLoaded) {
    Config config = on("gzdoom");

    profile(config).files = {
            {.file = "/mods/a.wad"}, {.file = "/mods/b.wad", .enabled = false}, {.file = "/mods/c.pk3"}};

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-file", "/mods/a.wad", "/mods/c.pk3"}));
}

TEST_F(ArgumentsOf, AMissingFileIsPassedAsWritten) {
    Config config = on("gzdoom");

    files(config, {"nowhere.wad"});

    EXPECT_TRUE(contains(Arguments::of(config, tests::Sandbox::scratch("arguments-port")), {"-file", "nowhere.wad"}));
}

TEST_F(ArgumentsOf, ARelativeFileBesideThePortIsMadeWhole) {
    const fs::path port = tests::Sandbox::scratch("arguments-beside");

    launch::write(port / "mod.wad", "PWAD");

    Config config = on("gzdoom");

    files(config, {"mod.wad"});

    EXPECT_TRUE(contains(Arguments::of(config, port), {"-file", (port / "mod.wad").string()}));
}

TEST_F(ArgumentsOf, ARelativeGameBesideThePortIsMadeWhole) {
    const fs::path port = tests::Sandbox::scratch("arguments-game");

    launch::write(port / "freedoom.wad", "IWAD");

    Config config = on("gzdoom");

    config.iwads.front().file = "freedoom.wad";

    EXPECT_EQ(Arguments::of(config, port), (Strings{"-iwad", (port / "freedoom.wad").string()}));
}

TEST_F(ArgumentsOf, APatchListedAfterABexComesLast) {
    Config config = on("gzdoom");

    files(config, {"/mods/a.bex", "/mods/b.deh"});

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-bex", "/mods/a.bex", "-deh", "/mods/b.deh"}));
}

TEST_F(ArgumentsOf, ABexListedAfterAPatchComesLast) {
    Config config = on("gzdoom");

    files(config, {"/mods/b.deh", "/mods/a.bex"});

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-deh", "/mods/b.deh", "-bex", "/mods/a.bex"}));
}

TEST_F(ArgumentsOf, EachPatchSwitchAppearsOnceWithEveryFileBehindIt) {
    Config config = on("gzdoom");

    files(config, {"/mods/one.deh", "/mods/two.deh"});

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-deh", "/mods/one.deh", "/mods/two.deh"}));
}

TEST_F(ArgumentsOf, APortThatReadsBexThroughDehHasOneSwitchForBoth) {
    Config config = on("prboom-plus");

    files(config, {"/mods/b.deh", "/mods/a.bex"});

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-deh", "/mods/b.deh", "/mods/a.bex"}));
}

TEST_F(ArgumentsOf, DoomLegacyPatchesWithDehacked) {
    Config config = on("doomlegacy");

    files(config, {"/mods/a.deh"});

    EXPECT_EQ(Arguments::of(config), (Strings{"-dehacked", "/mods/a.deh"}));
}

TEST_F(ArgumentsOf, APortThatTakesNoPatchIsHandedNone) {
    Config config = on("doom2");

    files(config, {"/mods/a.deh", "/mods/b.bex", "/mods/c.wad"});

    EXPECT_EQ(Arguments::of(config), (Strings{"-file", "/mods/c.wad"}));
}

TEST_F(ArgumentsOf, PatchExtensionsAreReadWithoutCase) {
    Config config = on("gzdoom");

    files(config, {"/mods/A.DEH"});

    EXPECT_TRUE(contains(Arguments::of(config), {"-deh", "/mods/A.DEH"}));
}

TEST_F(ArgumentsOf, AConfigFileIsExecutedOnAZdoomPort) {
    Config config = on("gzdoom");

    files(config, {"/mods/binds.cfg", "/mods/keys.cfg"});

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "+exec", "/mods/binds.cfg", "+exec", "/mods/keys.cfg"}));
}

TEST_F(ArgumentsOf, AConfigFileIsDroppedOnAPortWithoutExec) {
    Config config = on("dsda-doom");

    files(config, {"/mods/binds.cfg"});

    EXPECT_FALSE(has(Arguments::of(config), "/mods/binds.cfg"));
}

TEST_F(ArgumentsOf, ADemoInTheFilesIsPlayed) {
    Config config = on("gzdoom");

    files(config, {"/mods/run.lmp"});

    EXPECT_TRUE(contains(Arguments::of(config), {"-playdemo", "/mods/run.lmp"}));
}

TEST_F(ArgumentsOf, FilesComeInTheirOwnOrderOfKinds) {
    Config config = on("gzdoom");

    files(config, {"/mods/run.lmp", "/mods/binds.cfg", "/mods/a.deh", "/mods/b.wad"});

    EXPECT_EQ(Arguments::of(config),
              (Strings{"-iwad",
                       iwad(),
                       "-file",
                       "/mods/b.wad",
                       "-deh",
                       "/mods/a.deh",
                       "+exec",
                       "/mods/binds.cfg",
                       "-playdemo",
                       "/mods/run.lmp"}));
}

TEST_F(ArgumentsOf, ProfileConfigsOffKeepsThePortsOwnSettings) {
    Config config = on("gzdoom");

    profile(config).config = "alpha.cfg";

    EXPECT_FALSE(has(Arguments::of(config), "-config"));
}

TEST_F(ArgumentsOf, AProfileConfigComesFirstWithItsSaveFolder) {
    Config config = on("gzdoom");

    config.general.profileConfigs = true;
    profile(config).config = "alpha.cfg";

    const fs::path own = launch::profiles() / "alpha";

    EXPECT_EQ(
            Arguments::of(config),
            (Strings{"-config", (own / "alpha.cfg").string(), "-savedir", (own / "saves").string(), "-iwad", iwad()}));
}

TEST_F(ArgumentsOf, ASharedConfigIsNotHandedOver) {
    Config config = on("gzdoom");

    config.general.profileConfigs = true;
    profile(config).config = "alpha.cfg";
    profile(config).sharedConfig = true;

    EXPECT_FALSE(has(Arguments::of(config), "-config"));
}

TEST_F(ArgumentsOf, ChocolateIsHandedItsSecondConfig) {
    Config config = on("chocolate-doom");

    config.general.profileConfigs = true;
    profile(config).config = "alpha.cfg";

    const fs::path own = launch::profiles() / "alpha";

    EXPECT_TRUE(contains(Arguments::of(config),
                         {"-config",
                          (own / "alpha.cfg").string(),
                          "-extraconfig",
                          (own / "alpha-extra.cfg").string(),
                          "-savedir",
                          (own / "saves").string()}));
}

TEST_F(ArgumentsOf, ABoomPortNamesItsSaveFolderWithSave) {
    Config config = on("prboom-plus");

    config.general.profileConfigs = true;
    profile(config).config = "alpha.cfg";

    EXPECT_TRUE(contains(Arguments::of(config), {"-save", (launch::profiles() / "alpha" / "saves").string()}));
}

TEST_F(ArgumentsOf, APortWithoutASaveSwitchGetsNoSaveFolder) {
    Config config = on("doomlegacy");

    config.general.profileConfigs = true;
    profile(config).config = "alpha.cfg";

    EXPECT_FALSE(has(Arguments::of(config), (launch::profiles() / "alpha" / "saves").string()));
}

TEST_F(ArgumentsOf, AnAbsoluteProfileConfigIsUsedWhereItIs) {
    Config config = on("gzdoom");

    config.general.profileConfigs = true;
    profile(config).config = "/configs/mine.cfg";

    EXPECT_TRUE(contains(Arguments::of(config), {"-config", "/configs/mine.cfg", "-savedir", "/configs/saves"}));
}

TEST_F(ArgumentsOf, AlwaysAddComesAfterTheGameAndExtraComesLast) {
    Config config = on("gzdoom");

    config.general.alwaysAdd = "-nomusic +vid_vsync 0";
    profile(config).extra = "-fast 'two words'";
    profile(config).skill = 2;

    EXPECT_EQ(Arguments::of(config),
              (Strings{"-iwad", iwad(), "-skill", "2", "-nomusic", "+vid_vsync", "0", "-fast", "two words"}));
}

TEST_F(ArgumentsOf, ExtraArgumentsKeepDoubleQuotedSpaces) {
    Config config = on("gzdoom");

    profile(config).extra = R"(+name "Doom Guy")";

    EXPECT_TRUE(contains(Arguments::of(config), {"+name", "Doom Guy"}));
}

TEST_F(ArgumentsOf, RecordingWritesIntoTheReplaysFolderWithoutAnExtension) {
    Config config = on("gzdoom");

    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Record, .file = "run1"};

    EXPECT_TRUE(
            contains(Arguments::of(config), {"-record", (launch::profiles() / "alpha" / "replays" / "run1").string()}));
}

TEST_F(ArgumentsOf, RecordingPutsTheDemoHeaderSwitchesFirst) {
    Config config = on("dsda-doom");

    profile(config).config = "alpha.cfg";
    profile(config).replay = {
            .mode = ReplayMode::Record, .file = "run1.lmp", .compatibility = 21, .longtics = true, .soloNet = true};

    EXPECT_TRUE(contains(Arguments::of(config),
                         {"-complevel",
                          "21",
                          "-longtics",
                          "-solo-net",
                          "-record",
                          (launch::profiles() / "alpha" / "replays" / "run1").string()}));
}

TEST_F(ArgumentsOf, AComplevelOutsideThePortsTableIsLeftOut) {
    Config config = on("woof");

    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Record, .file = "run1", .compatibility = 5};

    EXPECT_FALSE(has(Arguments::of(config), "-complevel"));
}

TEST_F(ArgumentsOf, AComplevelOfMinusOneIsTheDefaultAndNotPassed) {
    Config config = on("dsda-doom");

    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Record, .file = "run1", .compatibility = -1};

    EXPECT_FALSE(has(Arguments::of(config), "-complevel"));
}

TEST_F(ArgumentsOf, LongticsAreNotAskedOfAPortWithout) {
    Config config = on("gzdoom");

    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Record, .file = "run1", .longtics = true, .soloNet = true};

    const Strings args = Arguments::of(config);

    EXPECT_FALSE(has(args, "-longtics"));
    EXPECT_FALSE(has(args, "-solo-net"));
    EXPECT_TRUE(has(args, "-record"));
}

TEST_F(ArgumentsOf, PlayingBackNamesTheWholeFile) {
    Config config = on("gzdoom");

    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Play, .file = "run1"};

    EXPECT_TRUE(contains(Arguments::of(config),
                         {"-playdemo", (launch::profiles() / "alpha" / "replays" / "run1.lmp").string()}));
}

TEST_F(ArgumentsOf, TimedPlaybackUsesTimedemo) {
    Config config = on("gzdoom");

    profile(config).replay = {.mode = ReplayMode::Play, .file = "/demos/a.lmp", .playback = Playback::Timed};

    EXPECT_TRUE(contains(Arguments::of(config), {"-timedemo", "/demos/a.lmp"}));
}

TEST_F(ArgumentsOf, FastPlaybackFallsBackToPlaydemoOnAPortWithoutIt) {
    Config config = on("gzdoom");

    profile(config).replay = {.mode = ReplayMode::Play, .file = "/demos/a.lmp", .playback = Playback::Fast};

    EXPECT_TRUE(contains(Arguments::of(config), {"-playdemo", "/demos/a.lmp"}));
}

TEST_F(ArgumentsOf, FastPlaybackUsesFastdemoWhereThereIsOne) {
    Config config = on("dsda-doom");

    profile(config).replay = {.mode = ReplayMode::Play, .file = "/demos/a.lmp", .playback = Playback::Fast};

    EXPECT_TRUE(contains(Arguments::of(config), {"-fastdemo", "/demos/a.lmp"}));
}

TEST_F(ArgumentsOf, TheProfilesDemoReplacesOnesInTheFiles) {
    Config config = on("gzdoom");

    files(config, {"/mods/other.lmp"});
    profile(config).replay = {.mode = ReplayMode::Play, .file = "/demos/a.lmp"};

    const Strings args = Arguments::of(config);

    EXPECT_FALSE(has(args, "/mods/other.lmp"));
    EXPECT_EQ(std::ranges::count(args, "-playdemo"), 1);
}

TEST_F(ArgumentsOf, LevelstatIsAskedOnlyOfAPortThatWritesIt) {
    Config dsda = on("dsda-doom");
    Config gzdoom = on("gzdoom");

    profile(dsda).levelstat = true;
    profile(gzdoom).levelstat = true;

    EXPECT_TRUE(has(Arguments::of(dsda), "-levelstat"));
    EXPECT_FALSE(has(Arguments::of(gzdoom), "-levelstat"));
}

TEST_F(ArgumentsOf, AZdoomPortLoadsASaveByName) {
    Config config = on("gzdoom");

    config.general.profileConfigs = true;
    profile(config).config = "alpha.cfg";
    profile(config).save = {.enabled = true, .file = "save3.zds"};

    EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "save3.zds"}));
}

TEST_F(ArgumentsOf, ZdoomLoadsASaveByPath) {
    Config config = on("zdoom");

    config.general.profileConfigs = true;
    profile(config).config = "alpha.cfg";
    profile(config).save = {.enabled = true, .file = "save3.zds"};

    EXPECT_TRUE(contains(Arguments::of(config),
                         {"-loadgame", (launch::profiles() / "alpha" / "saves" / "save3.zds").string()}));
}

TEST_F(ArgumentsOf, ABoomPortLoadsASaveBySlot) {
    Config config = on("prboom-plus");

    config.general.profileConfigs = true;
    profile(config).config = "alpha.cfg";
    profile(config).save = {.enabled = true, .file = "prbmsav4.dsg"};

    EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "4"}));
}

TEST_F(ArgumentsOf, ASaveWithoutASlotNumberIsNotLoadedBySlot) {
    Config config = on("prboom-plus");

    profile(config).save = {.enabled = true, .file = "/saves/quick.dsg"};

    EXPECT_FALSE(has(Arguments::of(config), "-loadgame"));
}

TEST_F(ArgumentsOf, DsdaIsHandedNoSave) {
    Config config = on("dsda-doom");

    profile(config).save = {.enabled = true, .file = "/saves/dsdasav1.dsg"};

    EXPECT_FALSE(has(Arguments::of(config), "-loadgame"));
}

TEST_F(ArgumentsOf, ADisabledSaveIsNotLoaded) {
    Config config = on("gzdoom");

    profile(config).save = {.enabled = false, .file = "/saves/save3.zds"};

    EXPECT_FALSE(has(Arguments::of(config), "-loadgame"));
}

TEST_F(ArgumentsOf, ADemoTakesThePlaceOfTheSave) {
    Config config = on("gzdoom");

    profile(config).save = {.enabled = true, .file = "/saves/save3.zds"};
    profile(config).replay = {.mode = ReplayMode::Play, .file = "/demos/a.lmp"};

    EXPECT_FALSE(has(Arguments::of(config), "-loadgame"));
}

TEST_F(ArgumentsOf, TheNetgameFollowsTheSaveAndPrecedesAlwaysAdd) {
    Config config = on("gzdoom");

    config.general.alwaysAdd = "-nomusic";
    profile(config).save = {.enabled = true, .file = "/saves/save3.zds"};
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.players = 2;
    profile(config).multiplayer.savegame = "net.zds";

    const Strings args = Arguments::of(config);

    EXPECT_LT(at(args, "-loadgame"), at(args, "-host"));
    EXPECT_LT(at(args, "-host"), at(args, "-nomusic"));
    EXPECT_EQ(std::ranges::count(args, "-loadgame"), 1) << "only the first -loadgame is read";
}

TEST_F(ArgumentsOf, MapsGathersTheGameAndEnabledFilesInNaturalOrder) {
    Config config = on("gzdoom");

    config.iwads.front().file = episodic().string();

    const fs::path scratch = tests::Sandbox::scratch("arguments-maps");
    const fs::path extra = launch::wad(scratch / "extra.wad", "PWAD", {"MAP10", "MAP02", "E1M1"});
    const fs::path hidden = launch::wad(scratch / "hidden.wad", "PWAD", {"MAP99"});

    profile(config).files = {{.file = extra.string()}, {.file = hidden.string(), .enabled = false}};

    EXPECT_EQ(Arguments::maps(config), (Strings{"E1M1", "E2M1", "E2M3", "MAP02", "MAP10"}));
}

TEST_F(ArgumentsOf, MapsOfAProfileWithNothingIsEmpty) {
    Config config = on("gzdoom");

    profile(config).iwad.clear();

    EXPECT_TRUE(Arguments::maps(config).empty());
}
