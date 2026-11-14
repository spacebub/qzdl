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
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Launch.h"
#include "core/launch/Arguments.h"
#include "core/launch/Dialect.h"
#include "core/launch/Flags.h"
#include "core/launch/Rules.h"
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

    static std::string saved(const std::string &name) {
        const fs::path path = tests::Sandbox::root() / "launch" / "saves" / name;

        launch::write(path, "save");

        return path.string();
    }

    static void savedInProfile(const std::string &name) {
        launch::write(launch::profiles() / "alpha" / "saves" / name, "save");
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

    files(config, {episodic().string()});
    profile(config).warp = "E2M3";

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(contains(args, {"+map", "E2M3"}));
    EXPECT_FALSE(has(args, "-warp"));
}

TEST_F(ArgumentsOf, AWarpNoLoadedFileHoldsIsDropped) {
    Config config = on("chocolate-doom");

    config.iwads.front().file = episodic().string();
    files(config, {launch::wad(tests::Sandbox::root() / "launch" / "sigil.wad", "PWAD", {"E5M1"}).string()});
    profile(config).files.front().enabled = false;
    profile(config).warp = "E5M1";

    EXPECT_FALSE(has(Arguments::of(config), "-warp"));

    profile(config).files.front().enabled = true;

    EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "5", "1"}));
}

TEST_F(ArgumentsOf, AWarpIsTrustedWhenNotEveryFileCouldBeAsked) {
    Config config = on("gzdoom");

    files(config, {episodic().string()});
    profile(config).iwad.clear();
    profile(config).warp = "MAP05";

    EXPECT_TRUE(contains(Arguments::of(config), {"+map", "MAP05"})) << "no game to ask";

    profile(config) = launch::single("/games/gzdoom").activeProfile();
    files(config, {"/mods/gone.wad"});
    profile(config).warp = "E5M1";

    EXPECT_TRUE(contains(Arguments::of(config), {"+map", "E5M1"})) << "an add-on that cannot be read";

    profile(config).files.clear();
    profile(config).extra = "-file /mods/typed.wad";

    EXPECT_TRUE(contains(Arguments::of(config), {"+map", "E5M1"})) << "files typed as arguments";
}

TEST_F(ArgumentsOf, AWarpIsMatchedWithoutCase) {
    Config config = on("chocolate-doom");

    config.iwads.front().file = episodic().string();
    profile(config).warp = "e2m3";

    EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "2", "3"}));
}

TEST_F(ArgumentsOf, HereticAndStrifePatchesGoToDeh) {
    Config config = on("crispy-heretic");

    files(config, {"/mods/a.hhe", "/mods/b.SEH"});

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-deh", "/mods/a.hhe", "/mods/b.SEH"}));
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
                          (own / "chocolate.cfg").string(),
                          "-extraconfig",
                          (own / "chocolate-extra.cfg").string(),
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

TEST_F(ArgumentsOf, APortWithoutAConfigSwitchIsHandedNoConfig) {
    Config config = on("doomlegacy");

    config.general.profileConfigs = true;
    profile(config).config = "alpha.cfg";

    EXPECT_FALSE(has(Arguments::of(config), "-config"));
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
    profile(config).complevel = 21;
    profile(config).replay = {.mode = ReplayMode::Record, .file = "run1.lmp", .longtics = true, .soloNet = true};

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
    profile(config).complevel = 5;
    profile(config).replay = {.mode = ReplayMode::Record, .file = "run1"};

    EXPECT_FALSE(has(Arguments::of(config), "-complevel"));
}

TEST_F(ArgumentsOf, AComplevelOfMinusOneIsTheDefaultAndNotPassed) {
    Config config = on("dsda-doom");

    profile(config).config = "alpha.cfg";
    profile(config).complevel = -1;
    profile(config).replay = {.mode = ReplayMode::Record, .file = "run1"};

    EXPECT_FALSE(has(Arguments::of(config), "-complevel"));
}

TEST_F(ArgumentsOf, TheComplevelGoesOnEveryLaunch) {
    Config config = on("dsda-doom");

    profile(config).complevel = 9;

    EXPECT_TRUE(contains(Arguments::of(config), {"-complevel", "9"}));
}

TEST_F(ArgumentsOf, ADemoPlayedBackKeepsItsOwnComplevel) {
    Config config = on("dsda-doom");

    profile(config).config = "alpha.cfg";
    profile(config).complevel = 9;
    profile(config).replay = {.mode = ReplayMode::Play, .file = "run1"};
    launch::write(launch::profiles() / "alpha" / "replays" / "run1.lmp", "x");

    EXPECT_FALSE(has(Arguments::of(config), "-complevel"));

    profile(config).replay = {};
    files(config, {"/mods/a.lmp"});

    EXPECT_FALSE(has(Arguments::of(config), "-complevel")) << "a demo among the add-ons";
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
    savedInProfile("save3.zds");

    EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "save3.zds"}));
}

TEST_F(ArgumentsOf, ZdoomLoadsASaveByPath) {
    Config config = on("zdoom");

    config.general.profileConfigs = true;
    profile(config).config = "alpha.cfg";
    profile(config).save = {.enabled = true, .file = "save3.zds"};
    savedInProfile("save3.zds");

    EXPECT_TRUE(contains(Arguments::of(config),
                         {"-loadgame", (launch::profiles() / "alpha" / "saves" / "save3.zds").string()}));
}

TEST_F(ArgumentsOf, ABoomPortLoadsASaveBySlot) {
    Config config = on("prboom-plus");

    config.general.profileConfigs = true;
    profile(config).config = "alpha.cfg";
    profile(config).save = {.enabled = true, .file = "prbmsav4.dsg"};
    savedInProfile("prbmsav4.dsg");

    EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "4"}));
}

TEST_F(ArgumentsOf, ASaveWithoutASlotNumberIsNotLoadedBySlot) {
    Config config = on("prboom-plus");

    profile(config).save = {.enabled = true, .file = saved("quick.dsg")};

    EXPECT_FALSE(has(Arguments::of(config), "-loadgame"));
}

TEST_F(ArgumentsOf, DsdaLoadsASaveBySlotUpTo118) {
    Config config = on("dsda-doom");

    profile(config).save = {.enabled = true, .file = saved("dsda-doom-savegame118.dsg")};

    EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "118"}));

    profile(config).save.file = saved("dsda-doom-savegame119.dsg");

    EXPECT_FALSE(has(Arguments::of(config), "-loadgame"));
}

TEST_F(ArgumentsOf, NyanDoomLoadsBySlotLikeDsda) {
    Config config = on("nyan-doom");

    profile(config).save = {.enabled = true, .file = saved("nyan-doom-savegame3.dsg")};

    EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "3"}));
}

TEST_F(ArgumentsOf, DsdaRecordingLoadsNoSave) {
    Config config = on("dsda-doom");

    profile(config).config = "alpha.cfg";
    profile(config).save = {.enabled = true, .file = saved("dsda-doom-savegame1.dsg")};
    profile(config).replay = {.mode = ReplayMode::Record, .file = "run1"};

    EXPECT_FALSE(has(Arguments::of(config), "-loadgame"));
}

TEST_F(ArgumentsOf, ADisabledSaveIsNotLoaded) {
    Config config = on("gzdoom");

    profile(config).save = {.enabled = false, .file = saved("save3.zds")};

    EXPECT_FALSE(has(Arguments::of(config), "-loadgame"));
}

TEST_F(ArgumentsOf, AMissingSaveInTheProfileFolderIsNotLoaded) {
    Config config = on("gzdoom");

    config.general.profileConfigs = true;
    profile(config).config = "alpha.cfg";
    profile(config).save = {.enabled = true, .file = "save3.zds"};

    EXPECT_FALSE(has(Arguments::of(config), "-loadgame"));
}

TEST_F(ArgumentsOf, AMissingAbsoluteSaveIsNotLoaded) {
    Config config = on("gzdoom");

    profile(config).save = {.enabled = true, .file = (tests::Sandbox::root() / "launch" / "gone.zds").string()};

    EXPECT_FALSE(has(Arguments::of(config), "-loadgame"));
}

TEST_F(ArgumentsOf, AMissingSaveBySlotIsNotLoaded) {
    Config config = on("prboom-plus");

    config.general.profileConfigs = true;
    profile(config).config = "alpha.cfg";
    profile(config).save = {.enabled = true, .file = "prbmsav4.dsg"};

    EXPECT_FALSE(has(Arguments::of(config), "-loadgame"));
}

TEST_F(ArgumentsOf, AMissingSaveLeavesTheNetgameSaveToLoad) {
    Config config = on("uzdoom");

    profile(config).save = {.enabled = true, .file = (tests::Sandbox::root() / "launch" / "gone.zds").string()};
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.players = 2;
    profile(config).multiplayer.savegame = "net.zds";

    EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "net.zds"}));
}

TEST_F(ArgumentsOf, ADemoTakesThePlaceOfTheSave) {
    Config config = on("gzdoom");

    profile(config).save = {.enabled = true, .file = saved("save3.zds")};
    profile(config).replay = {.mode = ReplayMode::Play, .file = "/demos/a.lmp"};

    EXPECT_FALSE(has(Arguments::of(config), "-loadgame"));
}

TEST_F(ArgumentsOf, TheNetgameFollowsTheSaveAndPrecedesAlwaysAdd) {
    Config config = on("gzdoom");

    config.general.alwaysAdd = "-nomusic";
    profile(config).save = {.enabled = true, .file = saved("save3.zds")};
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.players = 2;
    profile(config).multiplayer.savegame = "net.zds";

    const Strings args = Arguments::of(config);

    EXPECT_LT(at(args, "-loadgame"), at(args, "-host"));
    EXPECT_LT(at(args, "-host"), at(args, "-nomusic"));
    EXPECT_EQ(std::ranges::count(args, "-loadgame"), 1) << "only the first -loadgame is read";
}

TEST_F(ArgumentsOf, UzdoomHostingLoadsTheNetgameSaveInPlaceOfTheProfileSave) {
    Config config = on("uzdoom");

    profile(config).save = {.enabled = true, .file = saved("save3.zds")};
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.players = 2;
    profile(config).multiplayer.savegame = "net.zds";

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(contains(args, {"-loadgame", "net.zds"}));
    EXPECT_EQ(std::ranges::count(args, "-loadgame"), 1);
}

TEST_F(ArgumentsOf, UzdoomJoiningLoadsNoProfileSave) {
    Config config = on("uzdoom");

    profile(config).save = {.enabled = true, .file = saved("save3.zds")};
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.host = "10.0.0.9";

    EXPECT_FALSE(has(Arguments::of(config), "-loadgame"));
}

TEST_F(ArgumentsOf, UzdoomHostingForOneLoadsTheProfileSave) {
    Config config = on("uzdoom");

    profile(config).save = {.enabled = true, .file = saved("save3.zds")};
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.players = 1;

    EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "save3.zds"}));
}

TEST_F(ArgumentsOf, UzdoomJoiningNowhereLoadsTheProfileSave) {
    Config config = on("uzdoom");

    profile(config).save = {.enabled = true, .file = saved("save3.zds")};
    profile(config).multiplayer.gameType = GameType::Coop;

    EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "save3.zds"}));
}

TEST_F(ArgumentsOf, GzdoomHostingLoadsTheProfileSave) {
    Config config = on("gzdoom");

    profile(config).save = {.enabled = true, .file = saved("save3.zds")};
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.players = 2;
    profile(config).multiplayer.savegame = "net.zds";

    EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "save3.zds"}));
}

TEST_F(ArgumentsOf, UzdoomLoadsTheProfileSaveWhenPlayingAlone) {
    Config config = on("uzdoom");

    profile(config).save = {.enabled = true, .file = saved("save3.zds")};

    EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "save3.zds"}));
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

TEST_F(ArgumentsOf, PistolStartIsACvarForZdoomAndASwitchElsewhere) {
    Config config = on("gzdoom");

    profile(config).pistolStart = true;

    EXPECT_TRUE(contains(Arguments::of(config), {"+sv_pistolstart", "1"}));

    config.ports.front().file = "/games/woof";

    EXPECT_TRUE(has(Arguments::of(config), "-pistolstart"));

    config.ports.front().file = "/games/zdoom";

    EXPECT_FALSE(has(Arguments::of(config), "+sv_pistolstart"));
    EXPECT_FALSE(has(Arguments::of(config), "-pistolstart"));

    config.ports.front().file = "/games/chocolate-doom";

    EXPECT_FALSE(has(Arguments::of(config), "-pistolstart"));
}

TEST_F(ArgumentsOf, ThePistolStartSwitchStaysOutOfDemosAndNetgames) {
    Config config = on("crispy-doom");

    profile(config).config = "alpha.cfg";
    profile(config).pistolStart = true;

    ASSERT_TRUE(has(Arguments::of(config), "-pistolstart"));

    profile(config).replay = {.mode = ReplayMode::Record, .file = "run1"};

    EXPECT_FALSE(has(Arguments::of(config), "-pistolstart"));

    profile(config).replay = {};
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.players = 2;

    EXPECT_FALSE(has(Arguments::of(config), "-pistolstart"));
}

TEST_F(ArgumentsOf, SkipAutoloadUsesTheSwitchOfEachPort) {
    Config config = on("gzdoom");

    EXPECT_FALSE(has(Arguments::of(config), "-noautoload"));

    profile(config).skipAutoload = true;

    EXPECT_TRUE(has(Arguments::of(config), "-noautoload"));

    config.ports.front().file = "/games/prboom-plus";

    EXPECT_TRUE(has(Arguments::of(config), "-noload"));
    EXPECT_FALSE(has(Arguments::of(config), "-noautoload"));

    config.ports.front().file = "/games/eternity";

    EXPECT_TRUE(has(Arguments::of(config), "-noload"));
    EXPECT_FALSE(has(Arguments::of(config), "-noautoload"));

    config.ports.front().file = "/games/helion";

    EXPECT_FALSE(has(Arguments::of(config), "-noautoload"));
    EXPECT_FALSE(has(Arguments::of(config), "-noload"));
}

TEST_F(ArgumentsOf, MusicAndSoundEffectsOffPassTheirSwitches) {
    Config config = on("gzdoom");

    EXPECT_FALSE(has(Arguments::of(config), "-nomusic"));
    EXPECT_FALSE(has(Arguments::of(config), "-nosfx"));

    profile(config).music = false;
    profile(config).soundEffects = false;

    EXPECT_TRUE(has(Arguments::of(config), "-nomusic"));
    EXPECT_TRUE(has(Arguments::of(config), "-nosfx"));

    config.ports.front().file = "/games/helion";

    EXPECT_TRUE(has(Arguments::of(config), "-nomusic"));
    EXPECT_FALSE(has(Arguments::of(config), "-nosfx"));
}

TEST_F(ArgumentsOf, DisplayIsPassedTheWayEachPortTakesIt) {
    Config config = on("gzdoom");

    profile(config).display = Display::Window;

    EXPECT_TRUE(contains(Arguments::of(config), {"+vid_fullscreen", "0"}));

    profile(config).display = Display::Fullscreen;
    config.ports.front().file = "/games/zandronum";

    EXPECT_TRUE(contains(Arguments::of(config), {"+fullscreen", "1"}));

    config.ports.front().file = "/games/prboom-plus";

    // -fullscreen would be saved into its config, and -nowindow is not.
    EXPECT_TRUE(has(Arguments::of(config), "-nowindow"));
    EXPECT_FALSE(has(Arguments::of(config), "-fullscreen"));

    profile(config).display = Display::Window;

    EXPECT_TRUE(has(Arguments::of(config), "-window"));
    EXPECT_FALSE(has(Arguments::of(config), "-nofullscreen"));

    profile(config).display = Display::Fullscreen;

    config.ports.front().file = "/games/dsda-doom";

    EXPECT_TRUE(has(Arguments::of(config), "-fullscreen"));

    profile(config).display = Display::Window;
    config.ports.front().file = "/games/woof";

    EXPECT_TRUE(has(Arguments::of(config), "-window"));

    config.ports.front().file = "/games/eternity";

    EXPECT_FALSE(has(Arguments::of(config), "-window"));

    profile(config).display = Display::Port;
    config.ports.front().file = "/games/gzdoom";

    EXPECT_FALSE(has(Arguments::of(config), "+vid_fullscreen"));
}

TEST_F(ArgumentsOf, CheatsAreACvarOnlyTheZdoomFamilyTakes) {
    Config config = on("zandronum");

    profile(config).cheats = true;

    EXPECT_TRUE(contains(Arguments::of(config), {"+sv_cheats", "1"}));

    config.ports.front().file = "/games/dsda-doom";

    EXPECT_FALSE(has(Arguments::of(config), "+sv_cheats"));
}

TEST_F(ArgumentsOf, ScreenshotsGoWhereTheProfileSays) {
    Config config = on("dsda-doom");

    profile(config).config = "alpha.cfg";

    EXPECT_FALSE(has(Arguments::of(config), "-shotdir"));

    profile(config).screenshots = Screenshots::Profile;

    EXPECT_TRUE(contains(Arguments::of(config), {"-shotdir", (launch::profiles() / "alpha" / "screenshots").string()}));

    profile(config).screenshots = Screenshots::Custom;

    EXPECT_FALSE(has(Arguments::of(config), "-shotdir"));

    profile(config).screenshotFolder = "/shots";

    EXPECT_TRUE(contains(Arguments::of(config), {"-shotdir", "/shots"}));

    config.ports.front().file = "/games/helion";

    EXPECT_FALSE(has(Arguments::of(config), "-shotdir"));
}

TEST_F(ArgumentsOf, ContinuePlaysTheSourceAndRecordsIntoTheName) {
    Config config = on("prboom-plus");

    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Continue, .file = "run2", .source = "run1"};

    const fs::path replays = launch::profiles() / "alpha" / "replays";

    launch::write(replays / "run1.lmp", "x");

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(contains(args, {"-recordfromto", (replays / "run1.lmp").string(), (replays / "run2").string()}));
    EXPECT_FALSE(has(args, "-complevel"));
    EXPECT_FALSE(has(args, "-record"));
}

TEST_F(ArgumentsOf, ContinueHandsAnyComplevelToAPortThatInsists) {
    Config config = on("woof");

    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Continue, .file = "run2", .source = "run1"};

    EXPECT_TRUE(contains(Arguments::of(config), {"-complevel", "2", "-recordfromto"}));

    profile(config).complevel = 21;

    EXPECT_TRUE(contains(Arguments::of(config), {"-complevel", "21", "-recordfromto"}));
}

TEST_F(ArgumentsOf, ContinueAsksForLongticsWhenTheSourceHasThem) {
    Config config = on("dsda-doom");

    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Continue, .file = "run2", .source = "long"};

    launch::write(launch::profiles() / "alpha" / "replays" / "long.lmp", std::string(1, '\x6f'));

    EXPECT_TRUE(has(Arguments::of(config), "-longtics"));

    launch::write(launch::profiles() / "alpha" / "replays" / "long.lmp", std::string(1, '\x6d'));

    EXPECT_FALSE(has(Arguments::of(config), "-longtics"));
}

TEST_F(ArgumentsOf, APortWithoutContinueIsHandedNoDemo) {
    Config config = on("gzdoom");

    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Continue, .file = "run2", .source = "run1"};

    const Strings args = Arguments::of(config);

    EXPECT_FALSE(has(args, "-recordfromto"));
    EXPECT_FALSE(has(args, "-record"));
    EXPECT_FALSE(has(args, "-playdemo"));
}

TEST_F(ArgumentsOf, PistolStartStaysWhenNoNetgameLaunches) {
    Config config = on("dsda-doom");

    profile(config).pistolStart = true;
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.players = 2;

    EXPECT_TRUE(has(Arguments::of(config), "-pistolstart")) << "DSDA-Doom has no netgame to leave it out of";

    config.ports.front().file = "/games/woof";
    profile(config).multiplayer.players = 0;

    EXPECT_TRUE(has(Arguments::of(config), "-pistolstart")) << "a join with no address launches alone";
}

TEST_F(ArgumentsOf, PistolStartStaysOutOfAnAddOnDemo) {
    Config config = on("crispy-doom");

    profile(config).pistolStart = true;
    files(config, {"/demos/other.lmp"});

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(has(args, "-playdemo"));
    EXPECT_FALSE(has(args, "-pistolstart"));
}

TEST_F(ArgumentsOf, ContinueWithoutASourceIsNoDemo) {
    Config config = on("woof");

    profile(config).config = "alpha.cfg";
    profile(config).pistolStart = true;
    profile(config).replay = {.mode = ReplayMode::Continue, .file = "next"};

    const Strings args = Arguments::of(config);

    EXPECT_FALSE(has(args, "-recordfromto"));
    EXPECT_TRUE(has(args, "-pistolstart"));
}

TEST_F(ArgumentsOf, ARelativeScreenshotFolderIsInTheProfileFolder) {
    Config config = on("dsda-doom");

    profile(config).config = "alpha.cfg";
    profile(config).screenshots = Screenshots::Custom;
    profile(config).screenshotFolder = "shots";

    EXPECT_TRUE(contains(Arguments::of(config), {"-shotdir", (launch::profiles() / "alpha" / "shots").string()}));
}

TEST_F(ArgumentsOf, AWarpIsTrustedWhenAnAddOnIsDamaged) {
    Config config = on("gzdoom");

    config.iwads.front().file = episodic().string();
    profile(config).warp = "E5M1";

    const fs::path cut = tests::Sandbox::root() / "launch" / "cut.wad";

    launch::write(cut, std::string("PWAD\x01\0\0\0\0\x10\0\0", 12));
    files(config, {cut.string()});

    EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "5", "1"}));

    profile(config).files.front().file = launch::wad(cut.parent_path() / "whole.wad", "PWAD", {"E5M2"}).string();

    EXPECT_FALSE(has(Arguments::of(config), "-warp"));
}

TEST_F(ArgumentsOf, ChocolateMergesItsAddOns) {
    Config config = on("chocolate-doom");

    files(config, {"/mods/a.wad", "/mods/b.wad", "/mods/c.deh"});

    EXPECT_EQ(Arguments::of(config),
              (Strings{"-iwad", iwad(), "-merge", "/mods/a.wad", "/mods/b.wad", "-deh", "/mods/c.deh"}));
}

TEST_F(ArgumentsOf, CrispyLoadsItsAddOnsWithFile) {
    Config config = on("crispy-doom");

    files(config, {"/mods/a.wad", "/mods/b.wad"});

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-file", "/mods/a.wad", "/mods/b.wad"}));
}

TEST_F(ArgumentsOf, ChocolatePickedForAnUnknownNameMerges) {
    Config config = on("mystery-engine");

    config.ports.front().dialect = "chocolate";
    files(config, {"/mods/a.wad"});

    EXPECT_TRUE(contains(Arguments::of(config), {"-merge", "/mods/a.wad"}));
}

TEST_F(ArgumentsOf, AMergedAddOnStillCountsAsTypedFiles) {
    Config config = on("chocolate-doom");

    config.iwads.front().file = episodic().string();
    profile(config).extra = "-merge /mods/typed.wad";
    profile(config).warp = "E5M1";

    EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "5", "1"}));
}

TEST_F(ArgumentsOf, HelionWritesLevelstat) {
    Config config = on("helion");

    profile(config).levelstat = true;

    EXPECT_TRUE(has(Arguments::of(config), "-levelstat"));
}

TEST_F(ArgumentsOf, HelionTakesTheComplevelByName) {
    Config config = on("helion");

    profile(config).config = "alpha.cfg";

    const std::vector<std::pair<int, std::string>> named = {{2, "vanilla"}, {9, "boom"}, {11, "mbf"}, {21, "mbf21"}};

    for (const auto &[level, name] : named) {
        profile(config).complevel = level;
        profile(config).replay = {.mode = ReplayMode::Record, .file = "run1"};

        const Strings args = Arguments::of(config);

        EXPECT_TRUE(contains(args, {"+complevel", name})) << level;
        EXPECT_FALSE(has(args, "-complevel")) << level;
    }

    profile(config).complevel = 17;

    EXPECT_FALSE(has(Arguments::of(config), "+complevel")) << "outside the table";
}

TEST_F(ArgumentsOf, HelionRecordsToTheWholePath) {
    Config config = on("helion");

    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Record, .file = "run1"};

    EXPECT_TRUE(contains(Arguments::of(config),
                         {"-record", (launch::profiles() / "alpha" / "replays" / "run1.lmp").string()}));
}

TEST_F(ArgumentsOf, HelionGetsEveryPatchBehindOneDeh) {
    Config config = on("helion");

    files(config, {"/mods/a.wad", "/mods/b.deh", "/mods/c.bex"});

    EXPECT_EQ(Arguments::of(config),
              (Strings{"-iwad", iwad(), "-file", "/mods/a.wad", "-deh", "/mods/b.deh", "/mods/c.bex"}));
}

// Helion reads a word it does not know as one more value of the switch before it.
TEST_F(ArgumentsOf, HelionIsHandedOnlySwitchesItKnows) {
    static const Strings KNOWN = {
            "-iwad",        "-file",       "-portable",    "-config",    "-savedir",
            "-log",         "-loglevel",   "-logprofiler", "-warp",      "-skill",
            "+map",         "-nomonsters", "-levelstat",   "-loadgame",  "+sv_fastmonsters",
            "-fast",        "-deh",        "-record",      "-playdemo",  "-nomusic",
            "+setpos",      "+cheats",     "+setangle",    "+setpitch",  "+glversion",
            "-pistolstart", "-solo-net",   "+complevel",   "+restarted",
    };

    const auto unknown = [](const Strings &args) {
        Strings found;

        for (const std::string &each : args) {
            if ((each.starts_with('-') || each.starts_with('+')) && std::ranges::find(KNOWN, each) == KNOWN.end()) {
                found.push_back(each);
            }
        }

        return found;
    };

    Config config = on("helion");

    config.general.profileConfigs = true;
    profile(config).config = "alpha.cfg";
    profile(config).captureOutput = true;
    profile(config).skill = 3;
    profile(config).monsters = 4;
    profile(config).warp = "MAP07";
    profile(config).levelstat = true;
    profile(config).pistolStart = true;
    profile(config).cheats = true;
    profile(config).skipAutoload = true;
    profile(config).music = false;
    profile(config).soundEffects = false;
    profile(config).display = Display::Fullscreen;
    profile(config).screenshots = Screenshots::Profile;
    files(config, {"/mods/a.wad", "/mods/b.deh", "/mods/c.bex", "/mods/d.cfg", "/mods/e.lmp"});

    EXPECT_EQ(unknown(Arguments::of(config)), Strings{});

    profile(config).complevel = 21;
    profile(config).replay = {.mode = ReplayMode::Record, .file = "run1", .longtics = true, .soloNet = true};

    EXPECT_EQ(unknown(Arguments::of(config)), Strings{});

    profile(config).replay = {.mode = ReplayMode::Play, .file = "run1", .playback = Playback::Fast};

    EXPECT_EQ(unknown(Arguments::of(config)), Strings{});

    profile(config).replay = {.mode = ReplayMode::Continue, .file = "run2", .source = "run1"};
    launch::write(launch::profiles() / "alpha" / "replays" / "run1.lmp", "x");

    EXPECT_EQ(unknown(Arguments::of(config)), Strings{});
}

TEST_F(ArgumentsOf, AnEpisodeMapWithTwoDigitsWarpsOnAPortThatTakesIt) {
    for (const char *program : {"dsda-doom", "prboom-plus", "woof", "nugget-doom", "eternity", "helion"}) {
        Config config = on(program);

        config.iwads.front().file =
                launch::wad(tests::Sandbox::root() / "launch" / "long-episode.wad", "IWAD", {"E1M1", "E1M10"}).string();
        profile(config).warp = "E1M10";

        const Strings args = Arguments::of(config);

        EXPECT_TRUE(contains(args, {"-warp", "1", "10"})) << program;
        EXPECT_FALSE(has(args, "+map")) << program;

        profile(config).warp = "E1M1";

        EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "1", "1"})) << program;
    }
}

TEST_F(ArgumentsOf, AnEpisodeMapWithTwoDigitsGoesToMapOnAPortThatReadsOneDigit) {
    for (const char *program : {"gzdoom", "uzdoom", "zdoom", "zandronum", "skulltag"}) {
        Config config = on(program);

        config.iwads.front().file =
                launch::wad(tests::Sandbox::root() / "launch" / "long-episode.wad", "IWAD", {"E1M1", "E1M10"}).string();
        profile(config).warp = "E1M10";

        const Strings args = Arguments::of(config);

        EXPECT_TRUE(contains(args, {"+map", "E1M10"})) << program;
        EXPECT_FALSE(has(args, "-warp")) << program;

        profile(config).warp = "E1M1";

        EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "1", "1"})) << program;
    }
}

TEST_F(ArgumentsOf, AnEpisodeMapWithTwoDigitsIsDroppedOnAPortWithNeitherWay) {
    for (const char *program : {"chocolate-doom", "crispy-doom", "mbf"}) {
        Config config = on(program);

        config.iwads.front().file =
                launch::wad(tests::Sandbox::root() / "launch" / "long-episode.wad", "IWAD", {"E1M1", "E1M10"}).string();
        profile(config).warp = "E1M10";

        const Strings args = Arguments::of(config);

        EXPECT_FALSE(has(args, "-warp")) << program;
        EXPECT_FALSE(has(args, "+map")) << program;

        profile(config).warp = "E1M1";

        EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "1", "1"})) << program;
    }
}

TEST_F(ArgumentsOf, AnEpisodeMapNumberNeedsNoLeadingZero) {
    Config config = on("gzdoom");

    config.iwads.front().file =
            launch::wad(tests::Sandbox::root() / "launch" / "zero-episode.wad", "IWAD", {"E1M1", "E1M05"}).string();
    profile(config).warp = "E1M05";

    const Strings args = Arguments::of(config);

    EXPECT_FALSE(has(args, "-warp"));
    EXPECT_TRUE(contains(args, {"+map", "E1M05"}));
}

TEST_F(ArgumentsOf, OnlyACapturedZdoomLaunchOnWindowsAsksForStdout) {
    Config config = on("gzdoom");

    EXPECT_FALSE(Arguments::asksForStdout(config, true)) << "nothing captured";

    profile(config).captureOutput = true;

    EXPECT_TRUE(Arguments::asksForStdout(config, true));
    EXPECT_FALSE(Arguments::asksForStdout(config, false));

    for (const char *program : {"zdoom", "zandronum", "skulltag", "lzdoom"}) {
        config.ports.front().file = (fs::path("/games") / program).string();

        EXPECT_TRUE(Arguments::asksForStdout(config, true)) << program;
    }

    for (const char *program : {"uzdoom", "dsda-doom", "helion", "chocolate-doom"}) {
        config.ports.front().file = (fs::path("/games") / program).string();

        EXPECT_FALSE(Arguments::asksForStdout(config, true)) << program;
    }

    for (const char *program : {"mystery-engine", "doom_gog.exe"}) {
        config.ports.front().file = (fs::path("/games") / program).string();

        ASSERT_FALSE(Dialect::of(config).recognised) << program;
        EXPECT_FALSE(Arguments::asksForStdout(config, true)) << program << " is only guessed to be GZDoom";

        for (const char *picked : {"gzdoom", "zdoom"}) {
            config.ports.front().dialect = picked;

            EXPECT_TRUE(Dialect::of(config).recognised) << program << " picked as " << picked;
            EXPECT_TRUE(Arguments::asksForStdout(config, true)) << program << " picked as " << picked;
        }

        config.ports.front().dialect.clear();
    }

    config.ports.front().file = "/games/gzdoom";
    config.general.autoClose = true;

    EXPECT_FALSE(Arguments::asksForStdout(config, true)) << "auto close captures nothing";

    config.general.autoClose = false;
    config.ports.front().file = "/games/DOOM2.EXE";
    config.ports.front().dosbox = true;
    config.ports.front().dialect = "gzdoom";

    EXPECT_FALSE(Arguments::asksForStdout(config, true)) << "DOSBox captures nothing";
}

TEST_F(ArgumentsOf, TheLineAsksForStdoutExactlyWhenTheRuleSays) {
#ifdef _WIN32
    constexpr bool windows = true;
#else
    constexpr bool windows = false;
#endif

    Config config = on("gzdoom");

    profile(config).captureOutput = true;
    profile(config).extra = "-nomusic";

    const Strings args = Arguments::of(config);

    EXPECT_EQ(has(args, "-stdout"), windows);
    EXPECT_EQ(has(args, "-stdout"), Arguments::asksForStdout(config, windows));

    profile(config).captureOutput = false;

    EXPECT_FALSE(has(Arguments::of(config), "-stdout"));
}

TEST_F(ArgumentsOf, FlagWordsGoOnAsTheirCvars) {
    Config config = on("gzdoom");

    profile(config).flag(Flags::Word::dmflags) = "1024";
    profile(config).flag(Flags::Word::dmflags3) = "128";
    profile(config).flag(Flags::Word::compatflags2) = "0";

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(contains(args, {"+dmflags", "1024", "+dmflags3", "128", "+compatflags2", "0"}));
}

TEST_F(ArgumentsOf, AFlagWordGoesOnInDecimal) {
    Config config = on("gzdoom");

    // The port reads a leading zero as octal.
    profile(config).flag(Flags::Word::dmflags) = "010";

    EXPECT_TRUE(contains(Arguments::of(config), {"+dmflags", "10"}));
}

TEST_F(ArgumentsOf, AFlagWordThatIsNotANumberIsLeftOut) {
    Config config = on("gzdoom");

    for (const char *typed : {"abc", "0x10", "-4", "1 2", "4294967296"}) {
        profile(config).flag(Flags::Word::dmflags) = typed;

        EXPECT_FALSE(has(Arguments::of(config), "+dmflags")) << typed;
    }

    profile(config).flag(Flags::Word::dmflags) = "4294967295";

    EXPECT_TRUE(contains(Arguments::of(config), {"+dmflags", "4294967295"}));
}

TEST_F(ArgumentsOf, AFlagWordThePortDoesNotReadIsLeftOut) {
    Config config = on("zdoom");

    profile(config).flag(Flags::Word::dmflags3) = "128";
    profile(config).flag(Flags::Word::zadmflags) = "1";

    const Strings args = Arguments::of(config);

    EXPECT_FALSE(has(args, "+dmflags3"));
    EXPECT_FALSE(has(args, "+zadmflags"));
}

TEST_F(ArgumentsOf, ZandronumTakesItsOwnWords) {
    Config config = on("zandronum");

    profile(config).flag(Flags::Word::zadmflags) = "64";
    profile(config).flag(Flags::Word::lmsWeapons) = "3";

    EXPECT_TRUE(contains(Arguments::of(config), {"+zadmflags", "64", "+lmsallowedweapons", "3"}));
}

TEST_F(ArgumentsOf, EternityTakesDmflagsAsASwitch) {
    Config config = on("eternity");

    profile(config).flag(Flags::Word::dmflags) = "32";
    profile(config).flag(Flags::Word::compatflags) = "1";

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(contains(args, {"-dmflags", "32"}));
    EXPECT_FALSE(has(args, "+compatflags"));
}

TEST_F(ArgumentsOf, APortWithoutFlagWordsGetsNone) {
    for (const char *program : {"dsda-doom", "woof", "chocolate-doom", "helion"}) {
        Config config = on(program);

        profile(config).flag(Flags::Word::dmflags) = "1024";

        const Strings args = Arguments::of(config);

        EXPECT_FALSE(has(args, "+dmflags")) << program;
        EXPECT_FALSE(has(args, "-dmflags")) << program;
    }
}

TEST_F(ArgumentsOf, AJoiningPlayerLeavesTheFlagsToTheHost) {
    Config config = on("gzdoom");

    profile(config).flag(Flags::Word::dmflags) = "1024";
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.host = "10.0.0.7";

    EXPECT_FALSE(has(Arguments::of(config), "+dmflags"));

    profile(config).multiplayer.players = 2;

    EXPECT_TRUE(contains(Arguments::of(config), {"+dmflags", "1024"})) << "the host sets them";
}

TEST_F(ArgumentsOf, FlagWordsComeBeforeThePistolStartCvar) {
    Config config = on("gzdoom");

    profile(config).flag(Flags::Word::dmflags3) = "0";
    profile(config).pistolStart = true;

    const Strings args = Arguments::of(config);
    const auto word = std::ranges::find(args, "+dmflags3");
    const auto cvar = std::ranges::find(args, "+sv_pistolstart");

    ASSERT_NE(word, args.end());
    ASSERT_NE(cvar, args.end());
    EXPECT_LT(word, cvar);
}

TEST_F(ArgumentsOf, RecordingKeepsTheComplevelBesideADemoAmongTheAddons) {
    Config config = on("dsda-doom");

    profile(config).config = "alpha.cfg";
    profile(config).complevel = 9;
    profile(config).replay = {.mode = ReplayMode::Record, .file = "run1"};
    files(config, {"/mods/a.lmp"});

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(contains(args, {"-complevel", "9"}));
    EXPECT_FALSE(has(args, "-playdemo")) << "the recording wins over the add-on demo";
}

TEST_F(ArgumentsOf, HelionTakesTheComplevelOutsideARecordingToo) {
    Config config = on("helion");

    profile(config).complevel = 21;

    EXPECT_TRUE(contains(Arguments::of(config), {"+complevel", "mbf21"}));
}

TEST_F(ArgumentsOf, ContinueOnAPortThatDoesNotInsistLeavesTheComplevelOut) {
    Config config = on("prboom-plus");
    const fs::path replays = launch::profiles() / "alpha" / "replays";

    profile(config).config = "alpha.cfg";
    profile(config).complevel = 9;
    profile(config).replay = {.mode = ReplayMode::Continue, .file = "run2", .source = "run1"};
    launch::write(replays / "run1.lmp", "x");

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(has(args, "-recordfromto"));
    EXPECT_FALSE(has(args, "-complevel"));
}

TEST_F(ArgumentsOf, ZdoomAndZandronumGetTheTopBitAsANegativeNumber) {
    for (const char *program : {"zdoom", "zandronum"}) {
        Config config = on(program);

        profile(config).flag(Flags::Word::compatflags) = "3122215895";
        profile(config).flag(Flags::Word::compatflags2) = "11";

        const Strings args = Arguments::of(config);

        EXPECT_TRUE(contains(args, {"+compatflags", "-1172751401"})) << program;
        EXPECT_TRUE(contains(args, {"+compatflags2", "11"})) << program;
    }

    Config config = on("gzdoom");

    profile(config).flag(Flags::Word::compatflags) = "3122215895";

    EXPECT_TRUE(contains(Arguments::of(config), {"+compatflags", "3122215895"})) << "strtoll reads it whole";
}

TEST_F(ArgumentsOf, ASwitchTurnedOnGoesOnTheLine) {
    Config config = on("crispy-doom");

    profile(config).setRuleOn(Rules::Rule::coopSpawns, true);
    profile(config).setRuleOn(Rules::Rule::flipLevels, true);

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(has(args, "-coop_spawns"));
    EXPECT_TRUE(has(args, "-fliplevels"));
}

TEST_F(ArgumentsOf, ASwitchCarriesItsOwnValue) {
    Config config = on("gzdoom");

    profile(config).setRuleOn(Rules::Rule::alwaysApplyDmflags, true);

    EXPECT_TRUE(contains(Arguments::of(config), {"+alwaysapplydmflags", "1"}));
}

TEST_F(ArgumentsOf, ANumberGoesOnOnlyInsideThePortsRange) {
    Config config = on("dsda-doom");

    profile(config).rule(Rules::Rule::turbo) = "150";
    profile(config).rule(Rules::Rule::speed) = "200";

    EXPECT_TRUE(contains(Arguments::of(config), {"-turbo", "150", "-game_speed", "200"}));

    profile(config).rule(Rules::Rule::turbo) = "300";

    EXPECT_FALSE(has(Arguments::of(config), "-turbo"));

    Config woof = on("woof");

    profile(woof).rule(Rules::Rule::turbo) = "300";
    profile(woof).rule(Rules::Rule::speed) = "200";

    EXPECT_TRUE(contains(Arguments::of(woof), {"-turbo", "300", "-speed", "200"}));
}

TEST_F(ArgumentsOf, APortGetsNoSwitchItLacks) {
    Config config = on("dsda-doom");

    profile(config).rule(Rules::Rule::dogs) = "2";

    for (const Rules::Rule rule : {Rules::Rule::beta,
                                   Rules::Rule::doubleAmmo,
                                   Rules::Rule::coopPreset,
                                   Rules::Rule::alwaysApplyDmflags,
                                   Rules::Rule::vanilla}) {
        profile(config).setRuleOn(rule, true);
    }

    const Strings args = Arguments::of(config);

    for (const char *flag : {"-dogs", "-beta", "-doubleammo", "-coop", "+alwaysapplydmflags", "-vanilla"}) {
        EXPECT_FALSE(has(args, flag)) << flag;
    }
}

TEST_F(ArgumentsOf, DoubleAmmoIsLeftOutWhereCrispyStopsOnIt) {
    Config config = on("crispy-doom");

    profile(config).config = "alpha.cfg";
    profile(config).setRuleOn(Rules::Rule::doubleAmmo, true);

    EXPECT_TRUE(has(Arguments::of(config), "-doubleammo"));

    profile(config).replay = {.mode = ReplayMode::Record, .file = "run1"};

    EXPECT_FALSE(has(Arguments::of(config), "-doubleammo")) << "recording";

    profile(config).replay = {};
    files(config, {"/mods/a.lmp"});

    EXPECT_FALSE(has(Arguments::of(config), "-doubleammo")) << "playing a demo";

    files(config, {});
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.players = 2;

    EXPECT_FALSE(has(Arguments::of(config), "-doubleammo")) << "in a netgame";
}

TEST_F(ArgumentsOf, ChocolateTakesItsGameVersion) {
    Config config = on("chocolate-doom");

    profile(config).complevel = Dialect::GAME_VERSIONS + 5;

    EXPECT_TRUE(contains(Arguments::of(config), {"-gameversion", "1.9"}));

    profile(config).complevel = 9;

    EXPECT_FALSE(has(Arguments::of(config), "-gameversion")) << "a complevel is no game version";

    Config dsda = on("dsda-doom");

    profile(dsda).complevel = Dialect::GAME_VERSIONS + 5;

    EXPECT_FALSE(has(Arguments::of(dsda), "-complevel")) << "a game version is no complevel";
}

TEST_F(ArgumentsOf, DeathmatchThreeIsHostedWhereThePortHasIt) {
    for (const char *program : {"woof", "crispy-doom"}) {
        Config config = on(program);

        profile(config).multiplayer.gameType = GameType::Deathmatch3;
        profile(config).multiplayer.players = 2;

        EXPECT_TRUE(has(Arguments::of(config), "-dm3")) << program;
    }

    Config chocolate = on("chocolate-doom");

    profile(chocolate).multiplayer.gameType = GameType::Deathmatch3;
    profile(chocolate).multiplayer.players = 2;

    const Strings args = Arguments::of(chocolate);

    EXPECT_FALSE(has(args, "-dm3"));
    EXPECT_TRUE(has(args, "-deathmatch")) << "plain deathmatch where the port has no 3.0";
}

TEST_F(ArgumentsOf, AGameVersionGoesOnForDemosPlayedBackToo) {
    Config config = on("chocolate-doom");

    profile(config).config = "alpha.cfg";
    profile(config).complevel = Dialect::GAME_VERSIONS + 2;
    profile(config).replay = {.mode = ReplayMode::Play, .file = "run1"};
    launch::write(launch::profiles() / "alpha" / "replays" / "run1.lmp", "x");

    EXPECT_TRUE(contains(Arguments::of(config), {"-gameversion", "1.666"})) << "Chocolate stops on another version";

    profile(config).replay = {};
    files(config, {"/mods/a.lmp"});

    EXPECT_TRUE(contains(Arguments::of(config), {"-gameversion", "1.666"})) << "a demo among the add-ons";

    files(config, {});
    profile(config).replay = {.mode = ReplayMode::Record, .file = "run2"};

    EXPECT_TRUE(contains(Arguments::of(config), {"-gameversion", "1.666"}));
}

TEST_F(ArgumentsOf, MbfBetaGoesOnOnlyWithTheMbfComplevelPicked) {
    Config config = on("woof");

    profile(config).setRuleOn(Rules::Rule::beta, true);

    EXPECT_FALSE(has(Arguments::of(config), "-beta")) << "the port's own is MBF21, where Woof stops";

    profile(config).complevel = 21;

    EXPECT_FALSE(has(Arguments::of(config), "-beta"));

    profile(config).complevel = 11;

    EXPECT_TRUE(has(Arguments::of(config), "-beta"));
}

TEST_F(ArgumentsOf, HelperDogsAreLeftOutBelowMbfOnWoof) {
    Config config = on("woof");

    profile(config).rule(Rules::Rule::dogs) = "2";

    EXPECT_TRUE(contains(Arguments::of(config), {"-dogs", "2"})) << "the port's own counts";

    profile(config).complevel = 9;

    EXPECT_FALSE(has(Arguments::of(config), "-dogs"));

    profile(config).complevel = 11;

    EXPECT_TRUE(contains(Arguments::of(config), {"-dogs", "2"}));

    Config prboom = on("prboom-plus");

    profile(prboom).rule(Rules::Rule::dogs) = "2";
    profile(prboom).complevel = 9;

    EXPECT_TRUE(contains(Arguments::of(prboom), {"-dogs", "2"})) << "PrBoom+ takes them at any level";
}

TEST_F(ArgumentsOf, AJoiningPlayerLeavesTheSwitchesToTheHost) {
    Config config = on("woof");

    profile(config).setRuleOn(Rules::Rule::coopSpawns, true);
    profile(config).rule(Rules::Rule::turbo) = "150";
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.host = "10.0.0.7";

    const Strings args = Arguments::of(config);

    EXPECT_FALSE(has(args, "-coop_spawns"));
    EXPECT_FALSE(has(args, "-turbo"));
}

TEST_F(ArgumentsOf, DoubleAmmoIsLeftOutWhileTheProfilePlaysItsDemo) {
    Config config = on("crispy-doom");

    profile(config).config = "alpha.cfg";
    profile(config).setRuleOn(Rules::Rule::doubleAmmo, true);
    profile(config).replay = {.mode = ReplayMode::Play, .file = "run1"};
    launch::write(launch::profiles() / "alpha" / "replays" / "run1.lmp", "x");

    EXPECT_FALSE(has(Arguments::of(config), "-doubleammo"));
}

TEST_F(ArgumentsOf, ChocolateHereticHexenAndStrifeGetNoDoomRules) {
    for (const char *program : {"chocolate-heretic", "chocolate-hexen", "chocolate-strife", "crispy-heretic"}) {
        Config config = on(program);

        profile(config).complevel = Dialect::GAME_VERSIONS + 5;
        profile(config).rule(Rules::Rule::turbo) = "150";
        profile(config).multiplayer.gameType = GameType::Deathmatch3;
        profile(config).multiplayer.players = 2;

        const Strings args = Arguments::of(config);

        EXPECT_FALSE(has(args, "-gameversion")) << program;
        EXPECT_FALSE(has(args, "-turbo")) << program;
        EXPECT_FALSE(has(args, "-dm3")) << program;
    }
}
