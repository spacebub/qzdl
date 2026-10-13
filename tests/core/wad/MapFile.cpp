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
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Bytes.h"
#include "core/wad/LibDir.h"
#include "core/wad/LibPk3.h"
#include "core/wad/MapFile.h"
#include "core/wad/Wad.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;
namespace Bytes = tests::Bytes;

using Names = std::vector<std::string>;

class MapFileTest : public ::testing::Test {
protected:
    fs::path directory = tests::Sandbox::scratch("mapfile");

    fs::path file(const std::string &name, const std::string &bytes) {
        const fs::path at = directory / name;

        Bytes::write(at, bytes);

        return at;
    }

    static std::string nameIn(const std::string &text) {
        const fs::path at = tests::Sandbox::scratch("iwadinfo") / "info.wad";

        Bytes::write(at, Bytes::wad("IWAD", {{"IWADINFO", text}}));

        return Wad(at).iwadinfoName();
    }
};

TEST_F(MapFileTest, AWadOpensAsAWad) {
    const std::unique_ptr<MapFile> opened = MapFile::open(tests::Corpus::iwad());

    ASSERT_NE(opened, nullptr);
    EXPECT_NE(dynamic_cast<Wad *>(opened.get()), nullptr);
}

TEST_F(MapFileTest, APwadOpensAsAWad) {
    const std::unique_ptr<MapFile> opened = MapFile::open(tests::Corpus::pwad(1));

    EXPECT_NE(dynamic_cast<Wad *>(opened.get()), nullptr);
}

TEST_F(MapFileTest, AZipOpensAsAPk3WhateverItIsCalled) {
    const std::string bytes = Bytes::zip({{.name = "a.txt", .bytes = "a"}});

    EXPECT_NE(dynamic_cast<LibPk3 *>(MapFile::open(file("mod.pk3", bytes)).get()), nullptr);
    EXPECT_NE(dynamic_cast<LibPk3 *>(MapFile::open(file("mod.zip", bytes)).get()), nullptr);
    EXPECT_NE(dynamic_cast<LibPk3 *>(MapFile::open(file("mod.wad", bytes)).get()), nullptr);
}

TEST_F(MapFileTest, ADirectoryOpensAsAFolder) {
    EXPECT_NE(dynamic_cast<LibDir *>(MapFile::open(directory).get()), nullptr);
}

TEST_F(MapFileTest, AWadIsToldByItsMagicNotItsName) {
    const std::string bytes = Bytes::wad("PWAD", Bytes::maps({"MAP01"}));

    EXPECT_NE(dynamic_cast<Wad *>(MapFile::open(file("level.pk3", bytes)).get()), nullptr);
}

TEST_F(MapFileTest, AFileWithoutAnExtensionIsRefused) {
    EXPECT_EQ(MapFile::open(file("doom2", Bytes::wad("IWAD", {}))), nullptr);
}

TEST_F(MapFileTest, ExtensionsThatAreNeverMapsAreRefused) {
    const std::string bytes = Bytes::wad("PWAD", Bytes::maps({"MAP01"}));

    for (const char *name :
         {"x.lmp", "x.txt", "x.cfg", "x.ini", "x.deh", "x.bex", "x.zdl", "x.zds", "x.dsg", "x.esg"}) {
        EXPECT_EQ(MapFile::open(file(name, bytes)), nullptr) << name;
    }
}

TEST_F(MapFileTest, ARefusedExtensionIgnoresCase) {
    EXPECT_EQ(MapFile::open(file("x.DEH", Bytes::wad("PWAD", {}))), nullptr);
}

TEST_F(MapFileTest, UnknownMagicIsRefused) {
    EXPECT_EQ(MapFile::open(file("x.wad", "RIFF....WAVEfmt ")), nullptr);
}

TEST_F(MapFileTest, AFileShorterThanItsMagicIsRefused) {
    EXPECT_EQ(MapFile::open(file("x.wad", "IWA")), nullptr);
    EXPECT_EQ(MapFile::open(file("y.wad", "")), nullptr);
}

TEST_F(MapFileTest, AnEmptyZipIsRefused) {
    EXPECT_EQ(MapFile::open(file("empty.pk3", Bytes::zip({}))), nullptr);
}

TEST_F(MapFileTest, AMissingFileIsRefused) {
    EXPECT_EQ(MapFile::open(directory / "absent.wad"), nullptr);
}

TEST_F(MapFileTest, MapsOfAnIwadAreItsNames) {
    const MapFile::Maps &maps = MapFile::maps(tests::Corpus::iwad().string());

    EXPECT_TRUE(maps.opened);
    EXPECT_TRUE(maps.mapxx);
    EXPECT_EQ(maps.names.size(), 32U);
}

TEST_F(MapFileTest, MapsOfAMissingFileAreUnopened) {
    const MapFile::Maps &maps = MapFile::maps((directory / "absent.wad").string());

    EXPECT_FALSE(maps.opened);
    EXPECT_FALSE(maps.mapxx);
    EXPECT_TRUE(maps.names.empty());
}

TEST_F(MapFileTest, MapsAreReadAgainWhenTheFileChanges) {
    const fs::path at = file("changing.wad", Bytes::wad("PWAD", Bytes::maps({"E1M1"})));

    EXPECT_EQ(MapFile::maps(at.string()).names, (Names{"E1M1"}));

    Bytes::write(at, Bytes::wad("PWAD", Bytes::maps({"MAP01", "MAP02"})));

    const MapFile::Maps &again = MapFile::maps(at.string());

    EXPECT_EQ(again.names, (Names{"MAP01", "MAP02"}));
    EXPECT_TRUE(again.mapxx);
}

TEST_F(MapFileTest, MapsOfAnUnchangedFileAreTheSameAnswer) {
    const std::string at = file("still.wad", Bytes::wad("PWAD", Bytes::maps({"MAP03"}))).string();

    const MapFile::Maps &first = MapFile::maps(at);
    const MapFile::Maps &second = MapFile::maps(at);

    EXPECT_EQ(&first, &second);
}

TEST_F(MapFileTest, IwadinfoTakesTheQuotedName) {
    EXPECT_EQ(nameIn("IWadInfo\n{\n\tName = \"Freedoom: Phase 2\"\n\tAutoname = \"freedoom2\"\n}\n"),
              "Freedoom: Phase 2");
}

TEST_F(MapFileTest, IwadinfoKeysIgnoreCaseAndSpacing) {
    EXPECT_EQ(nameIn("name=\"tight\""), "tight");
    EXPECT_EQ(nameIn("NAME   =   \"wide\""), "wide");
}

TEST_F(MapFileTest, IwadinfoSkipsKeysThatOnlyEndInName) {
    EXPECT_EQ(nameIn("Autoname = \"auto\"\nName = \"real\""), "real");
    EXPECT_EQ(nameIn("Game_name = \"under\"\nName = \"real\""), "real");
}

TEST_F(MapFileTest, IwadinfoSkipsAKeyWithoutAnEqualsOrQuote) {
    EXPECT_EQ(nameIn("Names = \"plural\"\nName = \"real\""), "real");
    EXPECT_EQ(nameIn("Name = bare\nName = \"real\""), "real");
}

TEST_F(MapFileTest, AnUnclosedQuoteIsNoName) {
    EXPECT_EQ(nameIn("Name = \"never closed"), "");
}

TEST_F(MapFileTest, AnEmptyQuoteIsAnEmptyName) {
    EXPECT_EQ(nameIn("Name = \"\""), "");
}

TEST_F(MapFileTest, IwadinfoWithoutANameIsNoName) {
    EXPECT_EQ(nameIn("IWadInfo { Game = \"Doom\" }"), "");
    EXPECT_EQ(nameIn("Name"), "");
    EXPECT_EQ(nameIn("Name ="), "");
}

}
