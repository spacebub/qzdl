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

fs::path program(const fs::path &path) {
    ports::write(path, "#!/bin/sh\n");
    fs::permissions(path, fs::perms::owner_all);

    return path;
}

const Detect::Found *found(const std::string &id) {
    for (const Detect::Found &each : Detect::ports()) {
        if (each.portId == id) {
            return &each;
        }
    }

    return nullptr;
}

// A port installed on this machine outside the sandbox takes precedence over the one placed here.
bool foreign(const Detect::Found *port) {
    return port != nullptr && !port->program.string().starts_with(tests::Sandbox::root().string());
}

// Detection is looked up once per run, so the machine it walks is laid out before the first case.
class DetectPorts : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        program(bin() / "uzdoom");
        program(home() / "Games" / "uzdoom" / "uzdoom");
        program(bin() / "dosbox-x");
        program(bin() / "dosbox-staging");
        program(home() / "Games" / "DSDA-Doom-0.29" / "dsda-doom");
        program(home() / "Applications" / "Woof-15.0.0-x86_64.AppImage");
        program(home() / ".local" / "bin" / "helion");
        program(home() / "Games" / "boom202" / "BOOM.EXE");
        program(home() / "Games" / "DoomLegacy" / "doom3.exe");
        program(home() / "Games" / "nugget-doom-1.2" / "bin" / "nugget-doom");
        program(home() / "Games" / "chocolate-doom" / "a" / "b" / "chocolate-doom");
        program(home() / "Applications" / "zdoom");
        program(home() / "Games" / "zdoom" / "zdoom");
        program(home() / ".local" / "share" / "flatpak" / "exports" / "bin" / "io.github.fabiangreffrath.crispy-doom");
        program(Catalog::directory() / "gzdoom" / "gzdoom");
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
        placed.push_back(program(path));

        return path;
    }
};

}

TEST_F(DetectPorts, APortOnThePathBeatsOneOnAShelf) {
    const Detect::Found *port = found("uzdoom");

    ASSERT_NE(port, nullptr);
    EXPECT_EQ(port->program, bin() / "uzdoom");
    EXPECT_EQ(port->name, "UZDoom");
    EXPECT_FALSE(port->dos);
}

TEST_F(DetectPorts, ADirectoryNamedForThePortIsSearched) {
    const Detect::Found *port = found("dsda-doom");

    if (foreign(port)) {
        GTEST_SKIP() << port->program;
    }

    ASSERT_NE(port, nullptr);
    EXPECT_EQ(port->program, home() / "Games" / "DSDA-Doom-0.29" / "dsda-doom");
}

TEST_F(DetectPorts, AnAppImageOnAShelfAnswersForThePortItIsNamedFor) {
    const Detect::Found *port = found("woof");

    if (foreign(port)) {
        GTEST_SKIP() << port->program;
    }

    ASSERT_NE(port, nullptr);
    EXPECT_EQ(port->program, home() / "Applications" / "Woof-15.0.0-x86_64.AppImage");
}

TEST_F(DetectPorts, AProgramLooseOnAShelfIsFound) {
    const Detect::Found *port = found("helion");

    if (foreign(port)) {
        GTEST_SKIP() << port->program;
    }

    ASSERT_NE(port, nullptr);
    EXPECT_EQ(port->program, home() / ".local" / "bin" / "helion");
}

TEST_F(DetectPorts, ADosPortIsFoundAsAnExe) {
    const Detect::Found *port = found("boom");

    if (foreign(port)) {
        GTEST_SKIP() << port->program;
    }

    ASSERT_NE(port, nullptr);
    EXPECT_EQ(port->program, home() / "Games" / "boom202" / "BOOM.EXE");
    EXPECT_TRUE(port->dos);
}

TEST_F(DetectPorts, ADirectoryIsNamedForTheIdNotTheProgram) {
    const Detect::Found *port = found("doomlegacy");

    if (foreign(port)) {
        GTEST_SKIP() << port->program;
    }

    ASSERT_NE(port, nullptr);
    EXPECT_EQ(port->program, home() / "Games" / "DoomLegacy" / "doom3.exe");
}

TEST_F(DetectPorts, OneLevelBelowAMarkedDirectoryIsSearched) {
    const Detect::Found *port = found("nugget-doom");

    if (foreign(port)) {
        GTEST_SKIP() << port->program;
    }

    ASSERT_NE(port, nullptr);
    EXPECT_EQ(port->program, home() / "Games" / "nugget-doom-1.2" / "bin" / "nugget-doom");
}

TEST_F(DetectPorts, TwoLevelsBelowIsNot) {
    const Detect::Found *port = found("chocolate-doom");

    if (port != nullptr) {
        EXPECT_NE(port->program, home() / "Games" / "chocolate-doom" / "a" / "b" / "chocolate-doom");
    }
}

TEST_F(DetectPorts, AFlatpakExportAnswersForItsPort) {
    const Detect::Found *port = found("crispy-doom");

    if (foreign(port)) {
        GTEST_SKIP() << port->program;
    }

    ASSERT_NE(port, nullptr);
    EXPECT_EQ(port->program,
              home() / ".local" / "share" / "flatpak" / "exports" / "bin" / "io.github.fabiangreffrath.crispy-doom");
}

TEST_F(DetectPorts, APortOnTwoShelvesIsFoundOnTheFirst) {
    const Detect::Found *port = found("zdoom");

    if (foreign(port)) {
        GTEST_SKIP() << port->program;
    }

    ASSERT_NE(port, nullptr);
    EXPECT_EQ(port->program, home() / "Applications" / "zdoom");
}

TEST_F(DetectPorts, WhatZdlUnpackedIsLeftOut) {
    for (const Detect::Found &each : Detect::ports()) {
        EXPECT_FALSE(each.program.string().starts_with(Catalog::directory().string())) << each.program;
    }
}

TEST_F(DetectPorts, EachPortIsFoundOnce) {
    std::set<std::string> seen;

    for (const Detect::Found &each : Detect::ports()) {
        EXPECT_TRUE(seen.insert(each.portId).second) << each.portId;
        EXPECT_NE(Catalog::find(each.portId), nullptr) << each.portId;
    }
}

TEST_F(DetectPorts, DosboxIsTheFirstOfItsNamesOnThePath) {
    EXPECT_EQ(Detect::dosbox(), bin() / "dosbox-x");
}

TEST_F(DetectPorts, OfFindsTheRowForAProgram) {
    const Detect::Found *port = Detect::of(bin() / "uzdoom");

    ASSERT_NE(port, nullptr);
    EXPECT_EQ(port->portId, "uzdoom");
}

TEST_F(DetectPorts, OfFindsTheRowThroughALink) {
    const fs::path link = tests::Sandbox::scratch("detect-link") / "uzdoom";

    fs::create_symlink(bin() / "uzdoom", link);

    const Detect::Found *port = Detect::of(link);

    ASSERT_NE(port, nullptr);
    EXPECT_EQ(port->portId, "uzdoom");
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

    program(first / "port");
    program(second / "port");
    ttk::Env::set("PATH", ("::" + second.string() + ":" + first.string()).c_str());

    EXPECT_EQ(Detect::onPath("port"), second / "port");
}

TEST_F(DetectOnPath, AnEmptyPathFindsNothing) {
    place(bin() / "qzdl-test-port");
    ttk::Env::set("PATH", "");

    EXPECT_EQ(Detect::onPath("qzdl-test-port"), fs::path());
}
