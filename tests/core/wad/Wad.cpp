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

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Bytes.h"
#include "core/wad/Wad.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;
namespace Bytes = tests::Bytes;

using Names = std::vector<std::string>;

class WadTest : public ::testing::Test {
protected:
    fs::path directory = tests::Sandbox::scratch("wad");

    fs::path file(const std::string &name, const std::string &bytes) {
        const fs::path at = directory / name;

        Bytes::write(at, bytes);

        return at;
    }

    fs::path pwad(const std::vector<Bytes::Lump> &lumps) { return file("made.wad", Bytes::wad("PWAD", lumps)); }

    // A directory written by hand, after a data area of `data` bytes.
    fs::path raw(const std::string &data, const std::vector<Bytes::Placed> &entries) {
        std::string directory;

        for (const Bytes::Placed &each : entries) {
            Bytes::entry(directory, each);
        }

        return file("raw.wad",
                    Bytes::header("PWAD",
                                  static_cast<std::int32_t>(entries.size()),
                                  static_cast<std::int32_t>(12 + data.size()))
                            + data + directory);
    }
};

TEST_F(WadTest, AnIwadIsAGame) {
    Wad wad(tests::Corpus::iwad());

    EXPECT_TRUE(wad.isGame());
}

TEST_F(WadTest, APwadIsNotAGame) {
    Wad wad(tests::Corpus::pwad(2));

    EXPECT_FALSE(wad.isGame());
}

TEST_F(WadTest, TheIwadinfoNameIsRead) {
    Wad wad(tests::Corpus::iwad());

    EXPECT_EQ(wad.iwadinfoName(), "Test Doom");
}

TEST_F(WadTest, AWadWithoutIwadinfoHasNoName) {
    Wad wad(tests::Corpus::pwad(1));

    EXPECT_EQ(wad.iwadinfoName(), "");
}

TEST_F(WadTest, ALumpReadsItsBytes) {
    Wad wad(tests::Corpus::iwad());

    EXPECT_EQ(wad.lump("PLAYPAL"), Bytes::playpal());
}

TEST_F(WadTest, LumpLookupIgnoresCase) {
    Wad wad(tests::Corpus::iwad());

    EXPECT_EQ(wad.lump("playpal"), Bytes::playpal());
    EXPECT_EQ(wad.lump("PlayPal"), Bytes::playpal());
}

TEST_F(WadTest, AnEightLetterNameNeedsNoTerminator) {
    Wad wad(pwad({{"ABCDEFGH", "eight"}, {"NEXT", "next"}}));

    EXPECT_EQ(wad.lump("ABCDEFGH"), "eight");
    EXPECT_EQ(wad.lumpNames(), (Names{"ABCDEFGH", "NEXT"}));
}

TEST_F(WadTest, ANameLongerThanEightLettersFindsNothing) {
    Wad wad(pwad({{"ABCDEFGH", "eight"}}));

    EXPECT_EQ(wad.lump("ABCDEFGHI"), "");
}

TEST_F(WadTest, APrefixOfANameFindsNothing) {
    Wad wad(pwad({{"TITLEPIC", "title"}}));

    EXPECT_EQ(wad.lump("TITLE"), "");
}

TEST_F(WadTest, ADuplicateNameReadsTheFirstInTheDirectory) {
    Wad wad(pwad({{"DUP", "first"}, {"DUP", "second"}}));

    EXPECT_EQ(wad.lump("DUP"), "first");
}

TEST_F(WadTest, AnEmptyLumpGivesWayToALaterOneOfTheSameName) {
    Wad wad(pwad({{"DUP", ""}, {"DUP", "filled"}}));

    EXPECT_EQ(wad.lump("DUP"), "filled");
}

TEST_F(WadTest, AMarkerReadsAsEmpty) {
    Wad wad(pwad({{"F_START", ""}, {"F_END", ""}}));

    EXPECT_EQ(wad.lump("F_START"), "");
}

TEST_F(WadTest, AnAbsentLumpReadsAsEmpty) {
    Wad wad(tests::Corpus::iwad());

    EXPECT_EQ(wad.lump("NOSUCH"), "");
}

TEST_F(WadTest, LumpNamesAreUppercaseAndKeepDuplicates) {
    Wad wad(pwad({{"title", "a"}, {"DUP", "b"}, {"dup", "c"}}));

    EXPECT_EQ(wad.lumpNames(), (Names{"TITLE", "DUP", "DUP"}));
}

TEST_F(WadTest, AMapIsTheLumpBeforeItsThings) {
    Wad wad(tests::Corpus::iwad());

    const Names names = wad.mapNames();

    ASSERT_EQ(names.size(), 32U);
    EXPECT_EQ(names.front(), "MAP01");
    EXPECT_EQ(names[6], "MAP07");
    EXPECT_EQ(names.back(), "MAP32");
}

TEST_F(WadTest, EpisodeMapsAreFoundAndAreNotMapXX) {
    Wad wad(pwad(Bytes::maps({"E1M1", "E1M2", "E2M1"})));

    EXPECT_EQ(wad.mapNames(), (Names{"E1M1", "E1M2", "E2M1"}));
    EXPECT_FALSE(wad.isMapXX());
}

TEST_F(WadTest, AWadWithMap01IsMapXX) {
    Wad wad(tests::Corpus::pwad(1));

    EXPECT_TRUE(wad.isMapXX());
}

TEST_F(WadTest, MapXXIsToldByMap01Alone) {
    Wad wad(pwad(Bytes::maps({"MAP07"})));

    EXPECT_EQ(wad.mapNames(), (Names{"MAP07"}));
    EXPECT_FALSE(wad.isMapXX());
}

TEST_F(WadTest, ThingsFirstInTheDirectoryIsNoMap) {
    Wad wad(pwad({{"THINGS", "x"}, {"LINEDEFS", "y"}}));

    EXPECT_TRUE(wad.mapNames().empty());
}

TEST_F(WadTest, AnyLumpBeforeThingsNamesAMap) {
    Wad wad(pwad({{"PLAYPAL", Bytes::playpal()}, {"FOO", ""}, {"THINGS", "x"}}));

    EXPECT_EQ(wad.mapNames(), (Names{"FOO"}));
}

TEST_F(WadTest, ThePictureIsTheEarliestNameInTheList) {
    Wad wad(pwad({{"CREDIT", "credit"}, {"TITLEPIC", "titlepic"}}));

    constexpr std::array<std::string_view, 2> title = {"TITLEPIC", "CREDIT"};
    constexpr std::array<std::string_view, 2> credit = {"CREDIT", "TITLEPIC"};

    EXPECT_EQ(wad.picture(title), "titlepic");
    EXPECT_EQ(wad.picture(credit), "credit");
}

TEST_F(WadTest, PictureNamesIgnoreCase) {
    Wad wad(pwad({{"TITLEPIC", "titlepic"}}));

    constexpr std::array<std::string_view, 1> names = {"titlepic"};

    EXPECT_EQ(wad.picture(names), "titlepic");
}

TEST_F(WadTest, ANamelessPictureListFindsNothing) {
    Wad wad(tests::Corpus::iwad());

    EXPECT_EQ(wad.picture({}), "");
}

TEST_F(WadTest, APictureNoneOfTheNamesFitsIsEmpty) {
    Wad wad(tests::Corpus::iwad());

    constexpr std::array<std::string_view, 2> names = {"INTERPIC", "STARTUP"};

    EXPECT_EQ(wad.picture(names), "");
}

TEST_F(WadTest, APictureLumpPastTheEndGivesWayToTheNextName) {
    Wad wad(raw("credit",
                {{.offset = 12, .length = 6, .name = "CREDIT"}, {.offset = 12, .length = 900, .name = "TITLEPIC"}}));

    constexpr std::array<std::string_view, 2> names = {"TITLEPIC", "CREDIT"};

    EXPECT_EQ(wad.picture(names), "credit");
}

TEST_F(WadTest, ALumpPastTheEndReadsAsEmpty) {
    Wad wad(raw("abcdef", {{.offset = 12, .length = 600, .name = "LONG"}}));

    EXPECT_EQ(wad.lump("LONG"), "");
    EXPECT_EQ(wad.lumpNames(), (Names{"LONG"}));
}

TEST_F(WadTest, ANegativeLengthReadsAsEmpty) {
    Wad wad(raw("abcdef", {{.offset = 12, .length = -4, .name = "NEG"}}));

    EXPECT_EQ(wad.lump("NEG"), "");
}

TEST_F(WadTest, ANegativeOffsetReadsAsEmpty) {
    Wad wad(raw("abcdef", {{.offset = -8, .length = 4, .name = "NEG"}}));

    EXPECT_EQ(wad.lump("NEG"), "");
}

TEST_F(WadTest, ALumpReachingExactlyTheDirectoryIsRead) {
    Wad wad(raw("abcdef", {{.offset = 12, .length = 6, .name = "ALL"}}));

    EXPECT_EQ(wad.lump("ALL"), "abcdef");
}

TEST_F(WadTest, NoLumpsIsAnEmptyGame) {
    Wad wad(file("empty.wad", Bytes::header("IWAD", 0, 12)));

    EXPECT_TRUE(wad.isGame());
    EXPECT_TRUE(wad.lumpNames().empty());
    EXPECT_TRUE(wad.mapNames().empty());
    EXPECT_EQ(wad.iwadinfoName(), "");
}

TEST_F(WadTest, ANegativeLumpCountListsNothing) {
    Wad wad(file("negative.wad", Bytes::header("PWAD", -3, 12)));

    EXPECT_TRUE(wad.lumpNames().empty());
}

TEST_F(WadTest, AnAbsurdLumpCountListsNothing) {
    Wad wad(file("absurd.wad", Bytes::header("PWAD", 0x7FFFFFFF, 12)));

    EXPECT_TRUE(wad.lumpNames().empty());
}

TEST_F(WadTest, ADirectoryPastTheEndListsNothing) {
    Wad wad(file("past.wad", Bytes::header("PWAD", 1, 4096)));

    EXPECT_TRUE(wad.lumpNames().empty());
    EXPECT_EQ(wad.lump("ANY"), "");
}

TEST_F(WadTest, ANegativeDirectoryOffsetListsNothing) {
    Wad wad(file("before.wad", Bytes::header("PWAD", 1, -16) + std::string(16, 'x')));

    EXPECT_TRUE(wad.lumpNames().empty());
}

TEST_F(WadTest, ADirectoryCutShortListsNothing) {
    std::string bytes = Bytes::wad("PWAD", {{"ONE", "1"}, {"TWO", "2"}, {"THREE", "3"}});

    bytes.resize(bytes.size() - 20);

    Wad wad(file("short.wad", bytes));

    EXPECT_TRUE(wad.lumpNames().empty());
}

TEST_F(WadTest, AHeaderCutShortIsNotAGame) {
    Wad wad(file("cut.wad", std::string("IWAD\x02\x00", 6)));

    EXPECT_FALSE(wad.isGame());
    EXPECT_TRUE(wad.lumpNames().empty());
}

TEST_F(WadTest, AnEmptyFileIsNothing) {
    Wad wad(file("nothing.wad", ""));

    EXPECT_FALSE(wad.isGame());
    EXPECT_TRUE(wad.lumpNames().empty());
    EXPECT_TRUE(wad.mapNames().empty());
    EXPECT_FALSE(wad.isMapXX());
    EXPECT_EQ(wad.lump("PLAYPAL"), "");
}

TEST_F(WadTest, TextIsNotAWad) {
    Wad wad(file("readme.wad", "This is not a WAD file at all, only some words in a row.\n"));

    EXPECT_FALSE(wad.isGame());
    EXPECT_TRUE(wad.lumpNames().empty());
}

TEST_F(WadTest, AMissingFileIsNothing) {
    Wad wad(directory / "absent.wad");

    EXPECT_FALSE(wad.isGame());
    EXPECT_TRUE(wad.lumpNames().empty());
    EXPECT_EQ(wad.iwadinfoName(), "");
}

TEST_F(WadTest, ReadsInAnyOrderAnswerTheSame) {
    Wad wad(tests::Corpus::iwad());

    const std::string palette = wad.lump("PLAYPAL");
    const Names maps = wad.mapNames();
    const std::string again = wad.lump("PLAYPAL");

    EXPECT_EQ(palette, again);
    EXPECT_EQ(maps, wad.mapNames());
    EXPECT_TRUE(wad.isGame());
}

}
