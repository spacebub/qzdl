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

#include <gtest/gtest.h>

#include "Bytes.h"
#include "core/wad/FileInfo.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;
namespace Bytes = tests::Bytes;

class FileInfoTest : public ::testing::Test {
protected:
    fs::path directory = tests::Sandbox::scratch("fileinfo");

    fs::path file(const std::string &name, const std::string &bytes) {
        const fs::path at = directory / name;

        Bytes::write(at, bytes);

        return at;
    }

    [[nodiscard]] fs::path absent(const std::string &name) const { return directory / name; }
};

TEST_F(FileInfoTest, AnIwadIsNamedByItsIwadinfo) {
    EXPECT_EQ(FileInfo::describeIwad(tests::Corpus::iwad()), "Test Doom");
}

TEST_F(FileInfoTest, APk3IsNamedByItsIwadinfo) {
    EXPECT_EQ(FileInfo::describeIwad(tests::Corpus::pk3()), "Test PK3");
}

TEST_F(FileInfoTest, IwadinfoBeatsAKnownFileName) {
    const fs::path at = directory / "doom2.wad";

    fs::copy_file(tests::Corpus::iwad(), at);

    EXPECT_EQ(FileInfo::describeIwad(at), "Test Doom");
}

TEST_F(FileInfoTest, AKnownFileNameNamesAWadWithoutIwadinfo) {
    EXPECT_EQ(FileInfo::describeIwad(file("plutonia.wad", Bytes::wad("IWAD", Bytes::maps({"MAP01"})))),
              "Final Doom: The Plutonia Experiment");
}

TEST_F(FileInfoTest, AKnownFileNameIgnoresCase) {
    EXPECT_EQ(FileInfo::describeIwad(absent("DOOM2.WAD")), "Doom II: Hell on Earth");
}

TEST_F(FileInfoTest, AMissingFileIsStillNamedByItsFileName) {
    EXPECT_EQ(FileInfo::describeIwad(absent("heretic.wad")), "Heretic Registered");
    EXPECT_EQ(FileInfo::describeIwad(absent("square1.pk3")), "The Adventures of Square");
}

TEST_F(FileInfoTest, AnUnknownFileIsItsUppercaseFileName) {
    EXPECT_EQ(FileInfo::describeIwad(file("mystery.wad", Bytes::wad("IWAD", {}))), "MYSTERY.WAD");
    EXPECT_EQ(FileInfo::describeIwad(absent("Gone.Wad")), "GONE.WAD");
}

TEST_F(FileInfoTest, AnEmptyIwadinfoNameFallsBackToTheFileName) {
    EXPECT_EQ(FileInfo::describeIwad(file("tnt.wad", Bytes::wad("IWAD", {{"IWADINFO", "Name = \"\""}}))),
              "Final Doom: TNT Evilution");
}

TEST_F(FileInfoTest, AFileThatIsNoWadIsNamedByItsFileName) {
    EXPECT_EQ(FileInfo::describeIwad(file("freedoom2.wad", "not a wad")), "Freedoom: Phase 2");
}

TEST_F(FileInfoTest, APortIsNamedByItsProgram) {
    EXPECT_EQ(FileInfo::describePort("/usr/bin/gzdoom"), "GZDoom");
    EXPECT_EQ(FileInfo::describePort("/opt/dsda/prboom-plus"), "PrBoom+");
}

TEST_F(FileInfoTest, APortNameIgnoresCaseAndExtension) {
    EXPECT_EQ(FileInfo::describePort("C:/Games/GZDoom.exe"), "GZDoom");
    EXPECT_EQ(FileInfo::describePort("chocolate-doom.AppImage"), "Chocolate Doom");
}

TEST_F(FileInfoTest, AnUnknownPortIsItsStem) {
    EXPECT_EQ(FileInfo::describePort("/home/user/bin/MyDoom.exe"), "MyDoom");
}

TEST_F(FileInfoTest, AnEmptyPathIsAnEmptyPortName) {
    EXPECT_EQ(FileInfo::describePort(""), "");
}

TEST_F(FileInfoTest, MixedCaseProgramsInTheTableAreNamed) {
    EXPECT_EQ(FileInfo::describePort("zdoom32_SSE2.exe"), "ZDoom32 (SSE2)");
    EXPECT_EQ(FileInfo::describePort("zdoom32_N.exe"), "ZDoom32 (MinGW)");
}

}
