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

#include <chrono>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "ttk/system/Paths.h"

#include "Zip.h"
#include "core/ports/Catalog.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

std::string_view lastPart(const std::string_view url) {
    return url.substr(url.find_last_of('/') + 1);
}

class CatalogProgram : public ::testing::Test {
protected:
    fs::path shelf;

    void SetUp() override { shelf = tests::Sandbox::scratch("catalog"); }

    fs::path put(const fs::path &relative, const fs::perms perms = fs::perms::owner_all) const {
        ports::write(shelf / relative, "x");
        fs::permissions(shelf / relative, perms);

        return shelf / relative;
    }
};

}

TEST(Catalog, EveryPortHasTheFieldsTheShelfShows) {
    for (const Catalog::Port &port : Catalog::ports()) {
        EXPECT_FALSE(port.id.empty());
        EXPECT_FALSE(port.name.empty()) << port.id;
        EXPECT_FALSE(port.blurb.empty()) << port.id;
        EXPECT_TRUE(port.homepage.starts_with("https://")) << port.id;
        EXPECT_FALSE(port.program.empty()) << port.id;
    }
}

TEST(Catalog, EveryPortIsFetchedOneWayOnly) {
    for (const Catalog::Port &port : Catalog::ports()) {
        if (port.repository.empty()) {
            EXPECT_TRUE(port.file.starts_with("https://")) << port.id;
            EXPECT_FALSE(port.version.empty()) << port.id;
        } else {
            EXPECT_TRUE(port.file.empty()) << port.id;
            EXPECT_TRUE(port.version.empty()) << port.id;
            EXPECT_TRUE(port.repository.contains('/')) << port.id;
        }
    }
}

TEST(Catalog, IdsAreUnique) {
    std::set<std::string_view> seen;

    for (const Catalog::Port &port : Catalog::ports()) {
        EXPECT_TRUE(seen.insert(port.id).second) << port.id;
    }
}

TEST(Catalog, AFixedFileMatchesItsOwnBuildPattern) {
    for (const Catalog::Port &port : Catalog::ports()) {
        if (port.file.empty()) {
            continue;
        }

        if (!port.windowsBuild.empty()) {
            EXPECT_TRUE(Catalog::matches(lastPart(port.file), port.windowsBuild)) << port.id;
        }

        if (!port.linuxBuild.empty()) {
            EXPECT_TRUE(Catalog::matches(lastPart(port.file), port.linuxBuild)) << port.id;
        }
    }
}

TEST(Catalog, ADosPortRunsTheSameBuildEverywhere) {
    for (const Catalog::Port &port : Catalog::ports()) {
        if (port.dos) {
            EXPECT_EQ(port.windowsBuild, port.linuxBuild) << port.id;
            EXPECT_FALSE(port.windowsBuild.empty()) << port.id;
        }
    }
}

TEST(Catalog, FindReturnsThePortByItsId) {
    const Catalog::Port *found = Catalog::find("dsda-doom");

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "DSDA-Doom");
}

TEST(Catalog, FindOfAnUnknownIdIsNothing) {
    EXPECT_EQ(Catalog::find("no-such-port"), nullptr);
    EXPECT_EQ(Catalog::find(""), nullptr);
    EXPECT_EQ(Catalog::find("DSDA-DOOM"), nullptr);
}

TEST(Catalog, ThePatternIsThisPlatformsBuild) {
    for (const Catalog::Port &port : Catalog::ports()) {
#ifdef _WIN32
        EXPECT_EQ(Catalog::pattern(port), port.windowsBuild) << port.id;
#else
        EXPECT_EQ(Catalog::pattern(port), port.linuxBuild) << port.id;
#endif
    }
}

TEST(CatalogMatches, AnEmptyPatternMatchesNothing) {
    EXPECT_FALSE(Catalog::matches("anything.zip", ""));
    EXPECT_FALSE(Catalog::matches("", ""));
}

TEST(CatalogMatches, TheLastTokenMustEndTheName) {
    EXPECT_TRUE(Catalog::matches("woof-15.0.0-win64.zip", "win64.zip"));
    EXPECT_FALSE(Catalog::matches("woof-15.0.0-win64.zip.sha256", "win64.zip"));
}

TEST(CatalogMatches, EveryEarlierTokenMustAppearAnywhere) {
    EXPECT_TRUE(Catalog::matches("UZDoom-linux-4.14.3-x86_64.AppImage", "linux+.appimage"));
    EXPECT_FALSE(Catalog::matches("UZDoom-macos-4.14.3.AppImage", "linux+.appimage"));
    EXPECT_TRUE(Catalog::matches("prboom-plus-2.6.66-win64.zip", "prboom-plus+.zip"));
}

TEST(CatalogMatches, CaseIsIgnored) {
    EXPECT_TRUE(Catalog::matches("CHOCOLATE-DOOM-3.1.0-WIN64.ZIP", "chocolate-doom-+win64.zip"));
}

TEST(CatalogMatches, ThePatternOfEachPortPicksItsRealAssetName) {
    EXPECT_TRUE(Catalog::matches("dsda-doom-0.29.3-linux-x86_64.appimage", Catalog::find("dsda-doom")->linuxBuild));
    EXPECT_TRUE(Catalog::matches("Helion-0.9.6.0-linux-x64_AOT.zip", Catalog::find("helion")->linuxBuild));
    EXPECT_TRUE(Catalog::matches("gzdoom-4-14-2-windows.zip", Catalog::find("gzdoom")->windowsBuild));
    EXPECT_FALSE(Catalog::matches("gzdoom-4-14-2-macos.zip", Catalog::find("gzdoom")->windowsBuild));
}

TEST(CatalogDirectories, PortsLiveUnderTheDataDirectory) {
    const fs::path data = ttk::Paths::data_directory();

    EXPECT_EQ(Catalog::directory(), data / "source_ports");
    EXPECT_EQ(Catalog::directory(*Catalog::find("woof")), data / "source_ports" / "woof");
    EXPECT_EQ(Catalog::downloads(), data / "downloads");
}

TEST(CatalogRunnable, ADosProgramIsAnExe) {
    EXPECT_TRUE(Catalog::runnable("BOOM.EXE", true));
    EXPECT_TRUE(Catalog::runnable("mbf.exe", true));
    EXPECT_FALSE(Catalog::runnable("boom", true));
    EXPECT_FALSE(Catalog::runnable("boom.bat", true));
}

TEST(CatalogRunnable, ANativeProgramIsThePlatformsOwnKind) {
#ifdef _WIN32
    EXPECT_TRUE(Catalog::runnable("gzdoom.exe"));
    EXPECT_FALSE(Catalog::runnable("gzdoom"));
#else
    EXPECT_TRUE(Catalog::runnable("gzdoom"));
    EXPECT_TRUE(Catalog::runnable("Woof-15.AppImage"));
    EXPECT_FALSE(Catalog::runnable("gzdoom.exe"));
    EXPECT_FALSE(Catalog::runnable("gzdoom.pk3"));
#endif
}

TEST_F(CatalogProgram, TheProgramIsFoundByNameAtAnyDepth) {
    const fs::path program = put("gzdoom-4.14/bin/gzdoom");

    put("gzdoom-4.14/gzdoom.pk3");

    EXPECT_EQ(Catalog::program(shelf, "gzdoom"), program);
}

TEST_F(CatalogProgram, TheNameIsMatchedWithoutCase) {
    const fs::path program = put("GZDoom");

    EXPECT_EQ(Catalog::program(shelf, "gzdoom"), program);
}

TEST_F(CatalogProgram, OfSeveralThatFitTheNewestWins) {
    const fs::path older = put("old/gzdoom");
    const fs::path newer = put("new/gzdoom");
    const auto now = fs::file_time_type::clock::now();

    fs::last_write_time(older, now - std::chrono::hours(2));
    fs::last_write_time(newer, now - std::chrono::hours(1));

    EXPECT_EQ(Catalog::program(shelf, "gzdoom"), newer);
}

TEST_F(CatalogProgram, ADosProgramIsFoundAsAnExe) {
    put("boom");
    const fs::path program = put("BOOM.EXE");

    EXPECT_EQ(Catalog::program(shelf, "boom", true), program);
}

TEST_F(CatalogProgram, NothingThatFitsIsNothing) {
    put("readme.txt");

    EXPECT_EQ(Catalog::program(shelf, "gzdoom"), fs::path());
    EXPECT_EQ(Catalog::program(shelf / "absent", "gzdoom"), fs::path());
}

#ifndef _WIN32
TEST_F(CatalogProgram, AnAppImageAnswersWhateverItsName) {
    const fs::path program = put("Woof-15.0.0-x86_64.AppImage");

    EXPECT_EQ(Catalog::program(shelf, "woof"), program);
}

TEST_F(CatalogProgram, AnExecutableContainingTheNameStandsIn) {
    const fs::path program = put("bin/gzdoom-bin");

    EXPECT_EQ(Catalog::program(shelf, "gzdoom"), program);
}

TEST_F(CatalogProgram, ANonExecutableContainingTheNameDoesNot) {
    put("bin/gzdoom-bin", fs::perms::owner_read | fs::perms::owner_write);

    EXPECT_EQ(Catalog::program(shelf, "gzdoom"), fs::path());
}

TEST_F(CatalogProgram, AnExactNameBeatsOneContainingIt) {
    put("a/gzdoom-bin");
    const fs::path program = put("b/gzdoom");

    EXPECT_EQ(Catalog::program(shelf, "gzdoom"), program);
}
#endif
