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

#include "Launch.h"
#include "core/launch/DosFiles.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

using DosFiles::Reach;

class DosFilesTest : public ::testing::Test {
protected:
    fs::path scratch;
    fs::path port;

    void SetUp() override {
        fs::remove_all(launch::profiles());

        scratch = tests::Sandbox::scratch("dosfiles");
        port = scratch / "port";

        fs::create_directories(port);
    }

    [[nodiscard]] fs::path game(const fs::path &directory, const std::string &name) const {
        fs::create_directories(scratch / directory);
        fs::copy_file(tests::Corpus::iwad(), scratch / directory / name);

        return scratch / directory / name;
    }

    [[nodiscard]] fs::path
    episodic(const fs::path &directory, const std::string &name, const launch::Strings &maps) const {
        return launch::wad(scratch / directory / name, "IWAD", maps);
    }

    [[nodiscard]] DosFiles::Directories plain() const {
        return {.instance = scratch / "instance", .files = scratch / "instance" / "dosfiles", .profileOwned = true};
    }
};

}

TEST(DosFilesSpellable, EightAndThreeIsSpellable) {
    EXPECT_TRUE(DosFiles::spellable("DOOM2.EXE"));
    EXPECT_TRUE(DosFiles::spellable("abcdefgh.wad"));
    EXPECT_TRUE(DosFiles::spellable("doom2.wa"));
}

TEST(DosFilesSpellable, ANameWithoutAnExtensionIsSpellable) {
    EXPECT_TRUE(DosFiles::spellable("SETUP"));
}

TEST(DosFilesSpellable, ALongerStemOrExtensionIsNot) {
    EXPECT_FALSE(DosFiles::spellable("abcdefghi.wad"));
    EXPECT_FALSE(DosFiles::spellable("doom.wadx"));
    EXPECT_FALSE(DosFiles::spellable("abcdefghi"));
}

TEST(DosFilesSpellable, AnEmptyStemIsNot) {
    EXPECT_FALSE(DosFiles::spellable(""));
    EXPECT_FALSE(DosFiles::spellable(".wad"));
}

TEST(DosFilesSpellable, TwoDotsAreNot) {
    EXPECT_FALSE(DosFiles::spellable("a.b.c"));
}

TEST(DosFilesSpellable, DosPunctuationIsSpellable) {
    EXPECT_TRUE(DosFiles::spellable("a-b_c~1.wad"));
    EXPECT_TRUE(DosFiles::spellable("{x}!@#.$%&"));
}

TEST(DosFilesSpellable, SpacesAndOtherSymbolsAreNot) {
    EXPECT_FALSE(DosFiles::spellable("my mod.wad"));
    EXPECT_FALSE(DosFiles::spellable("a+b.wad"));
    EXPECT_FALSE(DosFiles::spellable("a,b.wad"));
    EXPECT_FALSE(DosFiles::spellable("caf\xC3\xA9.wad"));
}

TEST_F(DosFilesTest, AGameBesideThePortUnderItsOwnNameIsLeftThere) {
    EXPECT_EQ(DosFiles::reach(game("port", "doom2.wad"), port, true), Reach::beside);
}

TEST_F(DosFilesTest, AGameBesideThePortIsLeftThereWhateverItsName) {
    EXPECT_EQ(DosFiles::reach(game("port", "freedoom.wad"), port, true), Reach::beside);
}

TEST_F(DosFilesTest, AGameBesideAnotherIwadNameIsStaged) {
    const fs::path wanted = game("port", "doom2.wad");

    launch::write(port / "TNT.WAD", "IWAD");

    EXPECT_EQ(DosFiles::reach(wanted, port, true), Reach::staged);
}

TEST_F(DosFilesTest, AGameElsewhereUnderItsOwnNameIsPointedAt) {
    EXPECT_EQ(DosFiles::reach(game("games", "doom2.wad"), port, true), Reach::pointed);
}

TEST_F(DosFilesTest, FinalDoomIsPointedAtUnderItsOwnNames) {
    EXPECT_EQ(DosFiles::reach(game("tnt", "tnt.wad"), port, true), Reach::pointed);
    EXPECT_EQ(DosFiles::reach(game("plutonia", "plutonia.wad"), port, true), Reach::pointed);
}

TEST_F(DosFilesTest, AGameElsewhereUnderAnotherNameIsStaged) {
    EXPECT_EQ(DosFiles::reach(game("games", "freedoom2.wad"), port, true), Reach::staged);
}

TEST_F(DosFilesTest, AGameElsewhereBesideAnotherIwadNameIsStaged) {
    const fs::path wanted = game("games", "doom2.wad");

    launch::write(scratch / "games" / "doom.wad", "IWAD");

    EXPECT_EQ(DosFiles::reach(wanted, port, true), Reach::staged);
}

TEST_F(DosFilesTest, TheNameWantedIsToldFromTheMaps) {
    EXPECT_EQ(DosFiles::reach(episodic("ultimate", "doomu.wad", {"E1M1", "E4M1"}), port, true), Reach::pointed);
    EXPECT_EQ(DosFiles::reach(episodic("registered", "doom.wad", {"E1M1", "E2M1"}), port, true), Reach::pointed);
    EXPECT_EQ(DosFiles::reach(episodic("shareware", "doom1.wad", {"E1M1"}), port, true), Reach::pointed);
    EXPECT_EQ(DosFiles::reach(episodic("misnamed", "doom.wad", {"E1M1"}), port, true), Reach::staged);
}

TEST_F(DosFilesTest, AnUnknownPortIsOnlyEverPointed) {
    EXPECT_EQ(DosFiles::reach(game("port", "freedoom.wad"), port, false), Reach::beside);
    EXPECT_EQ(DosFiles::reach(game("games", "freedoom.wad"), port, false), Reach::pointed);
}

TEST_F(DosFilesTest, WithoutAProfileFolderThePortsDirectoryStandsIn) {
    const Config config = launch::single(port / "DOOM2.EXE", true);

    const DosFiles::Directories where = DosFiles::directories(config, port / "DOOM2.EXE");

    EXPECT_EQ(where.instance, port);
    EXPECT_EQ(where.files, port / "dosfiles");
    EXPECT_FALSE(where.profileOwned);
}

TEST_F(DosFilesTest, AProfileFolderHoldsTheInstanceAndItsCopies) {
    Config config = launch::single(port / "DOOM2.EXE", true);

    config.activeProfile().config = "alpha.cfg";

    const DosFiles::Directories where = DosFiles::directories(config, port / "DOOM2.EXE");

    EXPECT_EQ(where.instance, launch::profiles() / "alpha");
    EXPECT_EQ(where.files, launch::profiles() / "alpha" / "dosfiles");
    EXPECT_TRUE(where.profileOwned);
    EXPECT_TRUE(fs::is_directory(where.instance));
}

TEST_F(DosFilesTest, ASpellableNameIsUsedWhereItIs) {
    DosFiles::Staging staging(plain());

    EXPECT_EQ(staging.spellableName("/mods/short.wad"), fs::path("/mods/short.wad"));
    EXPECT_TRUE(staging.planned().empty());
}

TEST_F(DosFilesTest, ALongNameIsCopiedUnderItsFirstEightLetters) {
    DosFiles::Staging staging(plain());

    EXPECT_EQ(staging.spellableName("/mods/Sunlust-Final.WAD"), plain().files / "sunlust-.wad");
    ASSERT_EQ(staging.planned().size(), 1U);
    EXPECT_EQ(staging.planned().front().from, fs::path("/mods/Sunlust-Final.WAD"));
}

TEST_F(DosFilesTest, LettersDosCannotSpellAreDroppedFromTheShortName) {
    DosFiles::Staging staging(plain());

    EXPECT_EQ(staging.spellableName("/mods/my mod+pack.wad"), plain().files / "mymodpac.wad");
}

TEST_F(DosFilesTest, ANameWithNothingSpellableBecomesZdl) {
    DosFiles::Staging staging(plain());

    EXPECT_EQ(staging.spellableName("/mods/+++ +++.wad"), plain().files / "zdl.wad");
}

TEST_F(DosFilesTest, ALongExtensionIsCutToThree) {
    DosFiles::Staging staging(plain());

    EXPECT_EQ(staging.spellableName("/mods/maps.wad2x"), plain().files / "maps.wad");
}

TEST_F(DosFilesTest, ShortNamesAlikeAreNumberedApart) {
    DosFiles::Staging staging(plain());

    EXPECT_EQ(staging.spellableName("/a/very-long-one.wad"), plain().files / "very-lon.wad");
    EXPECT_EQ(staging.spellableName("/b/very-long-one.wad"), plain().files / "very-lo1.wad");
    EXPECT_EQ(staging.spellableName("/c/very-long-one.wad"), plain().files / "very-lo2.wad");
}

TEST_F(DosFilesTest, TheTenthAlikeGivesUpAnotherLetter) {
    DosFiles::Staging staging(plain());
    fs::path last;

    for (int at = 0; at < 11; at++) {
        last = staging.spellableName(fs::path("/d" + std::to_string(at)) / "very-long-one.wad");
    }

    EXPECT_EQ(last, plain().files / "very-l10.wad");
}

TEST_F(DosFilesTest, ShortNamesWithDifferentExtensionsDoNotCollide) {
    DosFiles::Staging staging(plain());

    EXPECT_EQ(staging.spellableName("/a/very-long-one.wad"), plain().files / "very-lon.wad");
    EXPECT_EQ(staging.spellableName("/a/very-long-one.deh"), plain().files / "very-lon.deh");
}

TEST_F(DosFilesTest, TheGameIsStagedUnderTheNameThePortSearchesFor) {
    DosFiles::Staging staging(plain());

    staging.game(game("games", "freedoom2.wad"));

    ASSERT_EQ(staging.planned().size(), 1U);
    EXPECT_EQ(staging.planned().front().to, plain().instance / "doom2.wad");
}

TEST_F(DosFilesTest, AnUnreadableGameIsStagedAsDoom) {
    DosFiles::Staging staging(plain());

    staging.game(scratch / "missing.wad");

    ASSERT_EQ(staging.planned().size(), 1U);
    EXPECT_EQ(staging.planned().front().to, plain().instance / "doom.wad");
}

TEST_F(DosFilesTest, PruneEmptiesTheCopiesOfWhatWasNotPlanned) {
    const DosFiles::Directories where = plain();

    launch::write(where.files / "kept.wad", "PWAD");
    launch::write(where.files / "stale.wad", "PWAD");
    launch::write(where.files / "stale.deh", "patch");
    fs::create_directories(where.files / "folder");

    DosFiles::prune(where, {{.from = "/mods/kept-long-name.wad", .to = where.files / "KEPT.WAD"}});

    EXPECT_TRUE(fs::exists(where.files / "kept.wad"));
    EXPECT_FALSE(fs::exists(where.files / "stale.wad"));
    EXPECT_FALSE(fs::exists(where.files / "stale.deh"));
    EXPECT_FALSE(fs::exists(where.files / "folder"));
}

TEST_F(DosFilesTest, PruneTakesOnlyUnplannedWadsFromAProfilesFolder) {
    const DosFiles::Directories where = plain();

    launch::write(where.instance / "doom2.wad", "IWAD");
    launch::write(where.instance / "tnt.wad", "IWAD");
    launch::write(where.instance / "zdl.cfg", "settings");
    launch::write(where.instance / "replays" / "run.lmp", "demo");

    DosFiles::prune(where, {{.from = "/games/freedoom2.wad", .to = where.instance / "doom2.wad"}});

    EXPECT_TRUE(fs::exists(where.instance / "doom2.wad"));
    EXPECT_FALSE(fs::exists(where.instance / "tnt.wad"));
    EXPECT_TRUE(fs::exists(where.instance / "zdl.cfg"));
    EXPECT_TRUE(fs::exists(where.instance / "replays" / "run.lmp"));
}

TEST_F(DosFilesTest, PruneLeavesThePortsOwnDirectoryAlone) {
    DosFiles::Directories where = plain();

    where.profileOwned = false;

    launch::write(where.instance / "tnt.wad", "IWAD");
    launch::write(where.files / "stale.wad", "PWAD");

    DosFiles::prune(where, {});

    EXPECT_TRUE(fs::exists(where.instance / "tnt.wad"));
    EXPECT_FALSE(fs::exists(where.files / "stale.wad"));
}
