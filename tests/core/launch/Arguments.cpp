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
#include "core/launch/KnownPorts.h"
#include "core/launch/Storage.h"
#include "core/launch/dialects/Dialects.h"
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

    static Config on(const char *file) { return launch::single(fs::path("/games") / file); }

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

    EXPECT_TRUE(contains(args, {"+sv_fastmonsters", "1"}));
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

TEST_F(ArgumentsOf, AWarpOnlyAnAddOnMarkedGoneCouldHoldIsDropped) {
    Config config = on("chocolate-doom");

    config.iwads.front().file = episodic().string();
    files(config, {"/mods/sigil.wad"});
    profile(config).warp = "E5M1";

    EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "5", "1"})) << "an add-on that cannot be read";

    profile(config).files.front().gone = true;

    EXPECT_FALSE(has(Arguments::of(config), "-warp"));
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
            {.file = "/mods/a.wad"},
            {.file = "/mods/b.wad", .enabled = false},
            {.file = "/mods/c.pk3"},
    };

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-file", "/mods/a.wad", "/mods/c.pk3"}));
}

TEST_F(ArgumentsOf, AMissingFileIsPassedAsWritten) {
    Config config = on("gzdoom");

    files(config, {"nowhere.wad"});

    EXPECT_TRUE(contains(Arguments::of(config, tests::Sandbox::scratch("arguments-port")), {"-file", "nowhere.wad"}));
}

TEST_F(ArgumentsOf, AnAddOnMarkedGoneIsLeftOffWhateverItsKind) {
    Config config = on("gzdoom");

    files(config, {"/mods/a.wad", "/mods/b.pk3", "/mods/c.deh", "/mods/d.bex", "/mods/e.cfg", "/mods/f.lmp"});

    for (FileEntry &entry : profile(config).files) {
        entry.gone = true;
    }

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad()}));
}

TEST_F(ArgumentsOf, AnAddOnBesideOneMarkedGoneStays) {
    Config config = on("gzdoom");

    files(config, {"/mods/a.wad", "/mods/b.wad", "/mods/c.deh", "/mods/d.deh"});
    profile(config).files[0].gone = true;
    profile(config).files[2].gone = true;

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-file", "/mods/b.wad", "-deh", "/mods/d.deh"}));
}

TEST_F(ArgumentsOf, OnlyTheAddOnsFoundGoneAreLeftOff) {
    const fs::path folder = tests::Sandbox::scratch("arguments-gone");
    const fs::path kept = folder / "kept.wad";

    launch::write(kept, "PWAD");

    Config config = on("gzdoom");

    files(config, {kept.string(), (folder / "gone.wad").string(), "nowhere.wad"});
    Storage::markGoneAddons(profile(config));

    EXPECT_EQ(Arguments::of(config, tests::Sandbox::scratch("arguments-port")),
              (Strings{"-iwad", iwad(), "-file", kept.string(), "nowhere.wad"}));
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
    profile(config).set(Switch::sharedConfig, true);

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
    profile(config).replay = {.mode = ReplayMode::Record, .longtics = true, .soloNet = true, .file = "run1.lmp"};

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
    profile(config).replay = {.mode = ReplayMode::Record, .longtics = true, .soloNet = true, .file = "run1"};

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

    profile(config).replay = {.mode = ReplayMode::Play, .playback = Playback::Timed, .file = "/demos/a.lmp"};

    EXPECT_TRUE(contains(Arguments::of(config), {"-timedemo", "/demos/a.lmp"}));
}

TEST_F(ArgumentsOf, FastPlaybackFallsBackToPlaydemoOnAPortWithoutIt) {
    Config config = on("gzdoom");

    profile(config).replay = {.mode = ReplayMode::Play, .playback = Playback::Fast, .file = "/demos/a.lmp"};

    EXPECT_TRUE(contains(Arguments::of(config), {"-playdemo", "/demos/a.lmp"}));
}

TEST_F(ArgumentsOf, FastPlaybackUsesFastdemoWhereThereIsOne) {
    Config config = on("dsda-doom");

    profile(config).replay = {.mode = ReplayMode::Play, .playback = Playback::Fast, .file = "/demos/a.lmp"};

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

    profile(dsda).setOption(Dialect::Feature::levelstat, true);
    profile(gzdoom).setOption(Dialect::Feature::levelstat, true);

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
    profile(config).save = {.enabled = true, .file = "prboom-plus-savegame4.dsg"};
    savedInProfile("prboom-plus-savegame4.dsg");

    EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "4"}));
}

TEST_F(ArgumentsOf, ANuggetPeriodicAutosaveIsNotLoadedBySlot) {
    Config config = on("nugget-doom");

    config.general.profileConfigs = true;
    profile(config).config = "alpha.cfg";
    profile(config).save = {.enabled = true, .file = "autosav3.dsg"};
    savedInProfile("autosav3.dsg");

    EXPECT_FALSE(has(Arguments::of(config), "-loadgame"));
}

TEST_F(ArgumentsOf, WoofLoadsItsAutosaveAsSlot255) {
    Config config = on("woof");

    config.general.profileConfigs = true;
    profile(config).config = "alpha.cfg";
    profile(config).save = {.enabled = true, .file = "autosave.dsg"};
    savedInProfile("autosave.dsg");

    EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "255"}));
}

TEST_F(ArgumentsOf, ABoomPortDoesNotLoadAnAutosave) {
    Config config = on("prboom-plus");

    profile(config).save = {.enabled = true, .file = saved("autosave.dsg")};

    EXPECT_FALSE(has(Arguments::of(config), "-loadgame"));
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

TEST_F(ArgumentsOf, TheWoofLineLoadsASaveBySlotUpTo77) {
    for (const auto &[file, prefix] :
         {std::pair{"woof", "woofsav"}, std::pair{"nugget-doom", "nuggsav"}, std::pair{"cherry-doom", "chersav"}}) {
        Config config = on(file);

        profile(config).save = {.enabled = true, .file = saved(std::string(prefix) + "77.dsg")};

        EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "77"})) << file;

        profile(config).save.file = saved(std::string(prefix) + "78.dsg");

        EXPECT_FALSE(has(Arguments::of(config), "-loadgame")) << file;
    }
}

TEST_F(ArgumentsOf, WoofBefore15LoadsAnySlotAndNoAutosave) {
    Config config = on("woof");

    config.engines.front().version = "10.2.0";
    profile(config).save = {.enabled = true, .file = saved("woofsav78.dsg")};

    EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "78"}));

    profile(config).save.file = saved("autosave.dsg");

    EXPECT_FALSE(has(Arguments::of(config), "-loadgame"));

    config.engines.front().version = "15.0.0";

    EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "255"}));
}

TEST_F(ArgumentsOf, CherryLoadsItsOwnSlotsAndItsAutosave) {
    Config config = on("cherry-doom");

    profile(config).save = {.enabled = true, .file = saved("chersav4.dsg")};

    EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "4"}));

    profile(config).save.file = saved("autosave.dsg");

    EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "255"}));

    for (const char *other : {"autosav3.dsg", "woofsav4.dsg"}) {
        profile(config).save.file = saved(other);

        EXPECT_FALSE(has(Arguments::of(config), "-loadgame")) << other;
    }
}

TEST_F(ArgumentsOf, CherryAndTheWoofReleasesBefore16AreHandedNoId24) {
    Config cherry = on("cherry-doom");
    Config woof = on("woof");
    Config nugget = on("nugget-doom");

    woof.engines.front().version = "15.0.0";
    nugget.engines.front().version = "4.0.0";

    for (Config *config : {&cherry, &woof, &nugget}) {
        profile(*config).complevel = Dialect::ID24;

        EXPECT_FALSE(has(Arguments::of(*config), "-complevel")) << config->engines.front().file;

        profile(*config).complevel = Dialect::MBF21;

        EXPECT_TRUE(contains(Arguments::of(*config), {"-complevel", "21"})) << config->engines.front().file;
    }
}

TEST_F(ArgumentsOf, CherryContinuesADemoWithoutAComplevel) {
    Config config = on("cherry-doom");
    const fs::path replays = launch::profiles() / "alpha" / "replays";

    profile(config).config = "alpha.cfg";
    profile(config).complevel = 9;
    profile(config).replay = {.mode = ReplayMode::Continue, .file = "run2", .source = "run1"};
    launch::write(replays / "run1.lmp", "x");

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(has(args, "-recordfromto"));
    EXPECT_FALSE(has(args, "-complevel"));
}

TEST_F(ArgumentsOf, DsdaIsHandedASaveOnlyByTheReleasesThatLoadOne) {
    Config config = on("dsda-doom");

    profile(config).save = {.enabled = true, .file = saved("dsda-doom-savegame4.dsg")};

    for (const char *version : {"", "0.30.0", "0.21.2", "0.21.1"}) {
        config.engines.front().version = version;

        EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "4"})) << version;
    }

    for (const char *version : {"0.28.0", "0.27.0", "0.26.0", "0.25.0", "0.22.3"}) {
        config.engines.front().version = version;

        EXPECT_FALSE(has(Arguments::of(config), "-loadgame")) << version;
    }
}

TEST_F(ArgumentsOf, ASaveStaysPickedForTheReleasesThatLoadNone) {
    Config config = on("dsda-doom");

    config.engines.front().version = "0.28.0";
    profile(config).save = {.enabled = true, .file = saved("dsda-doom-savegame4.dsg")};

    EXPECT_FALSE(has(Arguments::of(config), "-loadgame"));
    EXPECT_TRUE(profile(config).save.enabled);
    EXPECT_EQ(Storage::saveTrouble(config), "");

    config.engines.front().version = "0.30.0";

    EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "4"}));
}

TEST_F(ArgumentsOf, DsdaTakesChainEpisodesAndGameSpeedFrom0_26_0) {
    Config config = on("dsda-doom");

    profile(config).setOption(Dialect::Feature::chainEpisodes, true);
    profile(config).setNumber(Dialect::Feature::gameSpeed, "200");

    for (const char *version : {"", "0.30.0", "0.28.0", "0.27.0", "0.26.0"}) {
        config.engines.front().version = version;

        EXPECT_TRUE(contains(Arguments::of(config), {"-chain_episodes"})) << version;
        EXPECT_TRUE(contains(Arguments::of(config), {"-game_speed", "200"})) << version;
    }

    for (const char *version : {"0.25.0", "0.22.3", "0.21.2", "0.21.1"}) {
        config.engines.front().version = version;

        EXPECT_FALSE(has(Arguments::of(config), "-chain_episodes")) << version;
        EXPECT_FALSE(has(Arguments::of(config), "-game_speed")) << version;
    }
}

TEST_F(ArgumentsOf, AnOlderDsdaStillTakesTheRulesItHas) {
    Config config = on("dsda-doom");

    config.engines.front().version = "0.21.1";
    profile(config).setOption(Dialect::Feature::coopSpawns, true);
    profile(config).setOption(Dialect::Feature::stroller, true);
    profile(config).setOption(Dialect::Feature::noCheats, true);
    profile(config).setNumber(Dialect::Feature::turbo, "150");

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(has(args, "-coop_spawns"));
    EXPECT_TRUE(has(args, "-stroller"));
    EXPECT_TRUE(has(args, "-nocheats"));
    EXPECT_TRUE(contains(args, {"-turbo", "150"}));
}

TEST_F(ArgumentsOf, DsdaTakesPistolStartFrom0_21_2) {
    Config config = on("dsda-doom");

    profile(config).setOption(Dialect::Feature::pistolStart, true);

    for (const char *version : {"", "0.30.0", "0.22.3", "0.21.2"}) {
        config.engines.front().version = version;

        EXPECT_TRUE(has(Arguments::of(config), "-pistolstart")) << version;
    }

    config.engines.front().version = "0.21.1";

    EXPECT_FALSE(has(Arguments::of(config), "-pistolstart"));
}

TEST_F(ArgumentsOf, NyanDoomIsHandedASaveOnlyFrom1_6_1) {
    Config config = on("nyan-doom");

    profile(config).save = {.enabled = true, .file = saved("nyan-doom-savegame4.dsg")};

    for (const char *version : {"", "1.6.2", "1.6.1"}) {
        config.engines.front().version = version;

        EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "4"})) << version;
    }

    for (const char *version : {"1.6.0", "1.2.1", "1.1.0"}) {
        config.engines.front().version = version;

        EXPECT_FALSE(has(Arguments::of(config), "-loadgame")) << version;
    }
}

TEST_F(ArgumentsOf, NyanDoomIsHandedLimitRemovingFrom1_1_1) {
    Config config = on("nyan-doom");

    profile(config).setOption(Dialect::Feature::limitRemoving, true);
    profile(config).setOption(Dialect::Feature::chainEpisodes, true);
    profile(config).setOption(Dialect::Feature::pistolStart, true);

    for (const char *version : {"", "1.6.0", "1.1.1"}) {
        config.engines.front().version = version;

        EXPECT_TRUE(has(Arguments::of(config), "-lr")) << version;
    }

    config.engines.front().version = "1.1.0";

    const Strings oldest = Arguments::of(config);

    EXPECT_FALSE(has(oldest, "-lr"));
    EXPECT_TRUE(has(oldest, "-chain_episodes"));
    EXPECT_TRUE(has(oldest, "-pistolstart"));
}

// Organized saves win over -save before DSDA-Doom 0.28.3 and Nyan Doom 1.2.1. With them off in the config, the port
// saves in the -save folder.
TEST_F(ArgumentsOf, DsdaAndNyanDoomAreHandedASaveFolderOnEveryRelease) {
    using Case = std::pair<const char *, const char *>;

    for (const auto &[file, version] : {Case{"dsda-doom", ""},
                                        Case{"dsda-doom", "0.28.3"},
                                        Case{"dsda-doom", "0.28.2"},
                                        Case{"dsda-doom", "0.21.1"},
                                        Case{"nyan-doom", "1.2.1"},
                                        Case{"nyan-doom", "1.2.0"},
                                        Case{"nyan-doom", "1.1.0"}}) {
        Config config = on(file);

        config.general.profileConfigs = true;
        profile(config).config = "alpha.cfg";
        config.engines.front().version = version;

        const Strings args = Arguments::of(config);

        EXPECT_TRUE(contains(args, {"-save", (launch::profiles() / "alpha" / "saves").string()}))
                << file << " " << version;
        EXPECT_TRUE(has(args, "-config")) << file << " " << version;
    }
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
    profile(config).save = {.enabled = true, .file = "prboom-plus-savegame4.dsg"};

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

    profile(config).setOption(Dialect::Feature::pistolStart, true);

    EXPECT_TRUE(contains(Arguments::of(config), {"+sv_pistolstart", "1"}));

    config.engines.front().file = "/games/woof";

    EXPECT_TRUE(has(Arguments::of(config), "-pistolstart"));

    config.engines.front().file = "/games/zdoom";

    EXPECT_FALSE(has(Arguments::of(config), "+sv_pistolstart"));
    EXPECT_FALSE(has(Arguments::of(config), "-pistolstart"));

    config.engines.front().file = "/games/chocolate-doom";

    EXPECT_FALSE(has(Arguments::of(config), "-pistolstart"));
}

TEST_F(ArgumentsOf, ThePistolStartOptionStaysOutOfDemosAndNetgames) {
    Config config = on("crispy-doom");

    profile(config).config = "alpha.cfg";
    profile(config).setOption(Dialect::Feature::pistolStart, true);

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

    profile(config).setOption(Dialect::Feature::noAutoload, true);

    EXPECT_TRUE(has(Arguments::of(config), "-noautoload"));

    config.engines.front().file = "/games/prboom-plus";

    EXPECT_TRUE(has(Arguments::of(config), "-noload"));
    EXPECT_FALSE(has(Arguments::of(config), "-noautoload"));

    config.engines.front().file = "/games/eternity";

    EXPECT_TRUE(has(Arguments::of(config), "-noload"));
    EXPECT_FALSE(has(Arguments::of(config), "-noautoload"));

    config.engines.front().file = "/games/helion";

    EXPECT_FALSE(has(Arguments::of(config), "-noautoload"));
    EXPECT_FALSE(has(Arguments::of(config), "-noload"));
}

TEST_F(ArgumentsOf, MusicAndSoundEffectsOffPassTheirSwitches) {
    Config config = on("gzdoom");

    EXPECT_FALSE(has(Arguments::of(config), "-nomusic"));
    EXPECT_FALSE(has(Arguments::of(config), "-nosfx"));

    profile(config).setOption(Dialect::Feature::nomusic, true);
    profile(config).setOption(Dialect::Feature::nosfx, true);

    EXPECT_TRUE(has(Arguments::of(config), "-nomusic"));
    EXPECT_TRUE(has(Arguments::of(config), "-nosfx"));

    profile(config).setOption(Dialect::Feature::nosfx, false);

    EXPECT_TRUE(has(Arguments::of(config), "-nomusic"));
    EXPECT_FALSE(has(Arguments::of(config), "-nosfx"));

    profile(config).setOption(Dialect::Feature::nosfx, true);
    config.engines.front().file = "/games/helion";

    EXPECT_TRUE(has(Arguments::of(config), "-nomusic"));
    EXPECT_FALSE(has(Arguments::of(config), "-nosfx"));
}

TEST_F(ArgumentsOf, DisplayIsPassedTheWayEachPortTakesIt) {
    Config config = on("gzdoom");

    profile(config).display = Display::Window;

    EXPECT_TRUE(contains(Arguments::of(config), {"+vid_fullscreen", "0"}));

    profile(config).display = Display::Fullscreen;
    config.engines.front().file = "/games/zandronum";

    EXPECT_TRUE(contains(Arguments::of(config), {"+fullscreen", "1"}));

    config.engines.front().file = "/games/prboom-plus";

    // -fullscreen would be saved into its config, and -nowindow is not.
    EXPECT_TRUE(has(Arguments::of(config), "-nowindow"));
    EXPECT_FALSE(has(Arguments::of(config), "-fullscreen"));

    profile(config).display = Display::Window;

    EXPECT_TRUE(has(Arguments::of(config), "-window"));
    EXPECT_FALSE(has(Arguments::of(config), "-nofullscreen"));

    profile(config).display = Display::Fullscreen;

    config.engines.front().file = "/games/dsda-doom";

    EXPECT_TRUE(has(Arguments::of(config), "-fullscreen"));

    profile(config).display = Display::Window;
    config.engines.front().file = "/games/woof";

    EXPECT_TRUE(has(Arguments::of(config), "-window"));

    config.engines.front().file = "/games/eternity";

    EXPECT_FALSE(has(Arguments::of(config), "-window"));

    profile(config).display = Display::Port;
    config.engines.front().file = "/games/gzdoom";

    EXPECT_FALSE(has(Arguments::of(config), "+vid_fullscreen"));
}

TEST_F(ArgumentsOf, CheatsAreACvarOnlyTheZdoomFamilyTakes) {
    Config config = on("zandronum");

    profile(config).setOption(Dialect::Feature::cheats, true);

    EXPECT_TRUE(contains(Arguments::of(config), {"+sv_cheats", "1"}));

    config.engines.front().file = "/games/dsda-doom";

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

    config.engines.front().file = "/games/helion";

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

    profile(config).setOption(Dialect::Feature::pistolStart, true);
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.players = 2;

    EXPECT_TRUE(has(Arguments::of(config), "-pistolstart")) << "DSDA-Doom has no netgame to leave it out of";

    config.engines.front().file = "/games/woof";
    profile(config).multiplayer.players = 0;

    EXPECT_TRUE(has(Arguments::of(config), "-pistolstart")) << "a join with no address launches alone";
}

TEST_F(ArgumentsOf, PistolStartStaysOutOfAnAddOnDemo) {
    Config config = on("crispy-doom");

    profile(config).setOption(Dialect::Feature::pistolStart, true);
    files(config, {"/demos/other.lmp"});

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(has(args, "-playdemo"));
    EXPECT_FALSE(has(args, "-pistolstart"));
}

TEST_F(ArgumentsOf, ContinueWithoutASourceIsNoDemo) {
    Config config = on("woof");

    profile(config).config = "alpha.cfg";
    profile(config).setOption(Dialect::Feature::pistolStart, true);
    profile(config).replay = {.mode = ReplayMode::Continue, .file = "next"};

    const Strings args = Arguments::of(config);

    EXPECT_FALSE(has(args, "-recordfromto"));
    EXPECT_TRUE(has(args, "-pistolstart"));
}

TEST_F(ArgumentsOf, TheReplayIsLeftOutForTheFirstReasonThatHolds) {
    using Arguments::ReplayLeftOut;

    Config config = on("woof");
    const auto leftOut = [&config] { return Arguments::replayOf(config, Dialect::of(config)).leftOut; };

    profile(config).config = "alpha.cfg";

    EXPECT_EQ(leftOut(), ReplayLeftOut::off);

    profile(config).replay = {.mode = ReplayMode::Continue, .file = "next"};

    EXPECT_EQ(leftOut(), ReplayLeftOut::noSource);

    profile(config).replay.source = "run1";

    EXPECT_EQ(leftOut(), ReplayLeftOut::none);

    profile(config).replay.file.clear();

    EXPECT_EQ(leftOut(), ReplayLeftOut::noName);

    config.engines.front().file = "/games/chocolate-doom";

    EXPECT_EQ(leftOut(), ReplayLeftOut::cannotContinue);
}

TEST_F(ArgumentsOf, APortWithoutReplaysLeavesTheReplayOut) {
    Config config = on("woof");
    Dialect::Traits traits = Dialect::of(config);

    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Record, .file = "run"};
    traits.features &= ~Dialect::Features(Dialect::Feature::replays);

    EXPECT_EQ(Arguments::replayOf(config, traits).leftOut, Arguments::ReplayLeftOut::off);
}

TEST_F(ArgumentsOf, ARelativeNameWithoutAReplaysFolderCountsAsNoName) {
    Config config = on("woof");

    profile(config).config.clear();
    profile(config).replay = {.mode = ReplayMode::Record, .file = "run"};

    const Arguments::Replay relative = Arguments::replayOf(config, Dialect::of(config));

    EXPECT_TRUE(relative.file.empty());
    EXPECT_EQ(relative.leftOut, Arguments::ReplayLeftOut::noName);
    EXPECT_FALSE(has(Arguments::of(config), "-record"));

    profile(config).replay.file = "/demos/run.lmp";

    EXPECT_EQ(Arguments::replayOf(config, Dialect::of(config)).leftOut, Arguments::ReplayLeftOut::none);
    EXPECT_TRUE(has(Arguments::of(config), "-record"));
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

    config.engines.front().port = Port::Port::chocolate;
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

    profile(config).setOption(Dialect::Feature::levelstat, true);

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

// Helion reads only the first value after -deh and folds a second -deh into the first.
TEST_F(ArgumentsOf, HelionIsHandedOnlyThePatchItLoads) {
    Config config = on("helion");

    files(config, {"/mods/a.wad", "/mods/b.deh", "/mods/c.bex"});

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-file", "/mods/a.wad", "-deh", "/mods/b.deh"}));

    profile(config).files.clear();
    files(config, {"/mods/b.deh", "/mods/c.deh", "/mods/d.deh"});

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-deh", "/mods/b.deh"}));

    profile(config).files.clear();
    files(config, {"/mods/c.bex", "/mods/e.bex"});

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-deh", "/mods/c.bex"}));

    profile(config).files.clear();
    files(config, {"/mods/a.deh", "/mods/b.bex", "/mods/c.deh"});

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-deh", "/mods/a.deh"}));

    profile(config).files.clear();
    files(config, {"/mods/b.bex", "/mods/a.deh", "/mods/c.bex"});

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-deh", "/mods/b.bex"}));

    profile(config).files.clear();
    files(config, {"/mods/a.bex", "/mods/b.hhe", "/mods/c.bex"});
    profile(config).files.front().enabled = false;

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-deh", "/mods/b.hhe"}));
}

TEST_F(ArgumentsOf, APortThatReadsEveryPatchIsHandedThemAll) {
    Config config = on("dsda-doom");

    files(config, {"/mods/b.deh", "/mods/c.deh"});

    EXPECT_TRUE(contains(Arguments::of(config), {"-deh", "/mods/b.deh", "/mods/c.deh"}));
}

TEST_F(ArgumentsOf, EachHelionReleaseIsHandedOnlyTheSwitchesItReads) {
    struct Row {
        const char *version;
        Strings present;
        Strings absent;
    };

    const std::vector<Row> rows = {
            {"", {"-config", "-savedir", "+complevel", "-pistolstart", "-nomusic"}, {}},
            {"0.9.9.0", {"-config", "-savedir", "+complevel", "-pistolstart", "-nomusic"}, {}},
            {"0.9.8.0", {"-config", "-savedir", "-pistolstart", "-nomusic"}, {"+complevel"}},
            {"0.9.6.1", {"-config", "-savedir", "-pistolstart", "-nomusic"}, {"+complevel"}},
            {"0.9.3.0", {"-config", "-savedir"}, {"+complevel", "-pistolstart", "-nomusic"}},
            {"0.9.2.8", {"-config"}, {"+complevel", "-pistolstart", "-nomusic", "-savedir"}},
            {"0.9.2.3", {}, {"+complevel", "-pistolstart", "-nomusic", "-savedir", "-config"}},
    };

    Config config = on("helion");

    config.general.profileConfigs = true;
    profile(config).config = "alpha.cfg";
    profile(config).complevel = 9;
    profile(config).monsters = 2;
    profile(config).setOption(Dialect::Feature::pistolStart, true);
    profile(config).setOption(Dialect::Feature::nomusic, true);

    for (const Row &row : rows) {
        config.engines.front().version = row.version;

        const Strings args = Arguments::of(config);

        for (const std::string &each : row.present) {
            EXPECT_TRUE(has(args, each)) << row.version << " " << each;
        }

        for (const std::string &each : row.absent) {
            EXPECT_FALSE(has(args, each)) << row.version << " " << each;
        }

        // Every release reads the cvar, and only the newest read -fast.
        EXPECT_TRUE(contains(args, {"+sv_fastmonsters", "1"})) << row.version;
        EXPECT_FALSE(has(args, "-fast")) << row.version;
    }
}

TEST_F(ArgumentsOf, HelionIsHandedSoloNetFrom0_9_8_0) {
    Config config = on("helion");

    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Record, .soloNet = true, .file = "run1"};

    for (const char *version : {"", "0.9.8.0"}) {
        config.engines.front().version = version;

        EXPECT_TRUE(has(Arguments::of(config), "-solo-net")) << version;
    }

    config.engines.front().version = "0.9.7.0";

    EXPECT_FALSE(has(Arguments::of(config), "-solo-net"));
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
    profile(config).set(Switch::captureOutput, true);
    profile(config).skill = 3;
    profile(config).monsters = 4;
    profile(config).warp = "MAP07";
    profile(config).setOption(Dialect::Feature::levelstat, true);
    profile(config).setOption(Dialect::Feature::pistolStart, true);
    profile(config).setOption(Dialect::Feature::cheats, true);
    profile(config).setOption(Dialect::Feature::noAutoload, true);
    profile(config).setOption(Dialect::Feature::nomusic, true);
    profile(config).setOption(Dialect::Feature::nosfx, true);
    profile(config).display = Display::Fullscreen;
    profile(config).screenshots = Screenshots::Profile;
    files(config, {"/mods/a.wad", "/mods/b.deh", "/mods/c.bex", "/mods/d.cfg", "/mods/e.lmp"});

    EXPECT_EQ(unknown(Arguments::of(config)), Strings{});

    profile(config).complevel = 21;
    profile(config).replay = {.mode = ReplayMode::Record, .longtics = true, .soloNet = true, .file = "run1"};

    EXPECT_EQ(unknown(Arguments::of(config)), Strings{});

    profile(config).replay = {.mode = ReplayMode::Play, .playback = Playback::Fast, .file = "run1"};

    EXPECT_EQ(unknown(Arguments::of(config)), Strings{});

    profile(config).replay = {.mode = ReplayMode::Continue, .file = "run2", .source = "run1"};
    launch::write(launch::profiles() / "alpha" / "replays" / "run1.lmp", "x");

    EXPECT_EQ(unknown(Arguments::of(config)), Strings{});
}

TEST_F(ArgumentsOf, AnEpisodeMapWithTwoDigitsWarpsOnAPortThatTakesIt) {
    for (const char *file : {"dsda-doom", "prboom-plus", "woof", "nugget-doom", "eternity", "helion"}) {
        Config config = on(file);

        config.iwads.front().file =
                launch::wad(tests::Sandbox::root() / "launch" / "long-episode.wad", "IWAD", {"E1M1", "E1M10"}).string();
        profile(config).warp = "E1M10";

        const Strings args = Arguments::of(config);

        EXPECT_TRUE(contains(args, {"-warp", "1", "10"})) << file;
        EXPECT_FALSE(has(args, "+map")) << file;

        profile(config).warp = "E1M1";

        EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "1", "1"})) << file;
    }
}

TEST_F(ArgumentsOf, AnEpisodeMapWithTwoDigitsGoesToMapOnAPortThatReadsOneDigit) {
    for (const char *file : {"gzdoom", "uzdoom", "zdoom", "zandronum", "skulltag"}) {
        Config config = on(file);

        config.iwads.front().file =
                launch::wad(tests::Sandbox::root() / "launch" / "long-episode.wad", "IWAD", {"E1M1", "E1M10"}).string();
        profile(config).warp = "E1M10";

        const Strings args = Arguments::of(config);

        EXPECT_TRUE(contains(args, {"+map", "E1M10"})) << file;
        EXPECT_FALSE(has(args, "-warp")) << file;

        profile(config).warp = "E1M1";

        EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "1", "1"})) << file;
    }
}

TEST_F(ArgumentsOf, AnEpisodeMapWithTwoDigitsIsDroppedOnAPortWithNeitherWay) {
    for (const char *file : {"chocolate-doom", "crispy-doom", "mbf"}) {
        Config config = on(file);

        config.iwads.front().file =
                launch::wad(tests::Sandbox::root() / "launch" / "long-episode.wad", "IWAD", {"E1M1", "E1M10"}).string();
        profile(config).warp = "E1M10";

        const Strings args = Arguments::of(config);

        EXPECT_FALSE(has(args, "-warp")) << file;
        EXPECT_FALSE(has(args, "+map")) << file;

        profile(config).warp = "E1M1";

        EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "1", "1"})) << file;
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

    profile(config).set(Switch::captureOutput, true);

    EXPECT_TRUE(Arguments::asksForStdout(config, true));
    EXPECT_FALSE(Arguments::asksForStdout(config, false));

    for (const char *file : {"zdoom", "zandronum", "skulltag", "lzdoom"}) {
        config.engines.front().file = (fs::path("/games") / file).string();

        EXPECT_TRUE(Arguments::asksForStdout(config, true)) << file;
    }

    for (const char *file : {"uzdoom", "dsda-doom", "helion", "chocolate-doom"}) {
        config.engines.front().file = (fs::path("/games") / file).string();

        EXPECT_FALSE(Arguments::asksForStdout(config, true)) << file;
    }

    for (const char *file : {"mystery-engine", "doom_gog.exe"}) {
        config.engines.front().file = (fs::path("/games") / file).string();

        ASSERT_FALSE(Dialect::of(config).has(Dialect::Feature::recognised)) << file;
        EXPECT_FALSE(Arguments::asksForStdout(config, true)) << file << " is only guessed to be GZDoom";

        for (const Port::Port picked : {Port::Port::gzdoom, Port::Port::zdoom}) {
            const std::string_view id = Port::known(picked).id;

            config.engines.front().port = picked;

            EXPECT_TRUE(Dialect::of(config).has(Dialect::Feature::recognised)) << file << " picked as " << id;
            EXPECT_TRUE(Arguments::asksForStdout(config, true)) << file << " picked as " << id;
        }

        config.engines.front().port = Port::Port::unknown;
    }

    config.engines.front().file = "/games/gzdoom";
    config.general.autoClose = true;

    EXPECT_FALSE(Arguments::asksForStdout(config, true)) << "auto close captures nothing";

    config.general.autoClose = false;
    config.engines.front().file = "/games/DOOM2.EXE";
    config.engines.front().dosbox = true;
    config.engines.front().port = Port::Port::gzdoom;

    EXPECT_FALSE(Arguments::asksForStdout(config, true)) << "DOSBox captures nothing";
}

TEST_F(ArgumentsOf, TheLineAsksForStdoutExactlyWhenTheRuleSays) {
#ifdef _WIN32
    constexpr bool windows = true;
#else
    constexpr bool windows = false;
#endif

    Config config = on("gzdoom");

    profile(config).set(Switch::captureOutput, true);
    profile(config).extra = "-nomusic";

    const Strings args = Arguments::of(config);

    EXPECT_EQ(has(args, "-stdout"), windows);
    EXPECT_EQ(has(args, "-stdout"), Arguments::asksForStdout(config, windows));

    profile(config).set(Switch::captureOutput, false);

    EXPECT_FALSE(has(Arguments::of(config), "-stdout"));
}

TEST_F(ArgumentsOf, FlagWordsGoOnAsTheirCvars) {
    Config config = on("gzdoom");

    profile(config).setFlag(Flags::Word::dmflags, "1024");
    profile(config).setFlag(Flags::Word::dmflags3, "128");
    profile(config).setFlag(Flags::Word::compatflags2, "0");

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(contains(args, {"+dmflags", "1024", "+dmflags3", "128", "+compatflags2", "0"}));
}

TEST_F(ArgumentsOf, AFlagWordGoesOnInDecimal) {
    Config config = on("gzdoom");

    // The port reads a leading zero as octal.
    profile(config).setFlag(Flags::Word::dmflags, "010");

    EXPECT_TRUE(contains(Arguments::of(config), {"+dmflags", "10"}));
}

TEST_F(ArgumentsOf, AFlagWordThatIsNotANumberIsLeftOut) {
    Config config = on("gzdoom");

    for (const char *typed : {"abc", "0x10", "-4", "1 2", "4294967296"}) {
        profile(config).setFlag(Flags::Word::dmflags, typed);

        EXPECT_FALSE(has(Arguments::of(config), "+dmflags")) << typed;
    }

    profile(config).setFlag(Flags::Word::dmflags, "4294967295");

    EXPECT_TRUE(contains(Arguments::of(config), {"+dmflags", "4294967295"}));
}

TEST_F(ArgumentsOf, AFlagWordThePortDoesNotReadIsLeftOut) {
    Config config = on("zdoom");

    profile(config).setFlag(Flags::Word::dmflags3, "128");
    profile(config).setFlag(Flags::Word::zadmflags, "1");

    const Strings args = Arguments::of(config);

    EXPECT_FALSE(has(args, "+dmflags3"));
    EXPECT_FALSE(has(args, "+zadmflags"));
}

TEST_F(ArgumentsOf, ZandronumTakesItsOwnWords) {
    Config config = on("zandronum");

    profile(config).setFlag(Flags::Word::zadmflags, "64");
    profile(config).setFlag(Flags::Word::lmsWeapons, "3");

    EXPECT_TRUE(contains(Arguments::of(config), {"+zadmflags", "64", "+lmsallowedweapons", "3"}));
}

TEST_F(ArgumentsOf, EternityTakesDmflagsAsASwitch) {
    Config config = on("eternity");

    profile(config).setFlag(Flags::Word::dmflags, "32");
    profile(config).setFlag(Flags::Word::compatflags, "1");

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(contains(args, {"-dmflags", "32"}));
    EXPECT_FALSE(has(args, "+compatflags"));
}

TEST_F(ArgumentsOf, APortWithoutFlagWordsGetsNone) {
    for (const char *file : {"dsda-doom", "woof", "chocolate-doom", "helion"}) {
        Config config = on(file);

        profile(config).setFlag(Flags::Word::dmflags, "1024");

        const Strings args = Arguments::of(config);

        EXPECT_FALSE(has(args, "+dmflags")) << file;
        EXPECT_FALSE(has(args, "-dmflags")) << file;
    }
}

TEST_F(ArgumentsOf, AJoiningPlayerLeavesTheFlagsToTheHost) {
    Config config = on("gzdoom");

    profile(config).setFlag(Flags::Word::dmflags, "1024");
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.host = "10.0.0.7";

    EXPECT_FALSE(has(Arguments::of(config), "+dmflags"));

    profile(config).multiplayer.players = 2;

    EXPECT_TRUE(contains(Arguments::of(config), {"+dmflags", "1024"})) << "the host sets them";
}

TEST_F(ArgumentsOf, FlagWordsComeBeforeThePistolStartCvar) {
    Config config = on("gzdoom");

    profile(config).setFlag(Flags::Word::dmflags3, "0");
    profile(config).setOption(Dialect::Feature::pistolStart, true);

    const Strings args = Arguments::of(config);
    const auto word = std::ranges::find(args, "+dmflags3");
    const auto cvar = std::ranges::find(args, "+sv_pistolstart");

    ASSERT_NE(word, args.end());
    ASSERT_NE(cvar, args.end());
    EXPECT_LT(word, cvar);
}

TEST_F(ArgumentsOf, PistolAndCheatsGoAmongTheOtherOptions) {
    Config config = on("gzdoom");

    profile(config).setOption(Dialect::Feature::noAutoload, true);
    profile(config).setOption(Dialect::Feature::pistolStart, true);
    profile(config).setOption(Dialect::Feature::cheats, true);
    profile(config).setOption(Dialect::Feature::alwaysApplyDmflags, true);
    profile(config).setNumber(Dialect::Feature::turbo, "150");

    const Strings args = Arguments::of(config);
    const auto skip = std::ranges::find(args, "-noautoload");
    const auto pistol = std::ranges::find(args, "+sv_pistolstart");

    ASSERT_NE(skip, args.end());
    ASSERT_NE(pistol, args.end());
    EXPECT_LT(skip, pistol);
    EXPECT_TRUE(
            contains(args, {"+sv_pistolstart", "1", "+sv_cheats", "1", "+alwaysapplydmflags", "1", "-turbo", "150"}));
}

TEST_F(ArgumentsOf, AJoiningPlayerLeavesThePistolAndCheatsToTheHost) {
    Config config = on("gzdoom");

    profile(config).setOption(Dialect::Feature::pistolStart, true);
    profile(config).setOption(Dialect::Feature::cheats, true);
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.host = "10.0.0.7";

    EXPECT_FALSE(has(Arguments::of(config), "+sv_pistolstart"));
    EXPECT_FALSE(has(Arguments::of(config), "+sv_cheats"));

    profile(config).multiplayer.players = 2;

    EXPECT_TRUE(contains(Arguments::of(config), {"+sv_pistolstart", "1", "+sv_cheats", "1"})) << "the host sets them";
}

TEST_F(ArgumentsOf, AJoiningPlayerKeepsTheFileAndSoundOptions) {
    Config config = on("gzdoom");

    profile(config).setOption(Dialect::Feature::noAutoload, true);
    profile(config).setOption(Dialect::Feature::nomusic, true);
    profile(config).setOption(Dialect::Feature::nosfx, true);
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.host = "10.0.0.7";

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(has(args, "-join"));
    EXPECT_TRUE(has(args, "-noautoload"));
    EXPECT_TRUE(has(args, "-nomusic"));
    EXPECT_TRUE(has(args, "-nosfx"));
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
    for (const char *file : {"zdoom", "zandronum"}) {
        Config config = on(file);

        profile(config).setFlag(Flags::Word::compatflags, "3122215895");
        profile(config).setFlag(Flags::Word::compatflags2, "11");

        const Strings args = Arguments::of(config);

        EXPECT_TRUE(contains(args, {"+compatflags", "-1172751401"})) << file;
        EXPECT_TRUE(contains(args, {"+compatflags2", "11"})) << file;
    }

    Config config = on("gzdoom");

    profile(config).setFlag(Flags::Word::compatflags, "3122215895");

    EXPECT_TRUE(contains(Arguments::of(config), {"+compatflags", "3122215895"})) << "strtoll reads it whole";
}

TEST_F(ArgumentsOf, SkulltagTakesDmflags3From0_98cAndNoZandronumWords) {
    Config config = on("skulltag");

    profile(config).setFlag(Flags::Word::dmflags3, "5");
    profile(config).setFlag(Flags::Word::zadmflags, "16");
    profile(config).setFlag(Flags::Word::zacompatflags, "8");
    profile(config).setFlag(Flags::Word::compatflags2, "4");

    for (const char *version : {"", "0.98c"}) {
        config.engines.front().version = version;

        const Strings args = Arguments::of(config);

        EXPECT_TRUE(contains(args, {"+dmflags3", "5"})) << version;
        EXPECT_TRUE(contains(args, {"+compatflags2", "4"})) << version;
        EXPECT_FALSE(has(args, "+zadmflags")) << version;
        EXPECT_FALSE(has(args, "+zacompatflags")) << version;
    }

    config.engines.front().version = "0.98a";

    const Strings older = Arguments::of(config);

    EXPECT_FALSE(has(older, "+dmflags3"));
    EXPECT_TRUE(contains(older, {"+compatflags2", "4"}));
}

TEST_F(ArgumentsOf, AServerLaunchLeavesTheSaveAndTheReplayOut) {
    for (const char *file : {"zandronum", "skulltag"}) {
        Config config = on(file);

        config.general.profileConfigs = true;
        profile(config).config = "alpha.cfg";
        profile(config).save = {.enabled = true, .file = "save3.zds"};
        savedInProfile("save3.zds");

        ASSERT_TRUE(has(Arguments::of(config), "-loadgame")) << file;

        profile(config).multiplayer.gameType = GameType::Coop;
        profile(config).multiplayer.players = 4;

        EXPECT_FALSE(has(Arguments::of(config), "-loadgame")) << file;
        EXPECT_TRUE(contains(Arguments::of(config), {"-host", "4"})) << file;

        profile(config).save = {};
        profile(config).replay = {.mode = ReplayMode::Record, .file = "run1"};

        EXPECT_FALSE(has(Arguments::of(config), "-record")) << file;
        EXPECT_EQ(Arguments::replayOf(config, Dialect::of(config)).leftOut, Arguments::ReplayLeftOut::server) << file;

        profile(config).multiplayer.players = 0;
        profile(config).multiplayer.host = "10.0.0.9";

        EXPECT_TRUE(has(Arguments::of(config), "-record")) << file << " records as a client";
    }
}

TEST_F(ArgumentsOf, AServerTakesTheMapAndSkillButAClientDoesNot) {
    Config config = on("zandronum");

    profile(config).warp = "map07";
    profile(config).skill = 4;
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.players = 4;

    EXPECT_TRUE(contains(Arguments::of(config), {"-skill", "4"}));
    EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "07"}));

    profile(config).multiplayer.players = 0;
    profile(config).multiplayer.host = "10.0.0.9";

    const Strings args = Arguments::of(config);

    EXPECT_FALSE(has(args, "-skill"));
    EXPECT_FALSE(has(args, "-warp"));
    EXPECT_FALSE(has(args, "+map"));
    EXPECT_TRUE(contains(args, {"-connect", "10.0.0.9"}));

    config.engines.front().file = "/games/gzdoom";

    EXPECT_TRUE(contains(Arguments::of(config), {"-skill", "4"}));
}

TEST_F(ArgumentsOf, VkdoomTakesPistolStartFrom25_6_0) {
    Config config = on("vkdoom");

    profile(config).setOption(Dialect::Feature::pistolStart, true);

    for (const char *version : {"", "25.6.0"}) {
        config.engines.front().version = version;

        EXPECT_TRUE(contains(Arguments::of(config), {"+sv_pistolstart", "1"})) << version;
    }

    config.engines.front().version = "0.9";

    EXPECT_FALSE(has(Arguments::of(config), "+sv_pistolstart"));
}

TEST_F(ArgumentsOf, AnOptionTurnedOnGoesOnTheLine) {
    Config config = on("crispy-doom");

    profile(config).setOption(Dialect::Feature::coopSpawns, true);
    profile(config).setOption(Dialect::Feature::flipLevels, true);

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(has(args, "-coop_spawns"));
    EXPECT_TRUE(has(args, "-fliplevels"));
}

TEST_F(ArgumentsOf, AnOptionCarriesItsOwnValue) {
    Config config = on("gzdoom");

    profile(config).setOption(Dialect::Feature::alwaysApplyDmflags, true);

    EXPECT_TRUE(contains(Arguments::of(config), {"+alwaysapplydmflags", "1"}));
}

TEST_F(ArgumentsOf, ANumberGoesOnOnlyInsideThePortsRange) {
    Config config = on("dsda-doom");

    profile(config).setNumber(Dialect::Feature::turbo, "150");
    profile(config).setNumber(Dialect::Feature::gameSpeed, "200");

    EXPECT_TRUE(contains(Arguments::of(config), {"-turbo", "150", "-game_speed", "200"}));

    profile(config).setNumber(Dialect::Feature::turbo, "300");

    EXPECT_FALSE(has(Arguments::of(config), "-turbo"));

    Config woof = on("woof");

    profile(woof).setNumber(Dialect::Feature::turbo, "300");
    profile(woof).setNumber(Dialect::Feature::gameSpeed, "200");

    EXPECT_TRUE(contains(Arguments::of(woof), {"-turbo", "300", "-speed", "200"}));
}

TEST_F(ArgumentsOf, APortGetsNoOptionItLacks) {
    Config config = on("dsda-doom");

    profile(config).setNumber(Dialect::Feature::dogs, "2");

    for (const Dialect::Feature option : {
                 Dialect::Feature::mbfBeta,
                 Dialect::Feature::doubleAmmo,
                 Dialect::Feature::coopPreset,
                 Dialect::Feature::alwaysApplyDmflags,
                 Dialect::Feature::vanillaMode,
         }) {
        profile(config).setOption(option, true);
    }

    const Strings args = Arguments::of(config);

    for (const char *flag : {"-dogs", "-beta", "-doubleammo", "-coop", "+alwaysapplydmflags", "-vanilla"}) {
        EXPECT_FALSE(has(args, flag)) << flag;
    }
}

TEST_F(ArgumentsOf, DoubleAmmoIsLeftOutWhereCrispyStopsOnIt) {
    Config config = on("crispy-doom");

    profile(config).config = "alpha.cfg";
    profile(config).setOption(Dialect::Feature::doubleAmmo, true);

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
    for (const char *file : {"woof", "crispy-doom"}) {
        Config config = on(file);

        profile(config).multiplayer.gameType = GameType::Deathmatch3;
        profile(config).multiplayer.players = 2;

        EXPECT_TRUE(has(Arguments::of(config), "-dm3")) << file;
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

    profile(config).setOption(Dialect::Feature::mbfBeta, true);

    EXPECT_FALSE(has(Arguments::of(config), "-beta")) << "the port's own is MBF21, where Woof stops";

    profile(config).complevel = 21;

    EXPECT_FALSE(has(Arguments::of(config), "-beta"));

    profile(config).complevel = 11;

    EXPECT_TRUE(has(Arguments::of(config), "-beta"));
}

TEST_F(ArgumentsOf, HelperDogsAreLeftOutBelowMbfOnWoof) {
    Config config = on("woof");

    profile(config).setNumber(Dialect::Feature::dogs, "2");

    EXPECT_TRUE(contains(Arguments::of(config), {"-dogs", "2"})) << "the port's own counts";

    profile(config).complevel = 9;

    EXPECT_FALSE(has(Arguments::of(config), "-dogs"));

    profile(config).complevel = 11;

    EXPECT_TRUE(contains(Arguments::of(config), {"-dogs", "2"}));

    Config prboom = on("prboom-plus");

    profile(prboom).setNumber(Dialect::Feature::dogs, "2");
    profile(prboom).complevel = 9;

    EXPECT_TRUE(contains(Arguments::of(prboom), {"-dogs", "2"})) << "PrBoom+ takes them at any level";
}

TEST_F(ArgumentsOf, AJoiningPlayerLeavesTheRulesToTheHost) {
    Config config = on("woof");

    profile(config).setOption(Dialect::Feature::coopSpawns, true);
    profile(config).setNumber(Dialect::Feature::turbo, "150");
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.host = "10.0.0.7";

    const Strings args = Arguments::of(config);

    EXPECT_FALSE(has(args, "-coop_spawns"));
    EXPECT_FALSE(has(args, "-turbo"));
}

TEST_F(ArgumentsOf, DoubleAmmoIsLeftOutWhileTheProfilePlaysItsDemo) {
    Config config = on("crispy-doom");

    profile(config).config = "alpha.cfg";
    profile(config).setOption(Dialect::Feature::doubleAmmo, true);
    profile(config).replay = {.mode = ReplayMode::Play, .file = "run1"};
    launch::write(launch::profiles() / "alpha" / "replays" / "run1.lmp", "x");

    EXPECT_FALSE(has(Arguments::of(config), "-doubleammo"));
}

TEST_F(ArgumentsOf, ChocolateHereticHexenAndStrifeGetNoDoomRules) {
    for (const char *file : {"chocolate-heretic", "chocolate-hexen", "chocolate-strife", "crispy-heretic"}) {
        Config config = on(file);

        profile(config).complevel = Dialect::GAME_VERSIONS + 5;
        profile(config).setNumber(Dialect::Feature::turbo, "150");
        profile(config).multiplayer.gameType = GameType::Deathmatch3;
        profile(config).multiplayer.players = 2;

        const Strings args = Arguments::of(config);

        EXPECT_FALSE(has(args, "-gameversion")) << file;
        EXPECT_FALSE(has(args, "-turbo")) << file;
        EXPECT_FALSE(has(args, "-dm3")) << file;
    }
}

namespace {

Dialect::Traits released(const std::string &version, const char *file = "dsda-doom") {
    return Dialect::of(Engine{.name = "Port", .file = (fs::path("/games") / file).string(), .version = version});
}

Dialect::Traits named(const char *file) {
    return Dialect::of(fs::path("/games") / file);
}

Profile recording(const ReplayMode mode, const int complevel = -1, const int skill = 0) {
    Profile profile;

    profile.replay = {.mode = mode, .file = "run"};
    profile.complevel = complevel;
    profile.skill = skill;

    return profile;
}

}

TEST(ArgumentsNeedsComplevel, RecordingWithoutOneStopsThePortsThatAskForIt) {
    EXPECT_TRUE(Arguments::needsComplevel(recording(ReplayMode::Record), named("dsda-doom")));
    EXPECT_TRUE(Arguments::needsComplevel(recording(ReplayMode::Record), named("woof")));
}

TEST(ArgumentsNeedsComplevel, ALevelThePortReadsIsEnough) {
    EXPECT_FALSE(Arguments::needsComplevel(recording(ReplayMode::Record, 21), named("dsda-doom")));
    EXPECT_FALSE(Arguments::needsComplevel(recording(ReplayMode::Record, 9), named("woof")));
}

TEST(ArgumentsNeedsComplevel, AnUnreadLevelCountsAsNone) {
    EXPECT_TRUE(Arguments::needsComplevel(recording(ReplayMode::Record, 5), named("woof")));
    EXPECT_TRUE(Arguments::needsComplevel(recording(ReplayMode::Record, 99), named("dsda-doom")));
}

TEST(ArgumentsNeedsComplevel, AnUnrecordableLevelIsNoBetterThanNone) {
    EXPECT_TRUE(Arguments::needsComplevel(recording(ReplayMode::Record, Dialect::ID24), named("woof")));
    EXPECT_FALSE(Arguments::needsComplevel(recording(ReplayMode::Record, Dialect::MBF21), named("woof")));
}

TEST(ArgumentsNeedsComplevel, OnlyARecordingWithANameIsAsked) {
    Profile unnamed = recording(ReplayMode::Record);

    unnamed.replay.file.clear();

    EXPECT_FALSE(Arguments::needsComplevel(unnamed, named("dsda-doom")));
    EXPECT_FALSE(Arguments::needsComplevel(recording(ReplayMode::Play), named("dsda-doom")));
    EXPECT_FALSE(Arguments::needsComplevel(recording(ReplayMode::Continue), named("dsda-doom")));
    EXPECT_FALSE(Arguments::needsComplevel(recording(ReplayMode::Off), named("dsda-doom")));
}

TEST(ArgumentsNeedsComplevel, APortThatRecordsWithoutOneIsNeverAsked) {
    EXPECT_FALSE(Arguments::needsComplevel(recording(ReplayMode::Record), named("prboom-plus")));
    EXPECT_FALSE(Arguments::needsComplevel(recording(ReplayMode::Record), named("gzdoom")));
    EXPECT_FALSE(Arguments::needsComplevel(recording(ReplayMode::Record), Dialect::defined({})));
}

TEST(ArgumentsNeedsComplevel, CherryAndTheWoofReleasesBefore16NeverAsk) {
    EXPECT_FALSE(Arguments::needsComplevel(recording(ReplayMode::Record), named("cherry-doom")));
    EXPECT_FALSE(Arguments::needsComplevel(recording(ReplayMode::Record), released("15.0.0", "woof")));
    EXPECT_FALSE(Arguments::needsComplevel(recording(ReplayMode::Record), released("4.0.0", "nugget-doom")));
    EXPECT_TRUE(Arguments::needsComplevel(recording(ReplayMode::Record), released("16.0.0", "woof")));
    EXPECT_TRUE(Arguments::needsComplevel(recording(ReplayMode::Record), released("6.0.0", "nugget-doom")));
}

TEST(ArgumentsNeedsComplevel, DsdaAsksFrom0_25_0) {
    for (const char *version : {"0.30.0", "0.25.0"}) {
        EXPECT_TRUE(Arguments::needsComplevel(recording(ReplayMode::Record), released(version))) << version;
    }

    for (const char *version : {"0.22.3", "0.21.1"}) {
        EXPECT_FALSE(Arguments::needsComplevel(recording(ReplayMode::Record), released(version))) << version;
    }
}

TEST(ArgumentsNeedsComplevel, ALevelOnTheLineEndsTheAsk) {
    Config config = launch::single(fs::path("/games") / "dsda-doom");

    config.activeProfile().replay = {.mode = ReplayMode::Record, .file = "run"};

    EXPECT_TRUE(Arguments::needsComplevel(config.activeProfile(), Dialect::of(config)));
    EXPECT_FALSE(has(Arguments::of(config), "-complevel"));

    config.activeProfile().complevel = 21;

    EXPECT_FALSE(Arguments::needsComplevel(config.activeProfile(), Dialect::of(config)));
    EXPECT_TRUE(contains(Arguments::of(config), {"-complevel", "21"}));
}

TEST(ArgumentsNeedsSkill, RecordingWithoutOneStopsThePortsThatAskForIt) {
    for (const char *file : {"dsda-doom", "woof", "nugget-doom", "nyan-doom"}) {
        EXPECT_TRUE(Arguments::needsSkill(recording(ReplayMode::Record), named(file))) << file;
    }
}

TEST(ArgumentsNeedsSkill, APickedSkillIsEnough) {
    EXPECT_FALSE(Arguments::needsSkill(recording(ReplayMode::Record, -1, 4), named("dsda-doom")));
    EXPECT_FALSE(Arguments::needsSkill(recording(ReplayMode::Continue, -1, 1), released("0.27.0")));
}

TEST(ArgumentsNeedsSkill, OnlyARecordingWithANameIsAsked) {
    Profile unnamed = recording(ReplayMode::Record);

    unnamed.replay.file.clear();

    EXPECT_FALSE(Arguments::needsSkill(unnamed, named("dsda-doom")));
    EXPECT_FALSE(Arguments::needsSkill(recording(ReplayMode::Play), named("dsda-doom")));
    EXPECT_FALSE(Arguments::needsSkill(recording(ReplayMode::Off), named("dsda-doom")));
}

TEST(ArgumentsNeedsSkill, APortThatRecordsWithoutOneIsNeverAsked) {
    EXPECT_FALSE(Arguments::needsSkill(recording(ReplayMode::Record), named("prboom-plus")));
    EXPECT_FALSE(Arguments::needsSkill(recording(ReplayMode::Record), named("cherry-doom")));
    EXPECT_FALSE(Arguments::needsSkill(recording(ReplayMode::Record), named("gzdoom")));
    EXPECT_FALSE(Arguments::needsSkill(recording(ReplayMode::Record), Dialect::defined({})));
}

TEST(ArgumentsNeedsSkill, TheWoofLineAsksFromWoof16AndNugget6) {
    EXPECT_TRUE(Arguments::needsSkill(recording(ReplayMode::Record), released("16.0.0", "woof")));
    EXPECT_TRUE(Arguments::needsSkill(recording(ReplayMode::Record), released("6.0.0", "nugget-doom")));
    EXPECT_FALSE(Arguments::needsSkill(recording(ReplayMode::Record), released("15.0.0", "woof")));
    EXPECT_FALSE(Arguments::needsSkill(recording(ReplayMode::Record), released("4.0.0", "nugget-doom")));
}

TEST(ArgumentsNeedsSkill, DsdaAsksRecordingFrom0_27_0) {
    for (const char *version : {"0.30.0", "0.28.0", "0.27.0"}) {
        EXPECT_TRUE(Arguments::needsSkill(recording(ReplayMode::Record), released(version))) << version;
    }

    for (const char *version : {"0.26.0", "0.25.0", "0.22.3", "0.21.1"}) {
        EXPECT_FALSE(Arguments::needsSkill(recording(ReplayMode::Record), released(version))) << version;
    }
}

TEST(ArgumentsNeedsSkill, DsdaAsksContinuingOnlyBefore0_28_0) {
    EXPECT_TRUE(Arguments::needsSkill(recording(ReplayMode::Continue), released("0.27.0")));

    for (const char *version : {"0.30.0", "0.28.0", "0.26.0"}) {
        EXPECT_FALSE(Arguments::needsSkill(recording(ReplayMode::Continue), released(version))) << version;
    }

    EXPECT_FALSE(Arguments::needsSkill(recording(ReplayMode::Continue), named("dsda-doom")));
}

TEST(ArgumentsNeedsSkill, ASkillOnTheLineEndsTheAsk) {
    Config config = launch::single(fs::path("/games") / "dsda-doom");

    config.activeProfile().replay = {.mode = ReplayMode::Record, .file = "run"};

    EXPECT_TRUE(Arguments::needsSkill(config.activeProfile(), Dialect::of(config)));
    EXPECT_FALSE(has(Arguments::of(config), "-skill"));

    config.activeProfile().skill = 4;

    EXPECT_FALSE(Arguments::needsSkill(config.activeProfile(), Dialect::of(config)));
    EXPECT_TRUE(contains(Arguments::of(config), {"-skill", "4"}));
}
