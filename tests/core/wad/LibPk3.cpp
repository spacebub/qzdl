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
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Bytes.h"
#include "core/wad/LibPk3.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;
namespace Bytes = tests::Bytes;

using Names = std::vector<std::string>;

constexpr std::array<std::string_view, 2> TITLES = {"TITLEPIC", "TITLE"};

bool holds(const Names &names, const std::string &name) {
    return std::ranges::find(names, name) != names.end();
}

class LibPk3Test : public ::testing::Test {
protected:
    fs::path directory = tests::Sandbox::scratch("pk3");

    fs::path zip(const std::vector<Bytes::Entry> &entries, const std::string &name = "made.pk3") {
        const fs::path at = directory / name;

        Bytes::write(at, Bytes::zip(entries));

        return at;
    }

    fs::path file(const std::string &name, const std::string &bytes) {
        const fs::path at = directory / name;

        Bytes::write(at, bytes);

        return at;
    }
};

TEST_F(LibPk3Test, APk3WithIwadinfoIsAGame) {
    LibPk3 pk3(tests::Corpus::pk3());

    EXPECT_TRUE(pk3.isGame());
    EXPECT_EQ(pk3.iwadinfoName(), "Test PK3");
}

TEST_F(LibPk3Test, AZipWithoutIwadinfoIsNotAGame) {
    LibPk3 pk3(zip({{.name = "maps/MAP01.wad", .bytes = Bytes::wad("PWAD", Bytes::maps({"MAP01"}))}}));

    EXPECT_FALSE(pk3.isGame());
    EXPECT_EQ(pk3.iwadinfoName(), "");
}

TEST_F(LibPk3Test, IwadinfoCountsOnlyAtTheTop) {
    LibPk3 pk3(zip({{.name = "docs/iwadinfo.txt", .bytes = "Name = \"Nested\""}}));

    EXPECT_EQ(pk3.iwadinfoName(), "");
}

TEST_F(LibPk3Test, MapsAreTheFilesInTheMapsDirectory) {
    LibPk3 pk3(tests::Corpus::pk3());

    const Names names = pk3.mapNames();

    ASSERT_EQ(names.size(), 32U);
    EXPECT_EQ(names.front(), "MAP01");
    EXPECT_EQ(names.back(), "MAP32");
    EXPECT_TRUE(pk3.isMapXX());
}

TEST_F(LibPk3Test, AMapNameIsTheUppercaseStemCutToEight) {
    LibPk3 pk3(zip({{.name = "MAPS/longmapname.wad", .bytes = "x"}, {.name = "maps/e1m1.wad", .bytes = "x"}}));

    EXPECT_EQ(pk3.mapNames(), (Names{"LONGMAPN", "E1M1"}));
}

TEST_F(LibPk3Test, FilesOutsideTheMapsDirectoryAreNoMaps) {
    LibPk3 pk3(zip({{.name = "MAP01.wad", .bytes = "x"}, {.name = "maps/sub/MAP02.wad", .bytes = "x"}}));

    EXPECT_TRUE(pk3.mapNames().empty());
}

TEST_F(LibPk3Test, MapinfoLinesNameMaps) {
    LibPk3 pk3(zip({{.name = "mapinfo.txt",
                     .bytes = "map MAP01 \"Entryway\"\n"
                              "  map\tE1M1 lookup \"HUSTR_E1M1\"\r\n"
                              "MAP map05 {\n"
                              "mapname MAP09\n"
                              "defaultmap\n"
                              "map\n"
                              "// map MAP10\n"
                              "map MAP11"}}));

    EXPECT_EQ(pk3.mapNames(), (Names{"MAP01", "E1M1", "MAP05", "MAP11"}));
}

TEST_F(LibPk3Test, MapinfoNamesFollowTheMapsDirectory) {
    LibPk3 pk3(zip({{.name = "maps/MAP01.wad", .bytes = "x"}, {.name = "MAPINFO", .bytes = "map MAP02 \"Two\"\n"}}));

    EXPECT_EQ(pk3.mapNames(), (Names{"MAP01", "MAP02"}));
}

TEST_F(LibPk3Test, ZmapinfoIsPreferredToMapinfo) {
    LibPk3 pk3(zip({{.name = "mapinfo.txt", .bytes = "map MAP01 \"Old\"\n"},
                    {.name = "zmapinfo.txt", .bytes = "map MAP02 \"New\"\n"}}));

    EXPECT_EQ(pk3.mapNames(), (Names{"MAP02"}));
}

TEST_F(LibPk3Test, ZmapinfoIsPreferredWhicheverComesFirst) {
    LibPk3 pk3(zip({{.name = "zmapinfo.txt", .bytes = "map MAP02 \"New\"\n"},
                    {.name = "mapinfo.txt", .bytes = "map MAP01 \"Old\"\n"}}));

    EXPECT_EQ(pk3.mapNames(), (Names{"MAP02"}));
}

TEST_F(LibPk3Test, AMapinfoBelowTheTopIsIgnored) {
    LibPk3 pk3(zip({{.name = "sub/mapinfo.txt", .bytes = "map MAP01 \"Hidden\"\n"}}));

    EXPECT_TRUE(pk3.mapNames().empty());
}

TEST_F(LibPk3Test, Map01InTheMapsDirectoryIsMapXX) {
    EXPECT_TRUE(LibPk3(zip({{.name = "maps/map01.map", .bytes = "x"}}, "a.pk3")).isMapXX());
    EXPECT_FALSE(LibPk3(zip({{.name = "maps/MAP02.wad", .bytes = "x"}}, "b.pk3")).isMapXX());
    EXPECT_FALSE(LibPk3(zip({{.name = "maps/E1M1.wad", .bytes = "x"}}, "c.pk3")).isMapXX());
    EXPECT_FALSE(LibPk3(zip({{.name = "MAP01.wad", .bytes = "x"}}, "d.pk3")).isMapXX());
}

TEST_F(LibPk3Test, ALumpIsAnEntryByItsStem) {
    LibPk3 pk3(tests::Corpus::pk3());

    EXPECT_EQ(pk3.lump("PLAYPAL"), Bytes::playpal());
    EXPECT_EQ(pk3.lump("playpal"), Bytes::playpal());
}

TEST_F(LibPk3Test, ALumpIsFoundInAnyDirectory) {
    LibPk3 pk3(tests::Corpus::pk3());

    EXPECT_TRUE(pk3.lump("TITLE").starts_with("\x89PNG"));
}

TEST_F(LibPk3Test, AnAbsentLumpIsEmpty) {
    LibPk3 pk3(tests::Corpus::pk3());

    EXPECT_EQ(pk3.lump("NOSUCH"), "");
}

TEST_F(LibPk3Test, LumpNamesAreUppercaseStems) {
    LibPk3 pk3(tests::Corpus::pk3());

    const Names names = pk3.lumpNames();

    EXPECT_EQ(names.size(), 35U);
    EXPECT_TRUE(holds(names, "IWADINFO"));
    EXPECT_TRUE(holds(names, "TITLE"));
    EXPECT_TRUE(holds(names, "PLAYPAL"));
    EXPECT_TRUE(holds(names, "MAP17"));
}

TEST_F(LibPk3Test, DirectoryEntriesAreSkipped) {
    LibPk3 pk3(zip({{.name = "maps/", .bytes = ""}, {.name = "maps/MAP01.wad", .bytes = "x"}}));

    EXPECT_EQ(pk3.lumpNames(), (Names{"MAP01"}));
    EXPECT_EQ(pk3.mapNames(), (Names{"MAP01"}));
}

TEST_F(LibPk3Test, AnEmptyEntryReadsAsEmpty) {
    LibPk3 pk3(zip({{.name = "empty.txt", .bytes = ""}}));

    EXPECT_EQ(pk3.lump("EMPTY"), "");
    EXPECT_EQ(pk3.lumpNames(), (Names{"EMPTY"}));
}

TEST_F(LibPk3Test, ThePictureIsTheTitleGraphic) {
    LibPk3 pk3(tests::Corpus::pk3());

    EXPECT_TRUE(pk3.picture(TITLES).starts_with("\x89PNG"));
}

TEST_F(LibPk3Test, ThePictureIsTheEarliestNameInTheList) {
    LibPk3 pk3(zip({{.name = "graphics/TITLE.png", .bytes = "title"},
                    {.name = "graphics/TITLEPIC.png", .bytes = "titlepic"}}));

    EXPECT_EQ(pk3.picture(TITLES), "titlepic");
}

TEST_F(LibPk3Test, APictureMayBeAnyExtensionInGraphics) {
    LibPk3 pk3(zip({{.name = "graphics/TITLEPIC.raw", .bytes = "raw"}}));

    EXPECT_EQ(pk3.picture(TITLES), "raw");
}

TEST_F(LibPk3Test, APictureOutsideGraphicsNeedsAPictureExtension) {
    LibPk3 pk3(zip({{.name = "music/TITLEPIC.ogg", .bytes = "song"}, {.name = "TITLE.lmp", .bytes = "lump"}}));

    EXPECT_EQ(pk3.picture(TITLES), "lump");
}

TEST_F(LibPk3Test, OnlyMusicByThatNameIsNoPicture) {
    LibPk3 pk3(zip({{.name = "music/title.ogg", .bytes = "song"}}));

    EXPECT_EQ(pk3.picture(TITLES), "");
}

TEST_F(LibPk3Test, AnEntryWithoutAnExtensionMayBeAPicture) {
    LibPk3 pk3(zip({{.name = "TITLEPIC", .bytes = "bare"}}));

    EXPECT_EQ(pk3.picture(TITLES), "bare");
}

TEST_F(LibPk3Test, ADeflatedEntryIsInflated) {
    const std::string text = "IWadInfo\n{\n\tName = \"Packed\"\n}\n";

    LibPk3 pk3(zip({{.name = "iwadinfo.txt", .bytes = text, .method = Bytes::DEFLATED}}));

    EXPECT_EQ(pk3.lump("IWADINFO"), text);
    EXPECT_EQ(pk3.iwadinfoName(), "Packed");
}

TEST_F(LibPk3Test, AnEntryInAnUnsupportedMethodReadsAsEmpty) {
    LibPk3 pk3(zip({{.name = "iwadinfo.txt", .bytes = "Name = \"Lzma\"", .method = Bytes::LZMA}}));

    EXPECT_EQ(pk3.lump("IWADINFO"), "");
    EXPECT_FALSE(pk3.isGame());
    EXPECT_EQ(pk3.lumpNames(), (Names{"IWADINFO"}));
}

TEST_F(LibPk3Test, AnEntryFailingItsChecksumReadsAsEmpty) {
    std::string bytes = Bytes::zip({{.name = "iwadinfo.txt", .bytes = "Name = \"Good\""}});
    const size_t at = bytes.find("Good");

    ASSERT_NE(at, std::string::npos);

    bytes[at] = 'F';

    LibPk3 pk3(file("crc.pk3", bytes));

    EXPECT_EQ(pk3.lump("IWADINFO"), "");
}

TEST_F(LibPk3Test, ACentralDirectoryPastTheEndOpensNothing) {
    std::string bytes = Bytes::zip({{.name = "iwadinfo.txt", .bytes = "Name = \"Lost\""}});

    // The directory offset is the second last field of the end record.
    const size_t field = bytes.size() - 6;

    bytes[field] = '\x7F';
    bytes[field + 1] = '\x7F';

    LibPk3 pk3(file("lost.pk3", bytes));

    EXPECT_FALSE(pk3.isGame());
    EXPECT_TRUE(pk3.lumpNames().empty());
    EXPECT_TRUE(pk3.mapNames().empty());
    EXPECT_EQ(pk3.picture(TITLES), "");
}

TEST_F(LibPk3Test, AZipWithoutItsEndRecordOpensNothing) {
    std::string bytes = Bytes::zip({{.name = "iwadinfo.txt", .bytes = "Name = \"Cut\""}});

    bytes.resize(bytes.size() - 22);

    LibPk3 pk3(file("cut.pk3", bytes));

    EXPECT_FALSE(pk3.isGame());
    EXPECT_TRUE(pk3.lumpNames().empty());
}

TEST_F(LibPk3Test, AZipCutInHalfOpensNothing) {
    const std::string bytes = Bytes::zip({{.name = "iwadinfo.txt", .bytes = std::string(400, 'x')}});

    LibPk3 pk3(file("half.pk3", bytes.substr(0, bytes.size() / 2)));

    EXPECT_FALSE(pk3.isMapXX());
    EXPECT_TRUE(pk3.lumpNames().empty());
}

TEST_F(LibPk3Test, AnEmptyFileOpensNothing) {
    LibPk3 pk3(file("empty.pk3", ""));

    EXPECT_FALSE(pk3.isGame());
    EXPECT_EQ(pk3.iwadinfoName(), "");
    EXPECT_TRUE(pk3.mapNames().empty());
}

TEST_F(LibPk3Test, AMissingFileOpensNothing) {
    LibPk3 pk3(directory / "absent.pk3");

    EXPECT_FALSE(pk3.isGame());
    EXPECT_EQ(pk3.lump("ANY"), "");
}

}
