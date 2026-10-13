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
#include "core/wad/LibDir.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;
namespace Bytes = tests::Bytes;

using Names = std::vector<std::string>;

constexpr std::array<std::string_view, 2> TITLES = {"TITLEPIC", "TITLE"};

Names sorted(Names names) {
    std::ranges::sort(names);

    return names;
}

// Laid out as the corpus PK3 is, unpacked.
class LibDirTest : public ::testing::Test {
protected:
    fs::path root = tests::Sandbox::scratch("folder");

    void put(const std::string &path, const std::string &bytes) const { Bytes::write(root / path, bytes); }

    void unpacked() const {
        put("iwadinfo.txt", "IWadInfo\n{\n\tName = \"Test Folder\"\n}\n");
        put("graphics/TITLE.png", Bytes::png(64, 40));
        put("playpal.lmp", Bytes::playpal());
        put("maps/MAP01.wad", Bytes::wad("PWAD", Bytes::maps({"MAP01"})));
        put("maps/MAP02.wad", Bytes::wad("PWAD", Bytes::maps({"MAP02"})));
    }
};

TEST_F(LibDirTest, AFolderWithIwadinfoIsAGame) {
    unpacked();

    LibDir folder(root);

    EXPECT_TRUE(folder.isGame());
    EXPECT_EQ(folder.iwadinfoName(), "Test Folder");
}

TEST_F(LibDirTest, AFolderWithoutIwadinfoIsNotAGame) {
    put("maps/MAP01.wad", "x");

    LibDir folder(root);

    EXPECT_FALSE(folder.isGame());
    EXPECT_EQ(folder.iwadinfoName(), "");
}

TEST_F(LibDirTest, IwadinfoCountsOnlyAtTheTopForItsName) {
    put("docs/iwadinfo.txt", "Name = \"Nested\"");

    LibDir folder(root);

    EXPECT_EQ(folder.iwadinfoName(), "");
}

TEST_F(LibDirTest, MapsAreTheFilesInTheMapsFolder) {
    unpacked();

    LibDir folder(root);

    EXPECT_EQ(sorted(folder.mapNames()), (Names{"MAP01", "MAP02"}));
    EXPECT_TRUE(folder.isMapXX());
}

TEST_F(LibDirTest, AMapNameIsTheUppercaseStemCutToEight) {
    put("maps/longmapname.wad", "x");

    LibDir folder(root);

    EXPECT_EQ(folder.mapNames(), (Names{"LONGMAPN"}));
}

TEST_F(LibDirTest, TheMapsFolderIsFoundInAnyCase) {
    put("MAPS/map01.map", "x");

    LibDir folder(root);

    EXPECT_EQ(folder.mapNames(), (Names{"MAP01"}));
    EXPECT_TRUE(folder.isMapXX());
}

TEST_F(LibDirTest, AWadAtTheTopAddsItsMaps) {
    put("episode.wad", Bytes::wad("PWAD", Bytes::maps({"E1M1", "E1M2"})));
    put("maps/MAP05.wad", "x");

    LibDir folder(root);

    EXPECT_EQ(sorted(folder.mapNames()), (Names{"E1M1", "E1M2", "MAP05"}));
}

TEST_F(LibDirTest, WithoutMap01ItIsNotMapXX) {
    put("maps/MAP02.wad", "x");

    EXPECT_FALSE(LibDir(root).isMapXX());
}

TEST_F(LibDirTest, WithoutAMapsFolderItIsNotMapXX) {
    put("MAP01.wad", "x");

    EXPECT_FALSE(LibDir(root).isMapXX());
}

TEST_F(LibDirTest, ALumpIsAFileByItsStemInAnyCase) {
    unpacked();

    LibDir folder(root);

    EXPECT_EQ(folder.lump("PLAYPAL"), Bytes::playpal());
    EXPECT_EQ(folder.lump("PlayPal"), Bytes::playpal());
}

TEST_F(LibDirTest, ALumpIsFoundOneFolderDown) {
    unpacked();

    LibDir folder(root);

    EXPECT_TRUE(folder.lump("TITLE").starts_with("\x89PNG"));
}

TEST_F(LibDirTest, ALumpAtTheTopBeatsOneBelow) {
    put("sub/COLORMAP.lmp", "below");
    put("COLORMAP.lmp", "top");

    EXPECT_EQ(LibDir(root).lump("COLORMAP"), "top");
}

TEST_F(LibDirTest, AnEmptyFileGivesWayToALumpBelow) {
    put("COLORMAP.lmp", "");
    put("sub/COLORMAP.lmp", "below");

    EXPECT_EQ(LibDir(root).lump("COLORMAP"), "below");
}

TEST_F(LibDirTest, ALumpTwoFoldersDownIsNotFound) {
    put("a/b/COLORMAP.lmp", "deep");

    EXPECT_EQ(LibDir(root).lump("COLORMAP"), "");
}

TEST_F(LibDirTest, LumpNamesAreUppercaseStemsOneFolderDeep) {
    put("iwadinfo.txt", "x");
    put("graphics/title.png", "x");
    put("a/b/deep.lmp", "x");

    EXPECT_EQ(sorted(LibDir(root).lumpNames()), (Names{"IWADINFO", "TITLE"}));
}

TEST_F(LibDirTest, ThePictureIsTheTitleGraphic) {
    unpacked();

    EXPECT_TRUE(LibDir(root).picture(TITLES).starts_with("\x89PNG"));
}

TEST_F(LibDirTest, ThePictureIsTheEarliestNameInTheList) {
    put("graphics/TITLE.png", "title");
    put("TITLEPIC.lmp", "titlepic");

    EXPECT_EQ(LibDir(root).picture(TITLES), "titlepic");
}

TEST_F(LibDirTest, APictureMayBeAnyExtensionInGraphics) {
    put("graphics/TITLEPIC.raw", "raw");

    EXPECT_EQ(LibDir(root).picture(TITLES), "raw");
}

TEST_F(LibDirTest, OnlyMusicByThatNameIsNoPicture) {
    put("music/TITLE.ogg", "song");
    put("TITLEPIC.txt", "text");

    EXPECT_EQ(LibDir(root).picture(TITLES), "");
}

TEST_F(LibDirTest, AnEmptyFolderIsNothing) {
    LibDir folder(root);

    EXPECT_FALSE(folder.isGame());
    EXPECT_TRUE(folder.lumpNames().empty());
    EXPECT_TRUE(folder.mapNames().empty());
    EXPECT_EQ(folder.picture(TITLES), "");
}

TEST_F(LibDirTest, AMissingFolderIsNothing) {
    LibDir folder(root / "absent");

    EXPECT_FALSE(folder.isGame());
    EXPECT_FALSE(folder.isMapXX());
    EXPECT_TRUE(folder.lumpNames().empty());
    EXPECT_TRUE(folder.mapNames().empty());
    EXPECT_EQ(folder.iwadinfoName(), "");
    EXPECT_EQ(folder.picture(TITLES), "");
}

}
