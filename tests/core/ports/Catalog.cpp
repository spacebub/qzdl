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
#include <chrono>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Paths.h"

#include "Zip.h"
#include "core/launch/KnownPorts.h"
#include "core/ports/Catalog.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

std::string_view lastPart(const std::string_view url) {
    return url.substr(url.find_last_of('/') + 1);
}

class CatalogFile : public ::testing::Test {
protected:
    fs::path shelf;

    void SetUp() override { shelf = tests::Sandbox::scratch("catalog"); }

    // NOLINTNEXTLINE(modernize-use-nodiscard): some cases only need the file on the shelf.
    fs::path put(const fs::path &relative, const fs::perms perms = fs::perms::owner_all) const {
        ports::write(shelf / relative, "x");
        fs::permissions(shelf / relative, perms);

        return shelf / relative;
    }
};

}

TEST(Catalog, EveryPortHasTheFieldsTheShelfShows) {
    for (const Catalog::Download &download : Catalog::downloads()) {
        EXPECT_FALSE(Catalog::id(download).empty());
        EXPECT_FALSE(Catalog::name(download).empty()) << Catalog::id(download);
        EXPECT_FALSE(download.blurb.empty()) << Catalog::id(download);
        EXPECT_TRUE(download.homepage.starts_with("https://")) << Catalog::id(download);
        EXPECT_FALSE(download.executable.empty()) << Catalog::id(download);
    }
}

TEST(Catalog, EveryPortIsFetchedOneWayOnly) {
    for (const Catalog::Download &download : Catalog::downloads()) {
        if (download.repository.empty()) {
            EXPECT_TRUE(download.file.starts_with("https://")) << Catalog::id(download);
            EXPECT_FALSE(download.version.empty()) << Catalog::id(download);
        } else {
            EXPECT_TRUE(download.file.empty()) << Catalog::id(download);
            EXPECT_TRUE(download.version.empty()) << Catalog::id(download);
            EXPECT_TRUE(download.repository.contains('/')) << Catalog::id(download);
        }
    }
}

// A release's checksum comes with its answer. A fixed file's is pinned here, so a
// swapped file at a plain web host is caught.
TEST(Catalog, AFixedFileHasItsChecksumPinned) {
    for (const Catalog::Download &download : Catalog::downloads()) {
        if (download.repository.empty()) {
            EXPECT_EQ(download.sha256.size(), 64U) << Catalog::id(download);
            EXPECT_TRUE(std::ranges::all_of(download.sha256, [](const char each) {
                return (each >= '0' && each <= '9') || (each >= 'a' && each <= 'f');
            })) << Catalog::id(download);
        } else {
            EXPECT_TRUE(download.sha256.empty()) << Catalog::id(download);
        }
    }
}

TEST(Catalog, IdsAreUnique) {
    std::set<std::string_view> seen;

    for (const Catalog::Download &download : Catalog::downloads()) {
        EXPECT_TRUE(seen.insert(Catalog::id(download)).second) << Catalog::id(download);
    }
}

TEST(Catalog, AFixedFileMatchesItsOwnBuildPattern) {
    for (const Catalog::Download &download : Catalog::downloads()) {
        if (download.file.empty()) {
            continue;
        }

        if (!download.windowsBuild.empty()) {
            EXPECT_TRUE(Catalog::matches(lastPart(download.file), download.windowsBuild)) << Catalog::id(download);
        }

        if (!download.linuxBuild.empty()) {
            EXPECT_TRUE(Catalog::matches(lastPart(download.file), download.linuxBuild)) << Catalog::id(download);
        }
    }
}

TEST(Catalog, ADosPortRunsTheSameBuildEverywhere) {
    for (const Catalog::Download &download : Catalog::downloads()) {
        if (download.dos) {
            EXPECT_EQ(download.windowsBuild, download.linuxBuild) << Catalog::id(download);
            EXPECT_FALSE(download.windowsBuild.empty()) << Catalog::id(download);
        }
    }
}

TEST(Catalog, FindReturnsThePortByItsId) {
    const Catalog::Download *found = Catalog::find("dsda-doom");

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(Catalog::name(*found), "DSDA-Doom");
}

TEST(Catalog, EveryDownloadIsFoundByTheIdOfItsPort) {
    for (const Catalog::Download &download : Catalog::downloads()) {
        EXPECT_EQ(Catalog::find(Catalog::id(download)), &download) << Catalog::id(download);
    }
}

TEST(Catalog, TheIdsStayWhatZdlJsonStores) {
    std::vector<std::string_view> ids;

    for (const Catalog::Download &download : Catalog::downloads()) {
        ids.push_back(Catalog::id(download));
    }

    EXPECT_EQ(ids,
              (std::vector<std::string_view>{"uzdoom",
                                             "gzdoom",
                                             "zdoom",
                                             "zandronum",
                                             "dsda-doom",
                                             "prboom-plus",
                                             "helion",
                                             "woof",
                                             "nugget-doom",
                                             "chocolate-doom",
                                             "crispy-doom",
                                             "boom",
                                             "mbf",
                                             "doomlegacy"}));
}

TEST(Catalog, FindOfAnUnknownIdIsNothing) {
    EXPECT_EQ(Catalog::find("no-such-port"), nullptr);
    EXPECT_EQ(Catalog::find(""), nullptr);
    EXPECT_EQ(Catalog::find("DSDA-DOOM"), nullptr);
}

TEST(Catalog, ThePatternIsThisPlatformsBuild) {
    for (const Catalog::Download &download : Catalog::downloads()) {
#ifdef _WIN32
        EXPECT_EQ(Catalog::pattern(download), download.windowsBuild) << Catalog::id(download);
#else
        EXPECT_EQ(Catalog::pattern(download), download.linuxBuild) << Catalog::id(download);
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
    EXPECT_EQ(Catalog::downloadDirectory(), data / "downloads");
}

TEST(CatalogDirectories, APortWithoutAnIdHasNoDirectory) {
    Catalog::Download download = *Catalog::find("woof");

    download.port = Port::Port::glboomPlus;

    EXPECT_TRUE(Catalog::id(download).empty());
    EXPECT_EQ(Catalog::directory(download), fs::path());

    download.port = Port::Port::unknown;

    EXPECT_EQ(Catalog::directory(download), fs::path());
}

TEST(CatalogDirectories, PortablePortsLiveBesideTheExecutable) {
    const tests::Sandbox::Portable portable;

    EXPECT_EQ(Catalog::directory(), portable.directory() / "source_ports");
    EXPECT_EQ(Catalog::directory(*Catalog::find("woof")), portable.directory() / "source_ports" / "woof");
    EXPECT_EQ(Catalog::downloadDirectory(), portable.directory() / "downloads");
}

TEST(CatalogRunnable, ADosFileIsAnExe) {
    EXPECT_TRUE(Catalog::runnable("BOOM.EXE", true));
    EXPECT_TRUE(Catalog::runnable("mbf.exe", true));
    EXPECT_FALSE(Catalog::runnable("boom", true));
    EXPECT_FALSE(Catalog::runnable("boom.bat", true));
}

TEST(CatalogRunnable, ANativeFileIsThePlatformsOwnKind) {
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

TEST_F(CatalogFile, TheFileIsFoundByNameAtAnyDepth) {
    const fs::path file = put(fs::path("gzdoom-4.14/bin") / ports::program("gzdoom"));

    put("gzdoom-4.14/gzdoom.pk3");

    EXPECT_EQ(Catalog::file(shelf, "gzdoom"), file);
}

TEST_F(CatalogFile, TheNameIsMatchedWithoutCase) {
    const fs::path file = put(ports::program("GZDoom"));

    EXPECT_EQ(Catalog::file(shelf, "gzdoom"), file);
}

TEST_F(CatalogFile, OfSeveralThatFitTheNewestWins) {
    const fs::path older = put(fs::path("old") / ports::program("gzdoom"));
    const fs::path newer = put(fs::path("new") / ports::program("gzdoom"));
    const auto now = fs::file_time_type::clock::now();

    fs::last_write_time(older, now - std::chrono::hours(2));
    fs::last_write_time(newer, now - std::chrono::hours(1));

    EXPECT_EQ(Catalog::file(shelf, "gzdoom"), newer);
}

#ifdef _WIN32

TEST_F(CatalogFile, OnWindowsAFileWithoutExeIsPassedOver) {
    put("gzdoom");
    put("gzdoom.pk3");

    EXPECT_EQ(Catalog::file(shelf, "gzdoom"), fs::path());
}

TEST_F(CatalogFile, OnWindowsTheExeIsMatchedWithoutCase) {
    const fs::path file = put("bin/GZDOOM.EXE");

    EXPECT_EQ(Catalog::file(shelf, "gzdoom"), file);
}

#endif

TEST_F(CatalogFile, ADosFileIsFoundAsAnExe) {
    put("boom");
    const fs::path file = put("BOOM.EXE");

    EXPECT_EQ(Catalog::file(shelf, "boom", true), file);
}

TEST_F(CatalogFile, NothingThatFitsIsNothing) {
    put("readme.txt");

    EXPECT_EQ(Catalog::file(shelf, "gzdoom"), fs::path());
    EXPECT_EQ(Catalog::file(shelf / "absent", "gzdoom"), fs::path());
}

#ifndef _WIN32
TEST_F(CatalogFile, AnAppImageAnswersWhateverItsName) {
    const fs::path file = put("Woof-15.0.0-x86_64.AppImage");

    EXPECT_EQ(Catalog::file(shelf, "woof"), file);
}

TEST_F(CatalogFile, AnExecutableContainingTheNameStandsIn) {
    const fs::path file = put("bin/gzdoom-bin");

    EXPECT_EQ(Catalog::file(shelf, "gzdoom"), file);
}

TEST_F(CatalogFile, ANonExecutableContainingTheNameDoesNot) {
    put("bin/gzdoom-bin", fs::perms::owner_read | fs::perms::owner_write);

    EXPECT_EQ(Catalog::file(shelf, "gzdoom"), fs::path());
}

TEST_F(CatalogFile, AnExactNameBeatsOneContainingIt) {
    put("a/gzdoom-bin");
    const fs::path file = put("b/gzdoom");

    EXPECT_EQ(Catalog::file(shelf, "gzdoom"), file);
}
#endif
