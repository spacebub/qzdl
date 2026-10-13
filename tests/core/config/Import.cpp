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
#include <string_view>

#include <gtest/gtest.h>

#include "Same.h"
#include "core/config/Import.h"
#include "core/util/Ini.h"
#include "support/Corpus.h"
#include "support/Fixtures.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

constexpr int CORPUS_ENTRIES = 12;

Ini::Section sectionOf(const std::string_view text) {
    const fs::path path = tests::Sandbox::scratch("import-section") / "s.ini";

    same::spill(path, "[zdl.save]\n" + std::string(text));

    Ini ini;

    EXPECT_TRUE(ini.read(path));

    return *ini.section("zdl.save");
}

// What a .zdl holds: the profile less its id, config, launch command and DOS options.
Profile carried() {
    Profile made;

    made.name = "Sunlust \"final\" \xC3\xA9"
                "dition";
    made.iwad = "Doom II";
    made.port = "GZDoom";
    made.files = {{.file = "C:\\Games\\Doom\\sunlust.wad", .enabled = true},
                  {.file = "/home/user/mods/a b.pk3", .enabled = false},
                  {.file = "/m/c.wad", .enabled = true}};
    made.skill = 4;
    made.monsters = 2;
    made.warp = "MAP29";
    made.extra = "-fast +set sv_cheats 1";
    made.dialogOpen = true;
    made.replayOpen = false;
    made.saveOpen = true;
    made.multiplayer = {.gameType = GameType::Deathmatch,
                        .players = 4,
                        .extratic = true,
                        .netmode = 1,
                        .dup = 2,
                        .host = "10.0.0.7",
                        .port = "5030",
                        .fragLimit = "20",
                        .timeLimit = "15",
                        .dmflags = "1024",
                        .dmflags2 = "2048",
                        .savegame = "net.zds",
                        .listed = true};
    made.replay = {.mode = ReplayMode::Record,
                   .file = "run.lmp",
                   .playback = Playback::Timed,
                   .compatibility = 9,
                   .longtics = true,
                   .soloNet = true};
    made.save = {.enabled = true, .file = "save3.zds"};
    made.captureOutput = true;
    made.sharedConfig = true;
    made.levelstat = true;

    return made;
}

class ImportTest : public ::testing::Test {
protected:
    void SetUp() override {
        _dir = tests::Sandbox::scratch("import");

        std::error_code code;

        fs::remove_all(Config::profileFolder("x").parent_path(), code);
    }

    [[nodiscard]] fs::path at(const char *name) const { return _dir / name; }

    Config legacy(const std::string_view text) {
        const fs::path path = at("legacy.ini");

        same::spill(path, text);

        Config config;

        EXPECT_TRUE(Import::loadLegacyFile(path, config));

        return config;
    }

private:
    fs::path _dir;
};

}

TEST_F(ImportTest, TheCorpusIniBringsEveryGameAndPort) {
    Config config;

    ASSERT_TRUE(Import::loadLegacyFile(tests::Corpus::ini(), config));

    ASSERT_EQ(config.iwads.size(), static_cast<size_t>(CORPUS_ENTRIES));
    ASSERT_EQ(config.ports.size(), static_cast<size_t>(CORPUS_ENTRIES));
}

TEST_F(ImportTest, TheCorpusIniMatchesTheIniItWasReadFrom) {
    Config config;
    Ini ini;

    ASSERT_TRUE(Import::loadLegacyFile(tests::Corpus::ini(), config));
    ASSERT_TRUE(ini.read(tests::Corpus::ini()));

    const Ini::Section *iwads = ini.section("zdl.iwads");
    const Ini::Section *ports = ini.section("zdl.ports");

    ASSERT_NE(iwads, nullptr);
    ASSERT_NE(ports, nullptr);

    for (int at = 0; at < CORPUS_ENTRIES; ++at) {
        const std::string index = std::to_string(at);
        const auto slot = static_cast<size_t>(at);

        EXPECT_EQ(config.iwads[slot].name, iwads->get("i" + index + "n")) << at;
        EXPECT_EQ(config.iwads[slot].file, iwads->get("i" + index + "f")) << at;
        EXPECT_EQ(config.ports[slot].name, ports->get("p" + index + "n")) << at;
        EXPECT_EQ(config.ports[slot].file, ports->get("p" + index + "f")) << at;
    }
}

TEST_F(ImportTest, TheSaveSectionBecomesTheActiveImportedProfile) {
    Config config;

    ASSERT_TRUE(Import::loadLegacyFile(tests::Corpus::ini(), config));

    ASSERT_EQ(config.profiles.size(), 1U);
    EXPECT_EQ(config.profiles.front().name, "Imported");
    EXPECT_EQ(config.profiles.front().skill, 3);
    EXPECT_EQ(config.profiles.front().id.size(), 36U);
    EXPECT_EQ(config.activeProfileId, config.profiles.front().id);
    EXPECT_EQ(config.profiles.front().config, "imported.cfg");
}

TEST_F(ImportTest, EntriesAreOrderedByTheirNumberNotTheirSpelling) {
    const Config config = legacy("[zdl.iwads]\ni10n=Ten\ni10f=/g/10.wad\ni2n=Two\ni2f=/g/2.wad\n"
                                 "i1f=/g/1.wad\ni1n=One\n");

    ASSERT_EQ(config.iwads.size(), 3U);
    EXPECT_EQ(config.iwads[0].name, "One");
    EXPECT_EQ(config.iwads[1].name, "Two");
    EXPECT_EQ(config.iwads[2].name, "Ten");
}

TEST_F(ImportTest, AnEntryWithoutAFileIsDropped) {
    const Config config = legacy("[zdl.ports]\np0n=Nameless file\np1f=/p/gzdoom\n");

    ASSERT_EQ(config.ports.size(), 1U);
    EXPECT_EQ(config.ports.front().file, "/p/gzdoom");
    EXPECT_TRUE(config.ports.front().name.empty());
}

TEST_F(ImportTest, EntryKeysAreReadWithoutCase) {
    const Config config = legacy("[ZDL.IWADS]\nI0N=Doom\nI0F=/g/doom.wad\n");

    ASSERT_EQ(config.iwads.size(), 1U);
    EXPECT_EQ(config.iwads.front().name, "Doom");
}

TEST_F(ImportTest, EntryKeysThatAreNotNumberedAreSkipped) {
    const Config config = legacy("[zdl.iwads]\nixn=X\nixf=/g/x.wad\ni0n=Doom\ni0f=/g/doom.wad\ni0q=odd\n");

    ASSERT_EQ(config.iwads.size(), 1U);
    EXPECT_EQ(config.iwads.front().file, "/g/doom.wad");
}

TEST_F(ImportTest, WindowsPathsAreKeptAsWritten) {
    const Config config = legacy("[zdl.iwads]\ni0n=Doom\ni0f=C:\\Games\\Doom\\DOOM2.WAD\n"
                                 "[zdl.save]\nfile0=D:\\mods\\sunlust.wad\n");

    EXPECT_EQ(config.iwads.front().file, "C:\\Games\\Doom\\DOOM2.WAD");
    EXPECT_EQ(config.profiles.front().files.front().file, "D:\\mods\\sunlust.wad");
}

TEST_F(ImportTest, GeneralSettingsAreRead) {
    const Config config = legacy("[zdl.general]\nalwaysadd=-nomusic\nautoclose=1\nzdllaunch=1\nshowpaths=0\n"
                                 "nouserconf=1\nisimported=1\nimportedfrom=/old/zdl.ini\nimportdate=2020-01-02\n"
                                 "lastDir=/l/g\nwadLastDir=/l/w\nsrcLastDir=/l/s\nsaveLastDir=/l/v\n"
                                 "zdlLastDir=/l/z\niniLastDir=/l/i\nwindowsize=800,600\nwindowpos=-10,20\n");
    const GeneralSettings &general = config.general;

    EXPECT_EQ(general.alwaysAdd, "-nomusic");
    EXPECT_TRUE(general.autoClose);
    EXPECT_TRUE(general.launchZdlImmediately);
    EXPECT_FALSE(general.showPaths);
    EXPECT_TRUE(general.noUserConf);
    EXPECT_TRUE(general.isImported);
    EXPECT_EQ(general.importedFrom, "/old/zdl.ini");
    EXPECT_EQ(general.importDate, "2020-01-02");
    EXPECT_EQ(general.lastDirs.general, "/l/g");
    EXPECT_EQ(general.lastDirs.wad, "/l/w");
    EXPECT_EQ(general.lastDirs.src, "/l/s");
    EXPECT_EQ(general.lastDirs.save, "/l/v");
    EXPECT_EQ(general.lastDirs.zdl, "/l/z");
    EXPECT_EQ(general.lastDirs.config, "/l/i");
    EXPECT_TRUE(general.window.hasSize);
    EXPECT_EQ(general.window.width, 800);
    EXPECT_EQ(general.window.height, 600);
    EXPECT_TRUE(general.window.hasPosition);
    EXPECT_EQ(general.window.x, -10);
    EXPECT_EQ(general.window.y, 20);
}

TEST_F(ImportTest, MissingGeneralSettingsTakeTheirDefaults) {
    const Config config = legacy("[zdl.iwads]\ni0n=Doom\ni0f=/g/doom.wad\n");

    EXPECT_TRUE(config.general.showPaths);
    EXPECT_FALSE(config.general.autoClose);
    EXPECT_FALSE(config.general.noUserConf);
    EXPECT_FALSE(config.general.window.hasSize);
    EXPECT_TRUE(config.general.alwaysAdd.empty());
}

TEST_F(ImportTest, OnlyAOneIsTrue) {
    const Config config = legacy("[zdl.general]\nautoclose=true\nshowpaths=yes\n");

    EXPECT_FALSE(config.general.autoClose);
    EXPECT_FALSE(config.general.showPaths);
}

TEST_F(ImportTest, AWindowPairThatIsNotTwoNumbersIsIgnored) {
    const Config config = legacy("[zdl.general]\nwindowsize=800x600\nwindowpos=a,b\n");

    EXPECT_FALSE(config.general.window.hasSize);
    EXPECT_FALSE(config.general.window.hasPosition);
}

TEST_F(ImportTest, NoSaveSectionMeansNoProfile) {
    const Config config = legacy("[zdl.general]\nalwaysadd=-x\n");

    EXPECT_TRUE(config.profiles.empty());
    EXPECT_TRUE(config.activeProfileId.empty());
}

TEST_F(ImportTest, ImportingReplacesWhatTheConfigHeld) {
    Config config = tests::Fixtures::shaped(4, 4, 4);

    same::spill(at("legacy.ini"), "[zdl.general]\nalwaysadd=-x\n");

    ASSERT_TRUE(Import::loadLegacyFile(at("legacy.ini"), config));

    EXPECT_TRUE(config.profiles.empty());
    EXPECT_TRUE(config.ports.empty());
    EXPECT_TRUE(config.general.detected.empty());
    EXPECT_EQ(config.general.alwaysAdd, "-x");
}

TEST_F(ImportTest, AMissingFileFailsAndLeavesTheConfigAlone) {
    const Config made = tests::Fixtures::config(2, 1);
    Config config = made;

    EXPECT_FALSE(Import::loadLegacyFile(at("absent.ini"), config));

    same::config(config, made);
}

TEST_F(ImportTest, ASectionGivesEveryProfileSetting) {
    const Profile profile =
            Import::profileFromSection(sectionOf("iwad=Doom II\nport=GZDoom\nskill=4\nmonsters=2\nwarp=MAP29\n"
                                                 "extra=-fast\ndlgmode=open\ndemomode=OPEN\nsavemode=closed\n"
                                                 "gametype=2\nplayers=4\nextratic=1\nnetmode=1\ndup=2\n"
                                                 "host=10.0.0.7\nmp_port=5030\nfraglimit=20\ntimelimit=15\n"
                                                 "dmflags=1024\ndmflags2=2048\nsavegame=net.zds\nlisted=1\n"
                                                 "demo=2\ndemofile=run.lmp\ndemoplay=2\ncomplevel=9\nlongtics=1\n"
                                                 "solonet=1\nlevelstat=1\nloadsave=1\nsavefile=s.zds\n"));

    EXPECT_EQ(profile.iwad, "Doom II");
    EXPECT_EQ(profile.port, "GZDoom");
    EXPECT_EQ(profile.skill, 4);
    EXPECT_EQ(profile.monsters, 2);
    EXPECT_EQ(profile.warp, "MAP29");
    EXPECT_EQ(profile.extra, "-fast");
    EXPECT_TRUE(profile.dialogOpen);
    EXPECT_TRUE(profile.replayOpen);
    EXPECT_FALSE(profile.saveOpen);
    EXPECT_EQ(profile.multiplayer,
              (MultiplayerSettings{.gameType = GameType::Deathmatch,
                                   .players = 4,
                                   .extratic = true,
                                   .netmode = 1,
                                   .dup = 2,
                                   .host = "10.0.0.7",
                                   .port = "5030",
                                   .fragLimit = "20",
                                   .timeLimit = "15",
                                   .dmflags = "1024",
                                   .dmflags2 = "2048",
                                   .savegame = "net.zds",
                                   .listed = true}));
    EXPECT_EQ(profile.replay,
              (ReplaySettings{.mode = ReplayMode::Play,
                              .file = "run.lmp",
                              .playback = Playback::Fast,
                              .compatibility = 9,
                              .longtics = true,
                              .soloNet = true}));
    EXPECT_TRUE(profile.levelstat);
    EXPECT_EQ(profile.save, (SaveSettings{.enabled = true, .file = "s.zds"}));
}

TEST_F(ImportTest, AnEmptySectionGivesTheDefaults) {
    const Profile profile = Import::profileFromSection(Ini::Section{.name = "zdl.save", .values = {}});
    Profile want;

    want.id = profile.id;
    want.name = profile.name;

    same::profile(profile, want);
}

TEST_F(ImportTest, NumbersThatDoNotParseTakeTheirDefaults) {
    const Profile profile = Import::profileFromSection(sectionOf("skill=hard\nnetmode=x\ncomplevel=\nplayers=+4\n"));

    EXPECT_EQ(profile.skill, 0);
    EXPECT_EQ(profile.multiplayer.netmode, -1);
    EXPECT_EQ(profile.replay.compatibility, -1);
    EXPECT_EQ(profile.multiplayer.players, 0);
}

TEST_F(ImportTest, StoredValuesOutOfRangeTakeTheFirstEnumerator) {
    const Profile profile = Import::profileFromSection(sectionOf("gametype=9\ndemo=-1\ndemoplay=7\n"));

    EXPECT_EQ(profile.multiplayer.gameType, GameType::None);
    EXPECT_EQ(profile.replay.mode, ReplayMode::Off);
    EXPECT_EQ(profile.replay.playback, Playback::AsRecorded);
}

TEST_F(ImportTest, FilesAreOrderedByNumberAndAMarkedOneIsDisabled) {
    const Profile profile = Import::profileFromSection(
            sectionOf("file10=/m/k.wad\nfile2d=/m/c.wad\nfile0=/m/a.wad\nFILE1D=/m/b.wad\n"));

    same::files(profile.files,
                {{.file = "/m/a.wad", .enabled = true},
                 {.file = "/m/b.wad", .enabled = false},
                 {.file = "/m/c.wad", .enabled = false},
                 {.file = "/m/k.wad", .enabled = true}});
}

TEST_F(ImportTest, FileKeysThatAreNotNumberedAreSkipped) {
    const Profile profile = Import::profileFromSection(sectionOf("file=/m/bare.wad\nfiles=/m/s.wad\nfiled=/m/d.wad\n"
                                                                 "filex=/m/x.wad\nfile0=/m/a.wad\n"));

    same::files(profile.files, {{.file = "/m/a.wad", .enabled = true}});
}

TEST_F(ImportTest, WritingASectionLeavesOutWhatIsUnset) {
    Ini::Section section{.name = "zdl.save", .values = {}};
    Profile profile;

    Import::profileToSection(profile, section);

    for (const char *key :
         {"port", "iwad", "skill", "monsters", "warp", "extra", "host", "mp_port", "demofile", "savefile", "file0"}) {
        EXPECT_FALSE(section.has(key)) << key;
    }

    EXPECT_EQ(section.get("dlgmode"), "closed");
    EXPECT_EQ(section.get("netmode"), "-1");
    EXPECT_EQ(section.get("complevel"), "-1");
    EXPECT_EQ(section.get("loadsave"), "0");
}

TEST_F(ImportTest, WritingASectionMarksADisabledFile) {
    Ini::Section section{.name = "zdl.save", .values = {}};
    Profile profile;

    profile.files = {{.file = "/m/a.wad", .enabled = true}, {.file = "/m/b.wad", .enabled = false}};

    Import::profileToSection(profile, section);

    EXPECT_EQ(section.get("file0"), "/m/a.wad");
    EXPECT_EQ(section.get("file1d"), "/m/b.wad");
    EXPECT_FALSE(section.has("file1"));
}

TEST_F(ImportTest, ASectionWrittenThenReadGivesTheProfileBack) {
    Ini::Section section{.name = "zdl.save", .values = {}};
    const Profile made = carried();

    Import::profileToSection(made, section);

    Profile back = Import::profileFromSection(section);
    Profile want = made;

    want.name.clear();
    want.captureOutput = false;
    want.sharedConfig = false;
    back.id = want.id;

    same::profile(back, want);
}

TEST_F(ImportTest, AZdlSavedThenLoadedGivesTheProfileBack) {
    const Profile made = carried();

    ASSERT_TRUE(Import::saveZdlFile(at("round.zdl"), made));

    Profile back;

    ASSERT_TRUE(Import::loadZdlFile(at("round.zdl"), back));

    Profile want = made;

    want.id = back.id;

    EXPECT_EQ(back.id.size(), 36U);
    same::profile(back, want);
}

TEST_F(ImportTest, AZdlHoldsTheSaveAndProfileSections) {
    Profile made;

    made.name = "Doom";
    made.files = {{.file = "/m/a.wad", .enabled = false}};

    ASSERT_TRUE(Import::saveZdlFile(at("out.zdl"), made));

    const std::string text = same::slurp(at("out.zdl"));

    EXPECT_TRUE(text.starts_with("[zdl.save]\n")) << text;
    EXPECT_NE(text.find("\nfile0d=/m/a.wad\n"), std::string::npos) << text;
    EXPECT_NE(text.find("\n[zdl.profile]\nname=Doom\ncaptureOutput=0\nsharedConfig=0\n"), std::string::npos) << text;
}

TEST_F(ImportTest, AZdlWithoutANameIsNamedAfterItsFile) {
    same::spill(at("Ancient Aliens.zdl"), "[zdl.save]\nskill=2\n");

    Profile profile;

    ASSERT_TRUE(Import::loadZdlFile(at("Ancient Aliens.zdl"), profile));

    EXPECT_EQ(profile.name, "Ancient Aliens");
    EXPECT_EQ(profile.skill, 2);
}

TEST_F(ImportTest, AZdlWithABlankNameIsNamedAfterItsFile) {
    same::spill(at("scythe.zdl"), "[zdl.save]\nskill=2\n[zdl.profile]\nname=   \n");

    Profile profile;

    ASSERT_TRUE(Import::loadZdlFile(at("scythe.zdl"), profile));

    EXPECT_EQ(profile.name, "scythe");
}

TEST_F(ImportTest, EachZdlLoadGetsAFreshId) {
    ASSERT_TRUE(Import::saveZdlFile(at("x.zdl"), carried()));

    Profile first;
    Profile second;

    ASSERT_TRUE(Import::loadZdlFile(at("x.zdl"), first));
    ASSERT_TRUE(Import::loadZdlFile(at("x.zdl"), second));

    EXPECT_NE(first.id, second.id);
}

TEST_F(ImportTest, AZdlWithoutASaveSectionFails) {
    same::spill(at("nosave.zdl"), "[zdl.profile]\nname=X\n");

    Profile profile;

    profile.name = "untouched";

    EXPECT_FALSE(Import::loadZdlFile(at("nosave.zdl"), profile));
    EXPECT_EQ(profile.name, "untouched");
}

TEST_F(ImportTest, AMissingZdlFails) {
    Profile profile;

    EXPECT_FALSE(Import::loadZdlFile(at("absent.zdl"), profile));
}

TEST_F(ImportTest, SavingAZdlUnderAFileFails) {
    same::spill(at("blocker"), "x");

    EXPECT_FALSE(Import::saveZdlFile(at("blocker") / "x.zdl", carried()));
}
