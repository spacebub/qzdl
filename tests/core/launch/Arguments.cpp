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
#include <span>
#include <string>
#include <tuple>
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

    // -warp 5 reaches MAP02. MAP01 and MAP03 go by their own numbers.
    static const fs::path &hexen() {
        static const fs::path path = launch::withMapInfo(tests::Sandbox::root() / "launch" / "hexen.wad",
                                                         "IWAD",
                                                         {"MAP01", "MAP02", "MAP03"},
                                                         "map 1 \"Winnowing Hall\"\nwarptrans 1\n"
                                                         "map 2 \"Seven Portals\"\nwarptrans 5\n"
                                                         "map 3 \"Guardian of Ice\"\n");

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

// A ZDoom port reads the format of a patch from the content. ZDoom before 2.4 drops every -bex after a -deh.
TEST_F(ArgumentsOf, ZdoomPortsTakeEveryPatchBehindOneDehInListOrder) {
    const auto line = [](const char *port, const char *version, const Strings &patches) {
        Config config = on(port);

        config.engines.front().version = version;
        files(config, patches);

        return Arguments::of(config);
    };

    for (const char *port : {"gzdoom", "uzdoom", "lzdoom", "zdoom", "zandronum", "skulltag"}) {
        EXPECT_EQ(line(port, "", {"/mods/a.bex", "/mods/b.deh"}),
                  (Strings{"-iwad", iwad(), "-deh", "/mods/a.bex", "/mods/b.deh"}))
                << port;
        EXPECT_EQ(line(port, "", {"/mods/a.deh", "/mods/b.bex", "/mods/c.deh"}),
                  (Strings{"-iwad", iwad(), "-deh", "/mods/a.deh", "/mods/b.bex", "/mods/c.deh"}))
                << port;
    }

    EXPECT_EQ(line("zdoom", "2.1.0", {"/mods/a.deh", "/mods/b.bex"}),
              (Strings{"-iwad", iwad(), "-deh", "/mods/a.deh", "/mods/b.bex"}));
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

TEST_F(ArgumentsOf, EternityReadsEveryPatchBehindOneDeh) {
    const auto line = [](const Strings &patches) {
        Config config = on("eternity");

        files(config, patches);

        return Arguments::of(config);
    };

    EXPECT_EQ(line({"/mods/a.bex", "/mods/b.deh"}), (Strings{"-iwad", iwad(), "-deh", "/mods/a.bex", "/mods/b.deh"}));
    EXPECT_EQ(line({"/mods/b.deh", "/mods/a.bex"}), (Strings{"-iwad", iwad(), "-deh", "/mods/b.deh", "/mods/a.bex"}));
    EXPECT_EQ(line({"/mods/a.deh", "/mods/b.bex", "/mods/c.deh"}),
              (Strings{"-iwad", iwad(), "-deh", "/mods/a.deh", "/mods/b.bex", "/mods/c.deh"}));
    EXPECT_EQ(line({"/mods/a.bex", "/mods/b.bex"}), (Strings{"-iwad", iwad(), "-deh", "/mods/a.bex", "/mods/b.bex"}));
}

TEST_F(ArgumentsOf, APortWithOneSwitchForBothKindsAppliesThePatchesInListOrder) {
    for (const char *port :
         {"prboom-plus", "dsda-doom", "woof", "nugget-doom", "cherry-doom", "chocolate-doom", "crispy-doom"}) {
        Config config = on(port);

        files(config, {"/mods/a.deh", "/mods/b.bex", "/mods/c.deh"});

        EXPECT_EQ(Arguments::of(config),
                  (Strings{"-iwad", iwad(), "-deh", "/mods/a.deh", "/mods/b.bex", "/mods/c.deh"}))
                << port;
    }
}

TEST_F(ArgumentsOf, DoomLegacyPatchesWithDehacked) {
    Config config = on("doomlegacy");

    files(config, {"/mods/a.deh"});

    EXPECT_EQ(Arguments::of(config), (Strings{"-dehacked", "/mods/a.deh"}));

    files(config, {"/mods/b.bex", "/mods/c.deh"});

    EXPECT_EQ(Arguments::of(config), (Strings{"-dehacked", "/mods/a.deh", "/mods/b.bex", "/mods/c.deh"}));
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

TEST_F(ArgumentsOf, RecordingWritesIntoTheReplaysFolderWithTheWholeName) {
    Config config = on("gzdoom");

    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Record, .file = "run1"};

    EXPECT_TRUE(contains(Arguments::of(config),
                         {"-record", (launch::profiles() / "alpha" / "replays" / "run1.lmp").string()}));
}

TEST_F(ArgumentsOf, RecordingLeavesLmpOffForAPortThatAlwaysAddsIt) {
    Config config = on("chocolate-doom");

    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Record, .file = "run1"};

    EXPECT_TRUE(
            contains(Arguments::of(config), {"-record", (launch::profiles() / "alpha" / "replays" / "run1").string()}));
}

TEST_F(ArgumentsOf, ADottedDemoNameGoesWithoutLmpOnlyToAPortThatAppendsLmpOrNumbersTheNameAsGiven) {
    struct Case {
        const char *file;
        const char *version;
        bool whole;
    };

    const std::vector<Case> cases = {
            {"chocolate-doom", "", false},
            {"crispy-doom", "", false},
            {"woof", "", true},
            {"woof", "10.2.0", true},
            {"woof", "9.0.0", false},
            {"woof", "7.0.0", false},
            {"woof", "6.1.0", true},
            {"nugget-doom", "1.10.0", true},
            {"nugget-doom", "1.9.0", false},
            {"nugget-doom", "1.3.0", false},
            {"nugget-doom", "1.0.0", true},
            {"cherry-doom", "", true},
            {"prboom-plus", "", true},
            {"prboom", "", true},
            {"dsda-doom", "", true},
            {"nyan-doom", "", true},
            {"eternity", "", true},
            {"gzdoom", "", true},
            {"zandronum", "", true},
            {"helion", "", true},
    };

    const fs::path replays = launch::profiles() / "alpha" / "replays";

    for (const Case &each : cases) {
        Config config = on(each.file);

        config.engines.front().version = each.version;
        profile(config).config = "alpha.cfg";
        profile(config).replay = {.mode = ReplayMode::Record, .file = "e1m1.max"};

        const Strings args = Arguments::of(config);
        const std::ptrdiff_t record = at(args, "-record");

        ASSERT_LT(record + 1, std::ssize(args)) << each.file << ' ' << each.version;
        EXPECT_EQ(args[static_cast<size_t>(record + 1)],
                  (replays / (each.whole ? "e1m1.max.lmp" : "e1m1.max")).string())
                << each.file << ' ' << each.version;
    }
}

TEST_F(ArgumentsOf, Woof9RecordsWithoutLmpAndWoofNowWithIt) {
    Config config = on("woof");
    const fs::path replays = launch::profiles() / "alpha" / "replays";

    profile(config).config = "alpha.cfg";
    profile(config).skill = 4;
    profile(config).complevel = 21;
    profile(config).replay = {.mode = ReplayMode::Record, .file = "run1"};

    EXPECT_EQ(
            Arguments::of(config),
            (Strings{"-iwad", iwad(), "-skill", "4", "-complevel", "21", "-record", (replays / "run1.lmp").string()}));

    config.engines.front().version = "9.0.0";

    EXPECT_EQ(Arguments::of(config),
              (Strings{"-iwad", iwad(), "-skill", "4", "-complevel", "21", "-record", (replays / "run1").string()}));
}

TEST_F(ArgumentsOf, PrboomPlusRecordsIntoAnAbsoluteNameWithLmp) {
    Config config = on("prboom-plus");

    profile(config).replay = {.mode = ReplayMode::Record, .file = "/demos/run1"};

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-record", "/demos/run1.lmp"}));

    config.engines.front().file = "/games/chocolate-doom";

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-record", "/demos/run1"}));
}

TEST_F(ArgumentsOf, SoloNetGoesAheadOfPlayback) {
    const fs::path replays = launch::profiles() / "alpha" / "replays";

    launch::write(replays / "run1.lmp", "x");

    for (const char *file : {"chocolate-doom", "dsda-doom"}) {
        Config config = on(file);

        profile(config).config = "alpha.cfg";
        profile(config).replay = {.mode = ReplayMode::Play, .soloNet = true, .file = "run1"};

        EXPECT_EQ(Arguments::of(config),
                  (Strings{"-iwad", iwad(), "-solo-net", "-playdemo", (replays / "run1.lmp").string()}))
                << file;
    }
}

TEST_F(ArgumentsOf, SoloNetGoesAheadOfContinue) {
    const fs::path replays = launch::profiles() / "alpha" / "replays";

    launch::write(replays / "run1.lmp", "x");

    Config config = on("dsda-doom");

    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Continue, .soloNet = true, .file = "run2", .source = "run1"};

    EXPECT_EQ(Arguments::of(config),
              (Strings{"-iwad",
                       iwad(),
                       "-complevel",
                       "0",
                       "-solo-net",
                       "-recordfromto",
                       (replays / "run1.lmp").string(),
                       (replays / "run2.lmp").string()}));

    config.engines.front().file = "/games/chocolate-doom";

    EXPECT_FALSE(has(Arguments::of(config), "-solo-net")) << "Chocolate Doom continues nothing";
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
                          (launch::profiles() / "alpha" / "replays" / "run1.lmp").string()}));
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

TEST_F(ArgumentsOf, ThePrboomLineRecordsLongticsOnlyUpToBoomCompatibility) {
    for (const char *file : {"prboom-plus", "prboom"}) {
        Config config = on(file);

        profile(config).config = "alpha.cfg";
        profile(config).replay = {.mode = ReplayMode::Record, .longtics = true, .file = "run1"};

        EXPECT_TRUE(has(Arguments::of(config), "-longtics")) << file << ": the port's own counts";

        profile(config).complevel = 7;

        EXPECT_TRUE(has(Arguments::of(config), "-longtics")) << file;

        profile(config).complevel = 8;

        EXPECT_FALSE(has(Arguments::of(config), "-longtics")) << file;
        EXPECT_TRUE(has(Arguments::of(config), "-record")) << file;
    }

    Config dsda = on("dsda-doom");

    profile(dsda).config = "alpha.cfg";
    profile(dsda).complevel = 9;
    profile(dsda).replay = {.mode = ReplayMode::Record, .longtics = true, .file = "run1"};

    EXPECT_TRUE(has(Arguments::of(dsda), "-longtics")) << "DSDA-Doom sets no levels";
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

TEST_F(ArgumentsOf, ThePrboomLineLoadsASaveBySlotUpTo255) {
    for (const auto &[file, prefix] :
         {std::pair{"prboom-plus", "prboom-plus-savegame"}, std::pair{"prboom", "prbmsav"}}) {
        Config config = on(file);

        profile(config).save = {.enabled = true, .file = saved(std::string(prefix) + "255.dsg")};

        EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "255"})) << file;

        profile(config).save.file = saved(std::string(prefix) + "256.dsg");

        EXPECT_FALSE(has(Arguments::of(config), "-loadgame")) << file << ": the slot would wrap to 0";
    }
}

TEST_F(ArgumentsOf, TheOriginalPrboomLoadsOnlyItsOwnSaves) {
    Config config = on("prboom");

    profile(config).save = {.enabled = true, .file = saved("prbmsav3.dsg")};

    EXPECT_TRUE(contains(Arguments::of(config), {"-loadgame", "3"}));

    profile(config).save.file = saved("prboom-plus-savegame3.dsg");

    EXPECT_FALSE(has(Arguments::of(config), "-loadgame"));
}

TEST_F(ArgumentsOf, TheOriginalPrboomIsHandedNoSwitchThatOnlyPrboomPlusReads) {
    const auto line = [](const char *file, const ReplayMode mode) {
        Config config = on(file);

        profile(config).config = "alpha.cfg";
        profile(config).screenshots = Screenshots::Profile;
        profile(config).setOption(Dialect::Feature::levelstat, true);
        profile(config).setOption(Dialect::Feature::pistolStart, true);
        profile(config).setOption(Dialect::Feature::coopSpawns, true);
        profile(config).setOption(Dialect::Feature::stroller, true);
        profile(config).setOption(Dialect::Feature::noCheats, true);
        profile(config).replay = {.mode = mode, .file = "run2", .source = "run1"};

        return Arguments::of(config);
    };

    for (const char *only : {"-levelstat", "-pistolstart", "-coop_spawns", "-stroller", "-nocheats", "-shotdir"}) {
        EXPECT_TRUE(has(line("prboom-plus", ReplayMode::Off), only)) << only;
        EXPECT_FALSE(has(line("prboom", ReplayMode::Off), only)) << only;
        EXPECT_FALSE(has(line("glboom", ReplayMode::Off), only)) << only;
    }

    EXPECT_TRUE(has(line("prboom-plus", ReplayMode::Continue), "-recordfromto"));
    EXPECT_FALSE(has(line("prboom", ReplayMode::Continue), "-recordfromto"));
    EXPECT_TRUE(has(line("prboom", ReplayMode::Record), "-record"));
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

TEST_F(ArgumentsOf, EternityWarpsToANumberOrFrom4_01_00ToAName) {
    Config config = on("eternity");

    files(config, {launch::wad(tests::Sandbox::root() / "launch" / "named.wad", "PWAD", {"START"}).string()});
    profile(config).warp = "MAP29";

    EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "29"}));

    profile(config).warp = "START";

    EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "START"}));
    EXPECT_FALSE(has(Arguments::of(config), "+map"));

    config.iwads.front().file = episodic().string();
    profile(config).warp = "E2M3";

    EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "2", "3"}));

    config.engines.front().version = "4.00.00";

    EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "2", "3"}));

    profile(config).warp = "START";

    EXPECT_FALSE(has(Arguments::of(config), "-warp"));
    EXPECT_FALSE(has(Arguments::of(config), "START"));
}

TEST_F(ArgumentsOf, EternityIsHandedOnlyTheSwitchesItsReleaseReads) {
    const auto line = [](const char *version, const ReplayMode mode, const Display display) {
        Config config = on("eternity");

        config.engines.front().version = version;
        profile(config).config = "alpha.cfg";
        profile(config).display = display;
        profile(config).setOption(Dialect::Feature::vanillaMode, true);
        profile(config).replay = {.mode = mode, .file = "run2", .source = "run1"};

        return Arguments::of(config);
    };

    const fs::path replays = launch::profiles() / "alpha" / "replays";

    launch::write(replays / "run1.lmp", "x");

    for (const char *version : {"4.06.00", "4.01.00"}) {
        EXPECT_TRUE(contains(line(version, ReplayMode::Continue, Display::Port),
                             {"-recordfromto", (replays / "run1.lmp").string(), (replays / "run2.lmp").string()}))
                << version;
    }

    EXPECT_FALSE(has(line("4.00.00", ReplayMode::Continue, Display::Port), "-recordfromto"));

    EXPECT_FALSE(has(line("4.02.00", ReplayMode::Off, Display::Fullscreen), "-fullscreen"));
    EXPECT_FALSE(has(line("4.02.00", ReplayMode::Off, Display::Window), "-window"));

    for (const char *version : {"4.01.00", "3.40.00"}) {
        EXPECT_TRUE(has(line(version, ReplayMode::Off, Display::Fullscreen), "-fullscreen")) << version;
        EXPECT_TRUE(has(line(version, ReplayMode::Off, Display::Window), "-window")) << version;
    }

    EXPECT_TRUE(has(line("3.40.10", ReplayMode::Off, Display::Port), "-vanilla"));
    EXPECT_FALSE(has(line("3.40.00", ReplayMode::Off, Display::Port), "-vanilla"));
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

    EXPECT_TRUE(contains(args, {"-recordfromto", (replays / "run1.lmp").string(), (replays / "run2.lmp").string()}));
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

TEST_F(ArgumentsOf, AnOlderChocolatePlaysAndRecordsWithAGameVersionOnlyWhenItReadsIt) {
    Config config = on("chocolate-doom");

    profile(config).config = "alpha.cfg";
    launch::write(launch::profiles() / "alpha" / "replays" / "run1.lmp", "x");

    config.engines.front().version = "3.0.1";
    profile(config).complevel = Dialect::GAME_VERSIONS + 5;
    profile(config).replay = {.mode = ReplayMode::Play, .file = "run1"};

    const Strings played = Arguments::of(config);

    EXPECT_TRUE(has(played, "-playdemo"));
    EXPECT_TRUE(contains(played, {"-gameversion", "1.9"}));

    profile(config).replay = {.mode = ReplayMode::Record, .file = "run2"};

    const Strings recorded = Arguments::of(config);

    EXPECT_TRUE(has(recorded, "-record"));
    EXPECT_TRUE(contains(recorded, {"-gameversion", "1.9"}));

    config.engines.front().version = "3.1.0";
    profile(config).complevel = Dialect::GAME_VERSIONS + 1;

    const Strings unread = Arguments::of(config);

    EXPECT_TRUE(has(unread, "-record"));
    EXPECT_FALSE(has(unread, "-gameversion"));

    profile(config).replay = {.mode = ReplayMode::Play, .file = "run1"};

    const Strings unreadPlayed = Arguments::of(config);

    EXPECT_TRUE(has(unreadPlayed, "-playdemo"));
    EXPECT_FALSE(has(unreadPlayed, "-gameversion"));
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
}

TEST_F(ArgumentsOf, HelperDogsAreLeftOutBelowMbfOnThePrboomLine) {
    for (const char *file : {"prboom-plus", "glboom-plus", "prboom", "glboom"}) {
        Config config = on(file);

        profile(config).setNumber(Dialect::Feature::dogs, "2");

        EXPECT_TRUE(contains(Arguments::of(config), {"-dogs", "2"})) << file << ": the port's own counts";

        profile(config).complevel = 10;

        EXPECT_FALSE(has(Arguments::of(config), "-dogs")) << file;

        profile(config).complevel = 11;

        EXPECT_TRUE(contains(Arguments::of(config), {"-dogs", "2"})) << file;

        profile(config).complevel = 17;

        EXPECT_TRUE(contains(Arguments::of(config), {"-dogs", "2"})) << file;
    }
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

TEST_F(ArgumentsOf, TheOtherGamesAreHandedNoGameVersionAndNoDeathmatch3) {
    for (const char *file : {"chocolate-heretic",
                             "chocolate-hexen",
                             "chocolate-strife",
                             "crispy-heretic",
                             "crispy-hexen",
                             "crispy-strife"}) {
        Config config = on(file);

        profile(config).complevel = Dialect::GAME_VERSIONS + 5;
        profile(config).multiplayer.gameType = GameType::Deathmatch3;
        profile(config).multiplayer.players = 2;

        const Strings args = Arguments::of(config);

        EXPECT_FALSE(has(args, "-gameversion")) << file;
        EXPECT_FALSE(has(args, "-dm3")) << file;
    }
}

TEST_F(ArgumentsOf, OfTheOtherGamesOnlyStrifeTakesTurbo) {
    for (const char *file : {"chocolate-heretic", "chocolate-hexen", "crispy-heretic", "crispy-hexen"}) {
        Config config = on(file);

        profile(config).setNumber(Dialect::Feature::turbo, "150");

        EXPECT_FALSE(has(Arguments::of(config), "-turbo")) << file;
    }

    for (const char *file : {"chocolate-strife", "crispy-strife"}) {
        Config config = on(file);

        profile(config).setNumber(Dialect::Feature::turbo, "300");

        EXPECT_TRUE(contains(Arguments::of(config), {"-turbo", "300"})) << file;
    }
}

namespace {

Strings hosted(Config config, const GameType type) {
    config.activeProfile().multiplayer.gameType = type;
    config.activeProfile().multiplayer.players = 2;

    return Arguments::of(config);
}

}

TEST_F(ArgumentsOf, HereticAndHexenHostAnAltDeathmatchAsADeathmatch) {
    for (const char *file : {"chocolate-heretic", "chocolate-hexen", "crispy-heretic", "crispy-hexen"}) {
        const Strings alt = hosted(on(file), GameType::AltDeathmatch);

        EXPECT_TRUE(has(alt, "-deathmatch")) << file;
        EXPECT_FALSE(has(alt, "-altdeath")) << file;
        EXPECT_TRUE(has(hosted(on(file), GameType::Deathmatch), "-deathmatch")) << file;
        EXPECT_FALSE(has(hosted(on(file), GameType::Coop), "-deathmatch")) << file;
    }
}

// A Strife netgame is always a deathmatch, and -altdeath makes the items respawn.
TEST_F(ArgumentsOf, StrifeHostsADeathmatchWithNoSwitch) {
    for (const char *file : {"chocolate-strife", "crispy-strife"}) {
        for (const GameType type :
             {GameType::Coop, GameType::Deathmatch, GameType::Deathmatch3, GameType::TeamDeathmatch}) {
            const Strings args = hosted(on(file), type);

            EXPECT_TRUE(has(args, "-privateserver")) << file << ' ' << static_cast<int>(type);
            EXPECT_FALSE(has(args, "-deathmatch")) << file << ' ' << static_cast<int>(type);
            EXPECT_FALSE(has(args, "-altdeath")) << file << ' ' << static_cast<int>(type);
            EXPECT_FALSE(has(args, "-dm3")) << file << ' ' << static_cast<int>(type);
        }

        const Strings alt = hosted(on(file), GameType::AltDeathmatch);

        EXPECT_TRUE(has(alt, "-altdeath")) << file;
        EXPECT_FALSE(has(alt, "-deathmatch")) << file;
    }
}

TEST_F(ArgumentsOf, ChocolateAndCrispyDoomHostBothDeathmatches) {
    for (const char *file : {"chocolate-doom", "crispy-doom"}) {
        Config config = on(file);

        profile(config).multiplayer.players = 2;
        profile(config).multiplayer.gameType = GameType::Deathmatch;

        Strings args = Arguments::of(config);

        EXPECT_TRUE(has(args, "-deathmatch")) << file;
        EXPECT_FALSE(has(args, "-altdeath")) << file;

        profile(config).multiplayer.gameType = GameType::AltDeathmatch;
        args = Arguments::of(config);

        EXPECT_TRUE(has(args, "-altdeath")) << file;
        EXPECT_FALSE(has(args, "-deathmatch")) << file;
    }
}

TEST_F(ArgumentsOf, HexenIsHandedNoPatches) {
    for (const auto &[file, pwads] : {std::pair{"chocolate-hexen", "-merge"}, std::pair{"crispy-hexen", "-file"}}) {
        Config config = on(file);

        files(config, {"/mods/a.deh", "/mods/b.bex", "/mods/c.wad"});

        EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), pwads, "/mods/c.wad"})) << file;
    }
}

TEST_F(ArgumentsOf, HereticWarpsByEpisodeAndMapAndStrifeByMapNumber) {
    for (const char *file : {"chocolate-heretic", "crispy-heretic"}) {
        Config config = on(file);

        config.iwads.front().file = episodic().string();
        profile(config).warp = "E2M3";

        EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "2", "3"})) << file;
    }

    for (const char *file : {"chocolate-strife", "crispy-strife"}) {
        Config config = on(file);

        profile(config).warp = "MAP07";

        EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "07"})) << file;
    }
}

TEST_F(ArgumentsOf, HexenWarpsByTheWarptransOfThePickedMap) {
    for (const char *file : {"chocolate-hexen", "crispy-hexen"}) {
        Config config = on(file);

        config.iwads.front().file = hexen().string();
        profile(config).warp = "MAP02";

        EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", hexen().string(), "-warp", "5"})) << file;

        profile(config).warp = "map03";

        EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", hexen().string(), "-warp", "3"})) << file;
    }
}

TEST_F(ArgumentsOf, HexenMapsAreTheMapsItsWarpReaches) {
    const fs::path scratch = tests::Sandbox::scratch("arguments-hexen-maps");
    const fs::path patch = launch::withMapInfo(scratch / "patch.wad", "PWAD", {"MAP04"}, "map 4 \"Added\"\n");
    const fs::path deh = scratch / "names.deh";

    launch::write(deh, "Patch File for DeHackEd v3.0\n");

    Config config = on("crispy-hexen");

    config.iwads.front().file = hexen().string();

    EXPECT_EQ(Arguments::maps(config), (Strings{"MAP01", "MAP02", "MAP03"}));

    // The MAPINFO of the add-on replaces the one in the IWAD.
    profile(config).files = {{.file = deh.string()}, {.file = patch.string()}};

    EXPECT_EQ(Arguments::maps(config), (Strings{"MAP04"}));

    profile(config).files.back().enabled = false;

    EXPECT_EQ(Arguments::maps(config), (Strings{"MAP01", "MAP02", "MAP03"}));

    config.engines.front().file = "/games/chocolate-heretic";
    config.iwads.front().file = episodic().string();

    EXPECT_EQ(Arguments::maps(config), (Strings{"E1M1", "E2M1", "E2M3"}));
}

TEST_F(ArgumentsOf, AHexenPickOutsideTheListIsHandedNoWarp) {
    for (const char *file : {"chocolate-hexen", "crispy-hexen"}) {
        Config config = on(file);

        config.iwads.front().file = hexen().string();

        for (const char *map : {"MAP07", "E1M1", "MAP"}) {
            profile(config).warp = map;

            EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", hexen().string()})) << file << ' ' << map;
        }

        // A switch to an IWAD without MAPINFO leaves a stored pick out.
        profile(config).warp = "MAP02";
        config.iwads.front().file = iwad();

        EXPECT_TRUE(Arguments::maps(config).empty()) << file;
        EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad()})) << file;

        // The pick stays stored, so another port still starts on it.
        config.engines.front().file = "/games/chocolate-doom";

        EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "02"})) << file;
    }
}

TEST_F(ArgumentsOf, CrispyHereticStartsWithTheWandOnlyOutsideDemosAndNetgames) {
    Config config = on("crispy-heretic");

    profile(config).config = "alpha.cfg";
    profile(config).setOption(Dialect::Feature::pistolStart, true);
    launch::write(launch::profiles() / "alpha" / "replays" / "run1.lmp", "x");

    Strings args = Arguments::of(config);

    EXPECT_TRUE(has(args, "-wandstart"));
    EXPECT_FALSE(has(args, "-pistolstart"));

    profile(config).replay = {.mode = ReplayMode::Record, .file = "run2"};
    args = Arguments::of(config);

    EXPECT_TRUE(has(args, "-record"));
    EXPECT_FALSE(has(args, "-wandstart"));

    profile(config).replay = {.mode = ReplayMode::Play, .file = "run1"};
    args = Arguments::of(config);

    EXPECT_TRUE(has(args, "-playdemo"));
    EXPECT_FALSE(has(args, "-wandstart"));

    profile(config).replay = {};
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.players = 2;
    args = Arguments::of(config);

    EXPECT_TRUE(has(args, "-server") || has(args, "-privateserver"));
    EXPECT_FALSE(has(args, "-wandstart"));
}

TEST_F(ArgumentsOf, CrispyHereticAndHexenLeaveFastOutOfDemosAndNetgames) {
    for (const char *file : {"crispy-heretic", "crispy-hexen"}) {
        Config config = on(file);

        profile(config).config = "alpha.cfg";
        profile(config).monsters = 2;

        EXPECT_TRUE(has(Arguments::of(config), "-fast")) << file;

        profile(config).replay = {.mode = ReplayMode::Record, .file = "run1"};

        Strings args = Arguments::of(config);

        EXPECT_TRUE(has(args, "-record")) << file;
        EXPECT_FALSE(has(args, "-fast")) << file;

        profile(config).replay = {};
        profile(config).multiplayer.gameType = GameType::Coop;
        profile(config).multiplayer.players = 2;
        args = Arguments::of(config);

        EXPECT_FALSE(has(args, "-fast")) << file;
    }

    Config strife = on("crispy-strife");

    profile(strife).config = "alpha.cfg";
    profile(strife).monsters = 2;
    profile(strife).replay = {.mode = ReplayMode::Record, .file = "run1"};

    EXPECT_TRUE(has(Arguments::of(strife), "-fast")) << "Strife takes -fast in a demo";

    for (const char *file : {"chocolate-heretic", "chocolate-hexen"}) {
        Config config = on(file);

        profile(config).monsters = 2;

        EXPECT_FALSE(has(Arguments::of(config), "-fast")) << file;
    }
}

// Crispy Heretic and Hexen stop on -fast in a demo or a netgame. Respawn still goes on the line.
TEST_F(ArgumentsOf, RespawnStaysWhenADemoOrANetgameDropsFast) {
    for (const char *file : {"crispy-heretic", "crispy-hexen"}) {
        Config config = on(file);

        profile(config).config = "alpha.cfg";
        profile(config).monsters = 4;
        profile(config).setOption(Dialect::Feature::pistolStart, true);
        profile(config).replay = {.mode = ReplayMode::Record, .file = "run1"};

        const Strings args = Arguments::of(config);

        EXPECT_TRUE(has(args, "-record")) << file;
        EXPECT_TRUE(has(args, "-respawn")) << file;
        EXPECT_FALSE(has(args, "-fast")) << file;
        EXPECT_FALSE(has(args, "-wandstart")) << file;

        profile(config).replay = {};
        profile(config).multiplayer.gameType = GameType::Coop;
        profile(config).multiplayer.players = 2;

        const Strings hosted = Arguments::of(config);

        EXPECT_TRUE(has(hosted, "-respawn")) << file;
        EXPECT_FALSE(has(hosted, "-fast")) << file;
    }
}

TEST_F(ArgumentsOf, APortWithoutFastOffersAndPlaysNoFastMonsters) {
    const Dialect::Traits heretic = Dialect::of(on("chocolate-heretic"));
    const Dialect::Traits doom = Dialect::of(on("chocolate-doom"));
    const auto listed = [](const std::span<const int> values) {
        return std::vector<int>(values.begin(), values.end());
    };

    EXPECT_EQ(listed(Arguments::monsterChoices(heretic)), (std::vector{1, 3}));
    EXPECT_EQ(listed(Arguments::monsterChoices(doom)), (std::vector{1, 2, 3, 4}));
    EXPECT_EQ(Arguments::playedMonsters(2, heretic), 0);
    EXPECT_EQ(Arguments::playedMonsters(4, heretic), 3);
    EXPECT_EQ(Arguments::playedMonsters(3, heretic), 3);
    EXPECT_EQ(Arguments::playedMonsters(1, heretic), 1);
    EXPECT_EQ(Arguments::playedMonsters(4, doom), 4);

    Config config = on("chocolate-heretic");

    profile(config).monsters = 4;

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(has(args, "-respawn"));
    EXPECT_FALSE(has(args, "-fast"));
}

TEST_F(ArgumentsOf, APortWithoutRespawnOffersAndPlaysNoRespawningMonsters) {
    const Dialect::Traits helion = Dialect::of(on("helion"));
    const std::span<const int> choices = Arguments::monsterChoices(helion);

    EXPECT_EQ(std::vector<int>(choices.begin(), choices.end()), (std::vector{1, 2}));
    EXPECT_EQ(Arguments::playedMonsters(3, helion), 0);
    EXPECT_EQ(Arguments::playedMonsters(4, helion), 2);
    EXPECT_EQ(Arguments::playedMonsters(2, helion), 2);
    EXPECT_EQ(Arguments::playedMonsters(1, helion), 1);
    EXPECT_EQ(Arguments::playedMonsters(0, helion), 0);

    Config config = on("helion");

    profile(config).monsters = 4;

    const Strings both = Arguments::of(config);

    EXPECT_TRUE(contains(both, {"+sv_fastmonsters", "1"}));
    EXPECT_FALSE(has(both, "-respawn"));

    profile(config).monsters = 3;

    const Strings respawn = Arguments::of(config);

    EXPECT_FALSE(has(respawn, "-respawn"));
    EXPECT_FALSE(has(respawn, "+sv_fastmonsters"));
}

TEST_F(ArgumentsOf, TheMonstersHintSaysWhereFastIsLeftOut) {
    const auto hint = [](const char *file, const char *version) {
        Config config = on(file);

        config.engines.front().version = version;

        return Arguments::monstersHint(Dialect::of(config));
    };

    for (const char *file : {"crispy-heretic", "crispy-hexen"}) {
        EXPECT_TRUE(hint(file, "").starts_with("Monsters move and attack faster. Left out while a demo")) << file;
        EXPECT_EQ(hint(file, "5.10.3"), "") << file;
    }

    for (const char *file : {"crispy-doom", "crispy-strife", "chocolate-heretic", "helion", "gzdoom"}) {
        EXPECT_EQ(hint(file, ""), "") << file;
    }
}

TEST_F(ArgumentsOf, OnlyAPortWithWarpOrMapTakesAMap) {
    for (const char *file : {"gzdoom",
                             "chocolate-heretic",
                             "chocolate-hexen",
                             "chocolate-strife",
                             "crispy-heretic",
                             "crispy-hexen",
                             "crispy-strife"}) {
        EXPECT_TRUE(Arguments::takesMap(Dialect::of(on(file)))) << file;
    }

    Dialect::Traits without = Dialect::of(on("gzdoom"));

    without.features &= ~(Dialect::Feature::warp | Dialect::Feature::map);

    EXPECT_FALSE(Arguments::takesMap(without));
}

TEST_F(ArgumentsOf, TheStartedMapIsThePickTheLineSends) {
    Config config = on("gzdoom");

    EXPECT_EQ(Arguments::startedMap(config), "");

    profile(config).warp = "MAP02";

    EXPECT_EQ(Arguments::startedMap(config), "MAP02");

    // Without a game Chocolate Doom gets no -warp numbers and reads no map name.
    config.engines.front().file = "/games/chocolate-doom";
    profile(config).iwad.clear();

    EXPECT_FALSE(has(Arguments::of(config), "MAP02"));
    EXPECT_EQ(Arguments::startedMap(config), "");

    for (const char *file : {"chocolate-hexen", "crispy-hexen"}) {
        config = on(file);
        config.iwads.front().file = hexen().string();
        profile(config).warp = "MAP02";

        EXPECT_EQ(Arguments::startedMap(config), "MAP02") << file;

        profile(config).warp = "MAP07";

        EXPECT_EQ(Arguments::startedMap(config), "") << file;
    }
}

TEST_F(ArgumentsOf, TheStartedMapFindsRelativeFilesBesideThePort) {
    const fs::path port = tests::Sandbox::scratch("arguments-started-map");

    launch::withMapInfo(port / "beside.wad", "PWAD", {"MAP04"}, "map 4 \"Beside\" warptrans 9\n");

    Config config = on("chocolate-hexen");

    config.engines.front().file = (port / "chocolate-hexen").string();
    config.iwads.front().file = hexen().string();
    profile(config).warp = "MAP04";
    profile(config).files = {{.file = "beside.wad"}};

    EXPECT_EQ(Arguments::startedMap(config), "MAP04");
    EXPECT_EQ(Arguments::maps(config), (Strings{"MAP04"}));
    EXPECT_TRUE(contains(Arguments::of(config, port), {"-warp", "9"}));
}

TEST_F(ArgumentsOf, HexenWithNoGameNamedReadsOnlyTheAddOns) {
    const fs::path scratch = tests::Sandbox::scratch("arguments-hexen-no-game");
    const fs::path addon = launch::withMapInfo(scratch / "addon.wad", "PWAD", {"MAP04"}, "map 4 \"Added\"\n");

    for (const char *file : {"chocolate-hexen", "crispy-hexen"}) {
        Config config = on(file);

        profile(config).iwad.clear();
        profile(config).warp = "MAP04";

        EXPECT_TRUE(Arguments::maps(config).empty()) << file;
        EXPECT_FALSE(has(Arguments::of(config), "-warp")) << file;

        profile(config).files = {{.file = addon.string()}};

        EXPECT_EQ(Arguments::maps(config), (Strings{"MAP04"})) << file;
        EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "4"})) << file;
        EXPECT_FALSE(has(Arguments::of(config), "-iwad")) << file;
    }
}

TEST_F(ArgumentsOf, AFileTypedOnTheLineCarriesTheLastHexenMapInfo) {
    const fs::path scratch = tests::Sandbox::scratch("arguments-hexen-typed");
    const fs::path typed =
            launch::withMapInfo(scratch / "typed.wad", "PWAD", {"MAP04"}, "map 4 \"Typed\" warptrans 9\n");

    for (const char *file : {"chocolate-hexen", "crispy-hexen"}) {
        Config config = on(file);

        config.iwads.front().file = hexen().string();
        profile(config).warp = "MAP04";
        profile(config).extra = "-file " + typed.string();

        EXPECT_EQ(Arguments::maps(config), (Strings{"MAP04"})) << file;
        EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "9"})) << file;
        EXPECT_EQ(Arguments::startedMap(config), "MAP04") << file;

        profile(config).extra.clear();
        config.general.alwaysAdd = "-skill 2 -FILE " + typed.string() + " -nomusic";

        EXPECT_EQ(Arguments::maps(config), (Strings{"MAP04"})) << file;
        EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "9"})) << file;

        // The words of Extra follow those of Always add, so they run on in the same group.
        config.general.alwaysAdd = "-merge";
        profile(config).extra = typed.string();

        EXPECT_EQ(Arguments::maps(config), (Strings{"MAP04"})) << file;
    }
}

// The port finds a bare typed name in the IWAD folder too, as the launch hands it that folder as DOOMWADDIR.
TEST_F(ArgumentsOf, ATypedHexenFileIsFoundBesideTheGame) {
    const fs::path typed = launch::withMapInfo(
            hexen().parent_path() / "beside.wad", "PWAD", {"MAP04"}, "map 4 \"Beside\" warptrans 9\n");

    Config config = on("chocolate-hexen");

    config.iwads.front().file = hexen().string();
    profile(config).warp = "MAP04";
    profile(config).extra = "-file " + typed.filename().string();

    EXPECT_EQ(Arguments::maps(config), (Strings{"MAP04"}));
    EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "9"}));

    fs::remove(typed);
}

TEST_F(ArgumentsOf, TheMapsKeyTellsTheHexenPortsApart) {
    Config chocolate = on("chocolate-hexen");
    Config crispy = on("crispy-hexen");

    for (Config *config : {&chocolate, &crispy}) {
        config->iwads.front().file = hexen().string();
        profile(*config).extra = "-file typed.wad";
    }

    EXPECT_NE(Arguments::mapsKey(chocolate), Arguments::mapsKey(crispy)) << "one merges the add-ons, one files them";
}

// Chocolate Hexen merges the add-ons, which loads them ahead of the -file group. Crispy Hexen hands them over in the
// first -file group, and the port reads no later one.
TEST_F(ArgumentsOf, OnlyTheFirstGroupOfEachSwitchLoads) {
    const fs::path scratch = tests::Sandbox::scratch("arguments-hexen-groups");
    const fs::path addon = launch::withMapInfo(scratch / "addon.wad", "PWAD", {"MAP04"}, "map 4 \"Added\"\n");
    const fs::path typed = launch::withMapInfo(scratch / "typed.wad", "PWAD", {"MAP05"}, "map 5 \"Typed\"\n");

    for (const auto &[file, merged, filed] :
         {std::tuple("chocolate-hexen", "MAP04", "MAP05"), std::tuple("crispy-hexen", "MAP04", "MAP04")}) {
        Config config = on(file);

        config.iwads.front().file = hexen().string();
        profile(config).files = {{.file = addon.string()}};
        profile(config).extra = "-merge " + typed.string();

        EXPECT_EQ(Arguments::maps(config), (Strings{merged})) << file;

        profile(config).extra = "-file " + typed.string();

        EXPECT_EQ(Arguments::maps(config), (Strings{filed})) << file;
    }
}

TEST_F(ArgumentsOf, AHexenAddOnNamedMapInfoIsTheLastMapInfo) {
    const fs::path scratch = tests::Sandbox::scratch("arguments-hexen-lump");
    const fs::path text = scratch / "MAPINFO.txt";
    const fs::path inside = launch::withMapInfo(scratch / "patch.dat", "PWAD", {"MAP04"}, "map 4 \"Inside\"\n");

    launch::write(text, "map 2 \"Only\" warptrans 7\n");

    Config config = on("chocolate-hexen");

    config.iwads.front().file = hexen().string();
    profile(config).warp = "MAP02";
    profile(config).files = {{.file = text.string()}};

    EXPECT_EQ(Arguments::maps(config), (Strings{"MAP02"}));
    EXPECT_TRUE(contains(Arguments::of(config), {"-warp", "7"}));

    // Any other file that does not end in wad is one lump named after it, so the WAD inside goes unread.
    profile(config).files = {{.file = inside.string()}};

    EXPECT_EQ(Arguments::maps(config), (Strings{"MAP01", "MAP02", "MAP03"}));
}

TEST_F(ArgumentsOf, AnOlderOtherGameIsHandedOnlyTheSwitchesItReads) {
    struct Row {
        const char *file;
        const char *version;
        Strings present;
        Strings absent;
    };

    const std::vector<Row> rows = {
            {"chocolate-heretic", "3.1.0", {"-noautoload", "-savedir", "-longtics"}, {}},
            {"chocolate-heretic", "3.0.1", {"-savedir", "-longtics"}, {"-noautoload"}},
            {"chocolate-heretic", "2.3.0", {"-longtics"}, {"-noautoload", "-savedir"}},
            {"chocolate-heretic", "2.2.1", {}, {"-noautoload", "-savedir", "-longtics"}},
            {"chocolate-strife", "2.0.0", {"-longtics"}, {"-noautoload", "-savedir"}},
            {"crispy-heretic", "7.0", {"-coop_spawns", "-wandstart", "-levelstat", "-noautoload"}, {}},
            {"crispy-heretic", "6.0", {"-wandstart", "-levelstat", "-noautoload"}, {"-coop_spawns"}},
            {"crispy-heretic", "5.9.1", {"-levelstat", "-noautoload"}, {"-wandstart"}},
            {"crispy-heretic", "5.8.0", {"-noautoload"}, {"-levelstat"}},
            {"crispy-heretic", "5.3", {"-savedir"}, {"-noautoload"}},
            {"crispy-strife", "7.0", {"-flipweapons", "-doubleammo"}, {}},
            {"crispy-strife", "6.0", {"-doubleammo"}, {"-flipweapons"}},
            {"crispy-strife", "5.10.3", {"-noautoload"}, {"-doubleammo"}},
    };

    for (const Row &row : rows) {
        Config config = on(row.file);

        config.engines.front().version = row.version;
        config.general.profileConfigs = true;
        profile(config).config = "alpha.cfg";
        profile(config).replay = {.mode = ReplayMode::Off, .longtics = true, .file = "run1"};

        for (const Dialect::Feature option : {
                     Dialect::Feature::coopSpawns,
                     Dialect::Feature::doubleAmmo,
                     Dialect::Feature::pistolStart,
                     Dialect::Feature::levelstat,
                     Dialect::Feature::noAutoload,
                     Dialect::Feature::flipWeapons,
             }) {
            profile(config).setOption(option, true);
        }

        const Strings args = Arguments::of(config);

        profile(config).replay.mode = ReplayMode::Record;

        const Strings recorded = Arguments::of(config);

        for (const std::string &each : row.present) {
            const bool found = each == "-longtics" ? has(recorded, each) : has(args, each);

            EXPECT_TRUE(found) << row.file << " " << row.version << " " << each;
        }

        for (const std::string &each : row.absent) {
            EXPECT_FALSE(has(args, each) || has(recorded, each)) << row.file << " " << row.version << " " << each;
        }
    }
}

// Both ports stop on a -gameversion word they do not know, so an older release gets only the words it reads.
TEST_F(ArgumentsOf, ChocolateAndCrispyAreHandedOnlyTheGameVersionsTheyRead) {
    struct Case {
        const char *file;
        const char *version;
        int level;

        // Empty for none on the line.
        std::string word;
    };

    constexpr int V1_2 = Dialect::GAME_VERSIONS;
    constexpr int V1_5 = Dialect::GAME_VERSIONS + 1;
    constexpr int V1_666 = Dialect::GAME_VERSIONS + 2;
    constexpr int V1_9 = Dialect::GAME_VERSIONS + 5;

    const std::vector<Case> chocolate = {
            {"chocolate-doom", "", V1_5, "1.5"},
            {"chocolate-doom", "3.1.1", V1_5, "1.5"},
            {"chocolate-doom", "3.1.0", V1_5, ""},
            {"chocolate-doom", "3.1.0", V1_2, "1.2"},
            {"chocolate-doom", "3.0.1", V1_2, ""},
            {"chocolate-doom", "3.0.1", V1_666, "1.666"},
            {"chocolate-doom", "2.1.0", V1_666, "1.666"},
            {"chocolate-doom", "2.0.0", V1_666, ""},
            {"chocolate-doom", "2.0.0", V1_9, "1.9"},
    };

    const std::vector<Case> crispy = {
            {"crispy-doom", "", V1_5, "1.5"},
            {"crispy-doom", "7.1", V1_5, "1.5"},
            {"crispy-doom", "7.0", V1_5, ""},
            {"crispy-doom", "7.0", V1_2, "1.2"},
            {"crispy-doom", "5.6.4", V1_2, "1.2"},
            {"crispy-doom", "5.6.3", V1_2, ""},
            {"crispy-doom", "5.6.3", V1_666, "1.666"},
            {"crispy-doom", "5.0", V1_666, "1.666"},
            {"crispy-doom", "1.1", V1_666, "1.666"},
            {"crispy-doom", "1.0", V1_666, ""},
            {"crispy-doom", "1.0", V1_9, "1.9"},
    };

    for (const std::vector<Case> &cases : {chocolate, crispy}) {
        for (const Case &each : cases) {
            Config config = on(each.file);

            config.engines.front().version = each.version;
            profile(config).complevel = each.level;

            const Strings args = Arguments::of(config);

            if (each.word.empty()) {
                EXPECT_FALSE(has(args, "-gameversion")) << each.file << " " << each.version << " " << each.level;
            } else {
                EXPECT_TRUE(contains(args, {"-gameversion", each.word})) << each.file << " " << each.version;
            }
        }
    }
}

TEST_F(ArgumentsOf, EachChocolateReleaseIsHandedOnlyTheSwitchesItReads) {
    struct Row {
        const char *version;
        Strings present;
        Strings absent;
    };

    const std::vector<Row> rows = {
            {"", {"-config", "-extraconfig", "-savedir", "-noautoload", "-merge"}, {}},
            {"3.1.0", {"-config", "-extraconfig", "-savedir", "-noautoload", "-merge"}, {}},
            {"3.0.1", {"-config", "-extraconfig", "-savedir", "-merge"}, {"-noautoload"}},
            {"3.0.0", {"-config", "-extraconfig", "-savedir", "-merge"}, {"-noautoload"}},
            {"2.3.0", {"-config", "-extraconfig", "-merge"}, {"-noautoload", "-savedir"}},
            {"2.0.0", {"-config", "-extraconfig", "-merge"}, {"-noautoload", "-savedir"}},
    };

    Config config = on("chocolate-doom");

    config.general.profileConfigs = true;
    profile(config).config = "alpha.cfg";
    profile(config).setOption(Dialect::Feature::noAutoload, true);
    files(config, {"/mods/a.wad"});

    for (const Row &row : rows) {
        config.engines.front().version = row.version;

        const Strings args = Arguments::of(config);

        for (const std::string &each : row.present) {
            EXPECT_TRUE(has(args, each)) << row.version << " " << each;
        }

        for (const std::string &each : row.absent) {
            EXPECT_FALSE(has(args, each)) << row.version << " " << each;
        }
    }
}

TEST_F(ArgumentsOf, EachCrispyReleaseIsHandedOnlyTheSwitchesItReads) {
    using Row = std::pair<const char *, Strings>;

    const Strings all = {
            "-coop_spawns", "-doubleammo", "-pistolstart", "-levelstat", "-noautoload", "-flipweapons", "-fliplevels"};

    const std::vector<Row> rows = {
            {"", all},
            {"7.0", all},
            {"6.0", {"-doubleammo", "-pistolstart", "-levelstat", "-noautoload", "-flipweapons", "-fliplevels"}},
            {"5.11", {"-doubleammo", "-pistolstart", "-levelstat", "-noautoload", "-flipweapons", "-fliplevels"}},
            {"5.10.3", {"-pistolstart", "-levelstat", "-noautoload", "-flipweapons", "-fliplevels"}},
            {"5.9.2", {"-pistolstart", "-levelstat", "-noautoload", "-flipweapons", "-fliplevels"}},
            {"5.9.1", {"-levelstat", "-noautoload", "-flipweapons", "-fliplevels"}},
            {"5.9.0", {"-levelstat", "-noautoload", "-flipweapons", "-fliplevels"}},
            {"5.8.0", {"-noautoload", "-flipweapons", "-fliplevels"}},
            {"5.4", {"-noautoload", "-flipweapons", "-fliplevels"}},
            {"5.3", {"-flipweapons", "-fliplevels"}},
            {"5.2", {"-fliplevels"}},
    };

    Config config = on("crispy-doom");

    config.general.profileConfigs = true;
    profile(config).config = "alpha.cfg";

    for (const Dialect::Feature option : {
                 Dialect::Feature::coopSpawns,
                 Dialect::Feature::doubleAmmo,
                 Dialect::Feature::pistolStart,
                 Dialect::Feature::levelstat,
                 Dialect::Feature::noAutoload,
                 Dialect::Feature::flipWeapons,
                 Dialect::Feature::flipLevels,
         }) {
        profile(config).setOption(option, true);
    }

    for (const auto &[version, present] : rows) {
        config.engines.front().version = version;

        const Strings args = Arguments::of(config);

        for (const std::string &each : all) {
            EXPECT_EQ(has(args, each), std::ranges::find(present, each) != present.end()) << version << " " << each;
        }

        EXPECT_TRUE(has(args, "-savedir")) << version;
    }
}

TEST_F(ArgumentsOf, EachCrispyReleaseUpTo5_0IsHandedOnlyTheSwitchesItReads) {
    struct Row {
        const char *version;
        Strings present;
        Strings absent;
    };

    const std::vector<Row> rows = {
            {"5.0", {"-savedir", "-dm3", "-fliplevels", "-file"}, {"-merge"}},
            {"4.3", {"-savedir", "-dm3", "-fliplevels", "-file"}, {"-merge"}},
            {"4.1", {"-savedir", "-dm3", "-fliplevels", "-file"}, {"-merge"}},
            {"4.0", {"-savedir", "-deathmatch", "-fliplevels", "-file"}, {"-dm3", "-merge"}},
            {"3.5", {"-deathmatch", "-fliplevels", "-file"}, {"-savedir", "-dm3", "-merge"}},
            {"1.5", {"-deathmatch", "-fliplevels", "-file"}, {"-savedir", "-dm3", "-merge"}},
            {"1.4", {"-deathmatch", "-fliplevels", "-merge"}, {"-savedir", "-dm3", "-file"}},
            {"1.3", {"-deathmatch", "-fliplevels", "-merge"}, {"-savedir", "-dm3", "-file"}},
            {"1.2", {"-deathmatch", "-merge"}, {"-savedir", "-dm3", "-fliplevels", "-file"}},
            {"1.0", {"-deathmatch", "-merge"}, {"-savedir", "-dm3", "-fliplevels", "-file"}},
    };

    Config config = on("crispy-doom");

    config.general.profileConfigs = true;
    profile(config).config = "alpha.cfg";
    profile(config).setOption(Dialect::Feature::flipLevels, true);
    profile(config).multiplayer.gameType = GameType::Deathmatch3;
    profile(config).multiplayer.players = 2;
    files(config, {"/mods/a.wad"});

    for (const Row &row : rows) {
        config.engines.front().version = row.version;

        const Strings args = Arguments::of(config);

        for (const std::string &each : row.present) {
            EXPECT_TRUE(has(args, each)) << row.version << " " << each;
        }

        for (const std::string &each : row.absent) {
            EXPECT_FALSE(has(args, each)) << row.version << " " << each;
        }
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

TEST_F(ArgumentsOf, EachPortSaysWhatItDoesWithADemoAlreadyThere) {
    using Arguments::TakenDemo;

    struct Case {
        const char *file;
        const char *version;
        int complevel;
        TakenDemo taken;
    };

    const std::vector<Case> cases = {
            {"chocolate-doom", "", -1, TakenDemo::writesOver},
            {"crispy-doom", "", -1, TakenDemo::numbers},
            {"crispy-doom", "1.3", -1, TakenDemo::numbers},
            {"crispy-doom", "1.2", -1, TakenDemo::writesOver},
            {"crispy-heretic", "5.10.0", -1, TakenDemo::numbers},
            {"crispy-heretic", "5.9.2", -1, TakenDemo::writesOver},
            {"crispy-hexen", "7.2", -1, TakenDemo::numbers},
            {"crispy-hexen", "7.1", -1, TakenDemo::writesOver},
            {"crispy-strife", "7.2", -1, TakenDemo::numbers},
            {"crispy-strife", "7.1", -1, TakenDemo::writesOver},
            {"woof", "", -1, TakenDemo::numbers},
            {"woof", "7.0.0", -1, TakenDemo::numbers},
            {"woof", "6.3.1", -1, TakenDemo::writesOver},
            {"nugget-doom", "1.3.0", -1, TakenDemo::numbers},
            {"nugget-doom", "1.2.0", -1, TakenDemo::writesOver},
            {"cherry-doom", "", -1, TakenDemo::numbers},
            {"dsda-doom", "", 21, TakenDemo::numbers},
            {"dsda-doom", "0.19.0", 21, TakenDemo::numbers},
            {"dsda-doom", "0.18.0", 21, TakenDemo::writesOver},
            {"nyan-doom", "", 21, TakenDemo::numbers},
            {"eternity", "", -1, TakenDemo::writesOver},
            {"gzdoom", "", -1, TakenDemo::writesOver},
            {"zandronum", "", -1, TakenDemo::writesOver},
            {"helion", "", -1, TakenDemo::writesOver},
            {"prboom", "", 2, TakenDemo::continuesOrStops},
            {"prboom", "", 9, TakenDemo::continuesOrStops},
            {"prboom-plus", "", 2, TakenDemo::writesOver},
            {"prboom-plus", "", 9, TakenDemo::continuesOrWritesOver},
            {"prboom-plus", "", -1, TakenDemo::continuesOrWritesOver},
            {"prboom-plus", "2.5.1.4", 2, TakenDemo::writesOver},
            {"prboom-plus", "2.5.1.3", 2, TakenDemo::stops},
            {"prboom-plus", "2.5.1.3", 9, TakenDemo::continuesOrStops},
            {"prboom-plus", "2.5.0.3", 2, TakenDemo::stops},
            {"prboom-plus", "2.5.0.2", 2, TakenDemo::writesOver},
            {"prboom-plus", "2.5.0.2", 9, TakenDemo::continuesOrStops},
    };

    for (const Case &each : cases) {
        Config config = on(each.file);

        config.engines.front().version = each.version;
        profile(config).complevel = each.complevel;
        profile(config).replay = {.mode = ReplayMode::Record, .file = "run1"};

        EXPECT_EQ(Arguments::takenDemo(profile(config), Dialect::of(config)), each.taken)
                << each.file << ' ' << each.version << " at " << each.complevel;
    }
}

TEST_F(ArgumentsOf, AContinuedDemoWritesOverOneAlreadyThere) {
    Config config = on("prboom-plus");

    profile(config).complevel = 9;
    profile(config).replay = {.mode = ReplayMode::Continue, .file = "run2", .source = "run1"};

    EXPECT_EQ(Arguments::takenDemo(profile(config), Dialect::of(config)), Arguments::TakenDemo::writesOver);

    config.engines.front().file = "/games/dsda-doom";

    EXPECT_EQ(Arguments::takenDemo(profile(config), Dialect::of(config)), Arguments::TakenDemo::numbers);
}

TEST_F(ArgumentsOf, AJoiningZandronumClientRecordsAClientDemo) {
    Config config = on("zandronum");
    const fs::path replays = launch::profiles() / "alpha" / "replays";

    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Record, .file = "run1"};
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.host = "10.0.0.9";

    EXPECT_EQ(Arguments::of(config),
              (Strings{"-iwad", iwad(), "-record", (replays / "run1.cld").string(), "-connect", "10.0.0.9"}));

    profile(config).multiplayer = {};

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-record", (replays / "run1.lmp").string()}));
}

TEST_F(ArgumentsOf, AClientDemoPlaysBackThroughPlaydemoAtAnySpeed) {
    Config config = on("zandronum");
    const fs::path replays = launch::profiles() / "alpha" / "replays";

    launch::write(replays / "run1.cld", "x");
    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Play, .playback = Playback::Timed, .file = "run1.cld"};

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-playdemo", (replays / "run1.cld").string()}));

    profile(config).replay.file = "run2.lmp";

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-timedemo", (replays / "run2.lmp").string()}));
}

TEST_F(ArgumentsOf, AnOfflineZandronumRecordingOfAClientDemoNameWritesAnOrdinaryDemo) {
    Config config = on("zandronum");
    const fs::path replays = launch::profiles() / "alpha" / "replays";

    profile(config).config = "alpha.cfg";

    for (const char *name : {"run1.cld", "run1.CLD"}) {
        profile(config).replay = {.mode = ReplayMode::Record, .file = name};

        EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-record", (replays / "run1.lmp").string()}))
                << name;
    }
}

TEST_F(ArgumentsOf, AZandronumClientRecordingPlaysBackAsTheClientDemoAtAnySpeed) {
    Config config = on("zandronum");
    const fs::path replays = launch::profiles() / "alpha" / "replays";

    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Record, .file = "run1"};
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.host = "10.0.0.9";

    EXPECT_EQ(Arguments::of(config),
              (Strings{"-iwad", iwad(), "-record", (replays / "run1.cld").string(), "-connect", "10.0.0.9"}));

    launch::write(replays / "run1.cld", "x");
    profile(config).multiplayer = {};

    for (const Playback playback : {Playback::AsRecorded, Playback::Timed}) {
        profile(config).replay.mode = ReplayMode::Play;
        profile(config).replay.playback = playback;

        EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-playdemo", (replays / "run1.cld").string()}));
        EXPECT_EQ(Storage::replayTrouble(config), "");
    }
}

TEST_F(ArgumentsOf, AClientDemoBesideThePickedDemoIsWhatZandronumPlays) {
    Config config = on("zandronum");
    const fs::path replays = launch::profiles() / "alpha" / "replays";

    launch::write(replays / "run1.lmp", "x");
    launch::write(replays / "run1.cld", "x");
    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Play, .playback = Playback::Timed, .file = "run1.lmp"};

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-playdemo", (replays / "run1.cld").string()}));

    config.engines.front().file = "/games/gzdoom";

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-timedemo", (replays / "run1.lmp").string()}))
            << "GZDoom reads no client demo";
}

TEST_F(ArgumentsOf, AClientDemoNameInCapitalsPlaysThroughPlaydemo) {
    Config config = on("zandronum");
    const fs::path replays = launch::profiles() / "alpha" / "replays";

    launch::write(replays / "run1.CLD", "x");
    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Play, .playback = Playback::Timed, .file = "run1.CLD"};

    EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-playdemo", (replays / "run1.CLD").string()}));
}

TEST_F(ArgumentsOf, AJoiningSkulltagClientRecordsAClientDemo) {
    Config config = on("skulltag");
    const fs::path replays = launch::profiles() / "alpha" / "replays";

    profile(config).config = "alpha.cfg";
    profile(config).replay = {.mode = ReplayMode::Record, .file = "run1.lmp"};
    profile(config).multiplayer.gameType = GameType::Coop;
    profile(config).multiplayer.host = "10.0.0.9";

    EXPECT_EQ(Arguments::of(config),
              (Strings{"-iwad", iwad(), "-record", (replays / "run1.cld").string(), "-connect", "10.0.0.9"}));
}

TEST_F(ArgumentsOf, SoloNetGoesAheadOfPlaybackOnEveryPortThatReadsIt) {
    const fs::path replays = launch::profiles() / "alpha" / "replays";

    launch::write(replays / "run1.lmp", "x");

    for (const char *file : {"woof", "crispy-doom", "helion", "eternity"}) {
        Config config = on(file);

        profile(config).config = "alpha.cfg";
        profile(config).replay = {.mode = ReplayMode::Play, .soloNet = true, .file = "run1"};

        EXPECT_EQ(Arguments::of(config),
                  (Strings{"-iwad", iwad(), "-solo-net", "-playdemo", (replays / "run1.lmp").string()}))
                << file;

        profile(config).replay.soloNet = false;

        EXPECT_EQ(Arguments::of(config), (Strings{"-iwad", iwad(), "-playdemo", (replays / "run1.lmp").string()}))
                << file;
    }
}
