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
#include "core/launch/KnownPorts.h"
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

TEST_F(FileInfoTest, AnEngineIsNamedByItsPort) {
    EXPECT_EQ(FileInfo::describePort("/usr/bin/gzdoom"), "GZDoom");
    EXPECT_EQ(FileInfo::describePort("/opt/dsda/prboom-plus"), "PrBoom+");
}

TEST_F(FileInfoTest, ThePortTableNamesThePort) {
    EXPECT_EQ(FileInfo::describePort("/usr/bin/dsda-doom"), "DSDA-Doom");
    EXPECT_EQ(FileInfo::describePort("mbf.exe"), "MBF");
    EXPECT_EQ(FileInfo::describePort("UZDoom-4.14.2-x86_64.AppImage"), "UZDoom");
}

#ifdef __linux__
TEST_F(FileInfoTest, AFlatpakExportIsNamedForTheAppItRuns) {
    EXPECT_EQ(FileInfo::describePort("/var/lib/flatpak/exports/bin/org.zdoom.GZDoom"), "GZDoom");
    EXPECT_EQ(FileInfo::describePort("/var/lib/flatpak/exports/bin/io.github.fabiangreffrath.woof"), "Woof!");
}
#endif

TEST_F(FileInfoTest, AServerKeepsItsServerName) {
    EXPECT_EQ(FileInfo::describePort("/usr/bin/crispy-server"), "Crispy Doom (Server)");
    EXPECT_EQ(FileInfo::describePort("prboom-plus_server.exe"), "PrBoom+ (Server)");
    EXPECT_EQ(FileInfo::describePort("prboom_server"), "PrBoom (Server)");
}

TEST_F(FileInfoTest, AVanillaExecutableKeepsTheNameOfItsRelease) {
    EXPECT_EQ(FileInfo::describePort("doom.exe"), "Vanilla Doom");
    EXPECT_EQ(FileInfo::describePort("doomu.exe"), "Vanilla Doom");
    EXPECT_EQ(FileInfo::describePort("DOOM2.EXE"), "Doom II: Hell on Earth");
    EXPECT_EQ(FileInfo::describePort("doom95.exe"), "Doom 95");
    EXPECT_EQ(FileInfo::describePort("dosdoom.exe"), "DOSDoom");
}

TEST_F(FileInfoTest, NoListRowHidesANameThePortTableGives) {
    for (const Port::Known &known : Port::known()) {
        if (known.name.empty()) {
            continue;
        }

        if (!known.id.empty()) {
            EXPECT_EQ(FileInfo::describePort(fs::path(known.id)), known.name) << known.id;
        }

        // Other words of a row may be releases the list names on their own, such as doom2.
        EXPECT_EQ(FileInfo::describePort(fs::path(known.words.front())), known.name) << known.words.front();
    }
}

TEST_F(FileInfoTest, ARowOfSeveralPortsLeavesTheNameToTheList) {
    EXPECT_EQ(FileInfo::describePort("lzdoom.exe"), "LZDoom");
    EXPECT_EQ(FileInfo::describePort("qzdoom"), "QZDoom");
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

TEST_F(FileInfoTest, MixedCasePortsInTheTableAreNamed) {
    EXPECT_EQ(FileInfo::describePort("zdoom32_SSE2.exe"), "ZDoom32 (SSE2)");
    EXPECT_EQ(FileInfo::describePort("zdoom32_N.exe"), "ZDoom32 (MinGW)");
}

}
