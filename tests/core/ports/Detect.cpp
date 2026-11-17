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
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Env.h"
#include "ttk/system/Paths.h"

#include "Zip.h"
#include "core/launch/KnownPorts.h"
#include "core/ports/Catalog.h"
#include "core/ports/Detect.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

fs::path home() {
    return ttk::Paths::home_directory();
}

fs::path bin() {
    return tests::Sandbox::root() / "bin";
}

fs::path executableFile(const fs::path &path) {
    ports::write(path, "#!/bin/sh\n");
    fs::permissions(path, fs::perms::owner_all);

    return path;
}

const Detect::Found *detected(const std::string &id) {
    for (const Detect::Found &each : Detect::ports()) {
        if (Port::known(each.port).id == id) {
            return &each;
        }
    }

    return nullptr;
}

// A port installed on this machine outside the sandbox takes precedence over the one placed here.
bool foreign(const Detect::Found *found) {
    return found != nullptr && !found->file.string().starts_with(tests::Sandbox::root().string());
}

// Detection is looked up once per run, so the machine it walks is laid out before the first case.
class DetectPorts : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        executableFile(bin() / "uzdoom");
        executableFile(home() / "Games" / "uzdoom" / "uzdoom");
        executableFile(bin() / "dosbox-x");
        executableFile(bin() / "dosbox-staging");
        executableFile(home() / "Games" / "DSDA-Doom-0.29" / "dsda-doom");
        executableFile(home() / "Applications" / "Woof-15.0.0-x86_64.AppImage");
        executableFile(home() / ".local" / "bin" / "helion");
        executableFile(home() / "Games" / "boom202" / "BOOM.EXE");
        executableFile(home() / "Games" / "DoomLegacy" / "doom3.exe");
        executableFile(home() / "Games" / "nugget-doom-1.2" / "bin" / "nugget-doom");
        executableFile(home() / "Games" / "chocolate-doom" / "a" / "b" / "chocolate-doom");
        executableFile(home() / "Applications" / "zdoom");
        executableFile(home() / "Games" / "zdoom" / "zdoom");
        executableFile(home() / ".local" / "share" / "flatpak" / "exports" / "bin" / "io.github.fabiangreffrath.Doom");
        executableFile(home() / ".local" / "share" / "flatpak" / "exports" / "bin" / "org.example.PrBoom-Plus");
        executableFile(Catalog::directory() / "gzdoom" / "gzdoom");
        ports::write(home() / "Games" / "readme.txt", "notes");

        ttk::Env::set("PATH", (bin().string() + ":" + (Catalog::directory() / "gzdoom").string()).c_str());

        std::ignore = Detect::ports();
        std::ignore = Detect::dosbox();

        ttk::Env::set("PATH", bin().c_str());
    }

    static void TearDownTestSuite() {
        for (const char *name : {"uzdoom", "dosbox-x", "dosbox-staging"}) {
            fs::remove(bin() / name);
        }
    }
};

class DetectOnPath : public ::testing::Test {
protected:
    std::vector<fs::path> placed;

    void TearDown() override {
        ttk::Env::set("PATH", bin().c_str());

        for (const fs::path &each : placed) {
            fs::remove_all(each);
        }
    }

    fs::path place(const fs::path &path) {
        placed.push_back(executableFile(path));

        return path;
    }
};

}

TEST_F(DetectPorts, APortOnThePathBeatsOneOnAShelf) {
    const Detect::Found *found = detected("uzdoom");

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->file, bin() / "uzdoom");
    EXPECT_EQ(found->name, "UZDoom");
    EXPECT_FALSE(found->dos);
}

TEST_F(DetectPorts, ADirectoryNamedForThePortIsSearched) {
    const Detect::Found *found = detected("dsda-doom");

    if (foreign(found)) {
        GTEST_SKIP() << found->file;
    }

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->file, home() / "Games" / "DSDA-Doom-0.29" / "dsda-doom");
}

TEST_F(DetectPorts, AnAppImageOnAShelfAnswersForThePortItIsNamedFor) {
    const Detect::Found *found = detected("woof");

    if (foreign(found)) {
        GTEST_SKIP() << found->file;
    }

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->file, home() / "Applications" / "Woof-15.0.0-x86_64.AppImage");
}

TEST_F(DetectPorts, AProgramLooseOnAShelfIsFound) {
    const Detect::Found *found = detected("helion");

    if (foreign(found)) {
        GTEST_SKIP() << found->file;
    }

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->file, home() / ".local" / "bin" / "helion");
}

TEST_F(DetectPorts, ADosPortIsFoundAsAnExe) {
    const Detect::Found *found = detected("boom");

    if (foreign(found)) {
        GTEST_SKIP() << found->file;
    }

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->file, home() / "Games" / "boom202" / "BOOM.EXE");
    EXPECT_TRUE(found->dos);
}

TEST_F(DetectPorts, ADirectoryIsNamedForTheIdNotTheProgram) {
    const Detect::Found *found = detected("doomlegacy");

    if (foreign(found)) {
        GTEST_SKIP() << found->file;
    }

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->file, home() / "Games" / "DoomLegacy" / "doom3.exe");
}

TEST_F(DetectPorts, OneLevelBelowAMarkedDirectoryIsSearched) {
    const Detect::Found *found = detected("nugget-doom");

    if (foreign(found)) {
        GTEST_SKIP() << found->file;
    }

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->file, home() / "Games" / "nugget-doom-1.2" / "bin" / "nugget-doom");
}

TEST_F(DetectPorts, TwoLevelsBelowIsNot) {
    const Detect::Found *found = detected("chocolate-doom");

    if (found != nullptr) {
        EXPECT_NE(found->file, home() / "Games" / "chocolate-doom" / "a" / "b" / "chocolate-doom");
    }
}

TEST_F(DetectPorts, AFlathubExportAnswersForThePortItRuns) {
    const Detect::Found *found = detected("crispy-doom");

    if (foreign(found)) {
        GTEST_SKIP() << found->file;
    }

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->file,
              home() / ".local" / "share" / "flatpak" / "exports" / "bin" / "io.github.fabiangreffrath.Doom");
}

TEST_F(DetectPorts, AnyOtherExportAnswersForTheLastPartOfItsAppId) {
    const Detect::Found *found = detected("prboom-plus");

    if (foreign(found)) {
        GTEST_SKIP() << found->file;
    }

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->file, home() / ".local" / "share" / "flatpak" / "exports" / "bin" / "org.example.PrBoom-Plus");
}

TEST(DetectFlatpak, AnExportLivesInFlatpakExportsBin) {
    EXPECT_TRUE(Detect::flatpakExport("/var/lib/flatpak/exports/bin/org.zdoom.UZDoom"));
    EXPECT_TRUE(Detect::flatpakExport("/home/player/.local/share/flatpak/exports/bin/org.zdoom.UZDoom"));
    EXPECT_FALSE(Detect::flatpakExport("/var/lib/flatpak/app/org.zdoom.UZDoom"));
    EXPECT_FALSE(Detect::flatpakExport("/usr/bin/uzdoom"));
}

TEST(DetectFlatpak, OnlyAWholeAppIdFromFlathubRunsAKnownExecutable) {
    EXPECT_EQ(Detect::flatpakExecutable("io.github.fabiangreffrath.Doom"), "crispy-doom");
    EXPECT_EQ(Detect::flatpakExecutable("org.zdoom.UZDoom"), "uzdoom");
    EXPECT_EQ(Detect::flatpakExecutable("org.chocolate_doom.ChocolateDoom"), "chocolate-doom");
    EXPECT_EQ(Detect::flatpakExecutable("com.zandronum.Zandronum"), "");
    EXPECT_EQ(Detect::flatpakExecutable("io.github.fabiangreffrath.Doom.Heretic"), "");
    EXPECT_EQ(Detect::flatpakExecutable("Doom"), "");
}

TEST_F(DetectPorts, APortOnTwoShelvesIsFoundOnTheFirst) {
    const Detect::Found *found = detected("zdoom");

    if (foreign(found)) {
        GTEST_SKIP() << found->file;
    }

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->file, home() / "Applications" / "zdoom");
}

TEST_F(DetectPorts, WhatZdlUnpackedIsLeftOut) {
    for (const Detect::Found &each : Detect::ports()) {
        EXPECT_FALSE(each.file.string().starts_with(Catalog::directory().string())) << each.file;
    }
}

TEST_F(DetectPorts, EachPortIsFoundOnce) {
    std::set<Port::Port> seen;

    for (const Detect::Found &each : Detect::ports()) {
        const std::string_view id = Port::known(each.port).id;

        EXPECT_TRUE(seen.insert(each.port).second) << id;
        EXPECT_NE(Catalog::find(id), nullptr) << id;
    }
}

TEST_F(DetectPorts, DosboxIsTheFirstOfItsNamesOnThePath) {
    EXPECT_EQ(Detect::dosbox(), bin() / "dosbox-x");
}

TEST_F(DetectPorts, OfFindsTheRowForAProgram) {
    const Detect::Found *found = Detect::of(bin() / "uzdoom");

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->port, Port::Port::uzdoom);
}

TEST_F(DetectPorts, OfFindsTheRowThroughALink) {
    const fs::path link = tests::Sandbox::scratch("detect-link") / "uzdoom";

    fs::create_symlink(bin() / "uzdoom", link);

    const Detect::Found *found = Detect::of(link);

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->port, Port::Port::uzdoom);
}

TEST_F(DetectPorts, OfAnythingElseIsNothing) {
    EXPECT_EQ(Detect::of(""), nullptr);
    EXPECT_EQ(Detect::of(bin() / "dosbox-x"), nullptr);
    EXPECT_EQ(Detect::of("/nowhere/gzdoom"), nullptr);
}

TEST(DetectSame, EmptyIsNeverTheSame) {
    EXPECT_FALSE(Detect::same("", ""));
    EXPECT_FALSE(Detect::same("/a", ""));
}

TEST(DetectSame, TheSameSpellingIsTheSameWhetherOrNotItExists) {
    EXPECT_TRUE(Detect::same("/nowhere/gzdoom", "/nowhere/gzdoom"));
    EXPECT_FALSE(Detect::same("/nowhere/gzdoom", "/nowhere/uzdoom"));
}

TEST(DetectSame, TwoNamesForOneFileAreTheSame) {
    const fs::path scratch = tests::Sandbox::scratch("detect-same");

    ports::write(scratch / "real", "x");
    fs::create_symlink(scratch / "real", scratch / "link");

    EXPECT_TRUE(Detect::same(scratch / "link", scratch / "real"));
    EXPECT_TRUE(Detect::same(scratch / "." / "real", scratch / "real"));
}

#ifndef _WIN32
TEST(DetectSame, CaseCountsOnThisSystem) {
    EXPECT_FALSE(Detect::same("/nowhere/GZDoom", "/nowhere/gzdoom"));
}
#endif

TEST_F(DetectOnPath, AProgramOnThePathIsFound) {
    place(bin() / "qzdl-test-port");

    EXPECT_EQ(Detect::onPath("qzdl-test-port"), bin() / "qzdl-test-port");
}

TEST_F(DetectOnPath, NothingByThatNameIsNothing) {
    EXPECT_EQ(Detect::onPath("qzdl-test-absent"), fs::path());
    EXPECT_EQ(Detect::onPath(""), fs::path());
}

TEST_F(DetectOnPath, ADirectoryByThatNameIsNotAProgram) {
    fs::create_directories(bin() / "qzdl-test-folder");
    placed.push_back(bin() / "qzdl-test-folder");

    EXPECT_EQ(Detect::onPath("qzdl-test-folder"), fs::path());
}

TEST_F(DetectOnPath, TheFirstDirectoryHoldingItWins) {
    const fs::path first = tests::Sandbox::scratch("detect-first");
    const fs::path second = tests::Sandbox::scratch("detect-second");

    executableFile(first / "port");
    executableFile(second / "port");
    ttk::Env::set("PATH", ("::" + second.string() + ":" + first.string()).c_str());

    EXPECT_EQ(Detect::onPath("port"), second / "port");
}

TEST_F(DetectOnPath, AnEmptyPathFindsNothing) {
    place(bin() / "qzdl-test-port");
    ttk::Env::set("PATH", "");

    EXPECT_EQ(Detect::onPath("qzdl-test-port"), fs::path());
}
