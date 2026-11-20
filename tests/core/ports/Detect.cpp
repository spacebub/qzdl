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
#include <filesystem>
#include <set>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Env.h"
#include "ttk/system/Paths.h"

#include "Zip.h"
#include "core/config/Config.h"
#include "core/launch/KnownPorts.h"
#include "core/ports/Catalog.h"
#include "core/ports/Detect.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

using ports::program;

#ifdef _WIN32
constexpr const char *SEPARATOR = ";";
#else
constexpr const char *SEPARATOR = ":";
#endif

fs::path home() {
    return ttk::Paths::home_directory();
}

fs::path bin() {
    return tests::Sandbox::root() / "bin";
}

#ifdef _WIN32
// The first Windows shelf the suite uses, ahead of Games in the home directory.
fs::path programs() {
    return fs::path(ttk::Env::get("LOCALAPPDATA")) / "Programs";
}

fs::path programFiles() {
    return ttk::Env::get("ProgramFiles");
}
#endif

fs::path executableFile(const fs::path &path) {
    ports::write(path, "#!/bin/sh\n");
    fs::permissions(path, fs::perms::owner_all);

    return path;
}

// What detection found on the machine the suite laid out.
std::vector<Detect::Found> &walked() {
    static std::vector<Detect::Found> found;

    return found;
}

const Detect::Found *detected(const std::string &id) {
    for (const Detect::Found &each : walked()) {
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

// The machine is walked once, with the unpacked GZDoom on the path, so that the cases share what was found.
class DetectPorts : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        executableFile(bin() / program("uzdoom"));
        executableFile(home() / "Games" / "uzdoom" / program("uzdoom"));
        executableFile(bin() / program("dosbox-x"));
        executableFile(bin() / program("dosbox-staging"));
        executableFile(home() / "Games" / "DSDA-Doom-0.29" / program("dsda-doom"));
        executableFile(home() / "Games" / "boom202" / "BOOM.EXE");
        executableFile(home() / "Games" / "DoomLegacy" / "doom3.exe");
        executableFile(home() / "Games" / "nugget-doom-1.2" / "bin" / program("nugget-doom"));
        executableFile(home() / "Games" / "chocolate-doom" / "a" / "b" / program("chocolate-doom"));
        executableFile(home() / "Games" / "zdoom" / program("zdoom"));
        executableFile(Catalog::directory() / "gzdoom" / program("gzdoom"));
        ports::write(home() / "Games" / "readme.txt", "notes");
#ifdef _WIN32
        executableFile(programs() / "helion.exe");
        executableFile(programs() / "ZDoom" / "zdoom.exe");
        executableFile(programFiles() / "Woof" / "woof.exe");
        executableFile(home() / "scoop" / "apps" / "crispy-doom" / "current" / "crispy-doom.exe");
        executableFile(home() / "Games" / "zandronum" / "zandronum");
#else
        executableFile(home() / "Applications" / "Woof-15.0.0-x86_64.AppImage");
        executableFile(home() / ".local" / "bin" / "helion");
        executableFile(home() / "Applications" / "zdoom");
        executableFile(home() / ".local" / "share" / "flatpak" / "exports" / "bin" / "io.github.fabiangreffrath.Doom");
        executableFile(home() / ".local" / "share" / "flatpak" / "exports" / "bin" / "org.example.PrBoom-Plus");
#endif

        ttk::Env::set("PATH", (bin().string() + SEPARATOR + (Catalog::directory() / "gzdoom").string()).c_str());

        walked() = Detect::ports();
        std::ignore = Detect::dosbox();

        ttk::Env::set("PATH", bin().string().c_str());
    }

    static void TearDownTestSuite() {
        for (const char *name : {"uzdoom", "dosbox-x", "dosbox-staging"}) {
            fs::remove(bin() / program(name));
        }
    }
};

class DetectOnPath : public ::testing::Test {
protected:
    std::vector<fs::path> placed;

    void TearDown() override {
        ttk::Env::set("PATH", bin().string().c_str());

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
    EXPECT_EQ(found->file, bin() / program("uzdoom"));
    EXPECT_EQ(found->name, "UZDoom");
    EXPECT_FALSE(found->dos);
}

TEST_F(DetectPorts, ADirectoryNamedForThePortIsSearched) {
    const Detect::Found *found = detected("dsda-doom");

    if (foreign(found)) {
        GTEST_SKIP() << found->file;
    }

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->file, home() / "Games" / "DSDA-Doom-0.29" / program("dsda-doom"));
}

#ifdef _WIN32

TEST_F(DetectPorts, AProgramLooseOnAShelfIsFound) {
    const Detect::Found *found = detected("helion");

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->file, programs() / "helion.exe");
}

TEST_F(DetectPorts, ProgramFilesIsAShelf) {
    const Detect::Found *found = detected("woof");

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->file, programFiles() / "Woof" / "woof.exe");
}

TEST_F(DetectPorts, AScoopAppIsFoundInItsCurrentFolder) {
    const Detect::Found *found = detected("crispy-doom");

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->file, home() / "scoop" / "apps" / "crispy-doom" / "current" / "crispy-doom.exe");
}

TEST_F(DetectPorts, OnAShelfOnlyAnExeIsAPort) {
    EXPECT_EQ(detected("zandronum"), nullptr);
}

#else

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

#endif

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
    EXPECT_EQ(found->file, home() / "Games" / "nugget-doom-1.2" / "bin" / program("nugget-doom"));
}

TEST_F(DetectPorts, TwoLevelsBelowIsNot) {
    const Detect::Found *found = detected("chocolate-doom");

    if (found != nullptr) {
        EXPECT_NE(found->file, home() / "Games" / "chocolate-doom" / "a" / "b" / program("chocolate-doom"));
    }
}

#ifdef __linux__
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
#endif

TEST_F(DetectPorts, APortOnTwoShelvesIsFoundOnTheFirst) {
    const Detect::Found *found = detected("zdoom");

    if (foreign(found)) {
        GTEST_SKIP() << found->file;
    }

    ASSERT_NE(found, nullptr);
#ifdef _WIN32
    EXPECT_EQ(found->file, programs() / "ZDoom" / "zdoom.exe");
#else
    EXPECT_EQ(found->file, home() / "Applications" / "zdoom");
#endif
}

TEST_F(DetectPorts, WhatZdlUnpackedIsLeftOut) {
    for (const Detect::Found &each : walked()) {
        EXPECT_FALSE(each.file.string().starts_with(Catalog::directory().string())) << each.file;
    }
}

TEST_F(DetectPorts, EachPortIsFoundOnce) {
    std::set<Port::Port> seen;

    for (const Detect::Found &each : walked()) {
        const std::string_view id = Port::known(each.port).id;

        EXPECT_TRUE(seen.insert(each.port).second) << id;
        EXPECT_NE(Catalog::find(id), nullptr) << id;
    }
}

TEST_F(DetectPorts, DosboxIsTheFirstOfItsNamesOnThePath) {
    EXPECT_EQ(Detect::dosbox(), bin() / program("dosbox-x"));
}

TEST_F(DetectPorts, TheWalkSeesAPortPutOnThePathSince) {
    const fs::path added = executableFile(bin() / program("helion"));

    const std::vector<Detect::Found> found = Detect::ports();

    fs::remove(added);

    EXPECT_TRUE(std::ranges::any_of(found, [&added](const Detect::Found &each) { return each.file == added; }));
}

TEST(DetectUnlisted, AnEmptyListGetsEveryFoundPort) {
    const std::vector<Detect::Found> found = {
            {.port = Port::Port::woof, .name = "Woof!", .file = "/nowhere/woof", .dos = false},
            {.port = Port::Port::boom, .name = "Boom", .file = "/nowhere/BOOM.EXE", .dos = true},
    };

    const std::vector<Engine> added = Detect::unlisted({}, found);

    ASSERT_EQ(added.size(), 2U);
    EXPECT_EQ(added[0].file, "/nowhere/woof");
    EXPECT_EQ(added[0].port, Port::Port::woof);
    EXPECT_EQ(added[0].name, "Woof!");
    EXPECT_FALSE(added[0].dosbox);
    EXPECT_EQ(added[1].file, "/nowhere/BOOM.EXE");
    EXPECT_TRUE(added[1].dosbox);
}

TEST(DetectUnlisted, TheEnginesAreMarkedDetectedAndNotManaged) {
    const std::vector<Detect::Found> found = {
            {.port = Port::Port::woof, .name = "Woof!", .file = "/nowhere/woof", .dos = false},
    };

    const std::vector<Engine> added = Detect::unlisted({}, found);

    ASSERT_EQ(added.size(), 1U);
    EXPECT_TRUE(added.front().detected);
    EXPECT_FALSE(added.front().managed);
    EXPECT_EQ(added.front().version, "");
}

TEST(DetectUnlisted, AListedFileIsSkipped) {
    const std::vector<Engine> listed = {{.name = "Mine", .file = "/nowhere/woof"}};
    const std::vector<Detect::Found> found = {
            {.port = Port::Port::woof, .name = "Woof!", .file = "/nowhere/woof", .dos = false},
            {.port = Port::Port::helion, .name = "Helion", .file = "/nowhere/helion", .dos = false},
    };

    const std::vector<Engine> added = Detect::unlisted(listed, found);

    ASSERT_EQ(added.size(), 1U);
    EXPECT_EQ(added.front().file, "/nowhere/helion");
}

TEST(DetectUnlisted, ASecondInstallOfAListedPortIsAdded) {
    const std::vector<Engine> listed = {
            Detect::engine({.port = Port::Port::woof, .name = "Woof!", .file = "/nowhere/native/woof", .dos = false})};
    const std::vector<Detect::Found> found = {
            {.port = Port::Port::woof, .name = "Woof!", .file = "/nowhere/flatpak/woof", .dos = false},
    };

    const std::vector<Engine> added = Detect::unlisted(listed, found);

    ASSERT_EQ(added.size(), 1U);
    EXPECT_EQ(added.front().file, "/nowhere/flatpak/woof");
}

TEST(DetectUnlisted, AnEngineBuiltFromAFoundPortCountsAsListedForIt) {
    const Detect::Found woof{.port = Port::Port::woof, .name = "Woof!", .file = "/nowhere/woof", .dos = false};
    const std::vector<Engine> listed = Detect::unlisted({}, std::span(&woof, 1));

    ASSERT_EQ(listed.size(), 1U);
    EXPECT_TRUE(Detect::unlisted(listed, std::span(&woof, 1)).empty());
}

TEST(DetectUnlisted, AListedLinkToTheFoundFileCountsAsListed) {
    const fs::path scratch = tests::Sandbox::scratch("detect-unlisted-link");

    executableFile(scratch / "woof");
    fs::create_symlink(scratch / "woof", scratch / "link");

    const std::vector<Engine> listed = {{.name = "Mine", .file = (scratch / "link").string()}};
    const std::vector<Detect::Found> found = {
            {.port = Port::Port::woof, .name = "Woof!", .file = scratch / "woof", .dos = false},
    };

    EXPECT_TRUE(Detect::unlisted(listed, found).empty());
}

TEST(DetectUnlisted, TwoFoundPortsAtOneFileAddOneEngine) {
    const std::vector<Detect::Found> found = {
            {.port = Port::Port::gzdoom, .name = "GZDoom", .file = "/nowhere/doom", .dos = false},
            {.port = Port::Port::uzdoom, .name = "UZDoom", .file = "/nowhere/doom", .dos = false},
    };

    EXPECT_EQ(Detect::unlisted({}, found).size(), 1U);
}

TEST(DetectStillUnlisted, AFileListedSinceIsDropped) {
    const std::vector<Engine> fresh = {
            {.name = "Woof!", .file = "/nowhere/woof"},
            {.name = "Helion", .file = "/nowhere/helion"},
    };
    const std::vector<Engine> listed = {{.name = "Mine", .file = "/nowhere/woof"}};

    const std::vector<Engine> still = Detect::stillUnlisted(listed, {}, fresh);

    ASSERT_EQ(still.size(), 1U);
    EXPECT_EQ(still.front().file, "/nowhere/helion");
    EXPECT_EQ(still.front().name, "Helion");
}

TEST(DetectStillUnlisted, AnEmptyListKeepsEveryFreshEngine) {
    const std::vector<Engine> fresh = {{.name = "Woof!", .file = "/nowhere/woof", .detected = true}};

    const std::vector<Engine> still = Detect::stillUnlisted({}, {}, fresh);

    ASSERT_EQ(still.size(), 1U);
    EXPECT_TRUE(still.front().detected);
}

TEST(DetectStillUnlisted, ALinkListedSinceTheWalkCountsAsListed) {
    const fs::path scratch = tests::Sandbox::scratch("detect-still-since");

    executableFile(scratch / "woof");
    fs::create_symlink(scratch / "woof", scratch / "link");

    const std::vector<Engine> fresh = {{.name = "Woof!", .file = (scratch / "woof").string()}};
    const std::vector<Engine> listed = {{.name = "Mine", .file = (scratch / "link").string()}};

    EXPECT_TRUE(Detect::stillUnlisted(listed, {}, fresh).empty());
}

TEST(DetectStillUnlisted, AnEngineTheWalkComparedIsNotAskedAgain) {
    const fs::path scratch = tests::Sandbox::scratch("detect-still-compared");

    executableFile(scratch / "woof");
    fs::create_symlink(scratch / "woof", scratch / "link");

    const std::vector<Engine> fresh = {{.name = "Woof!", .file = (scratch / "woof").string()}};
    const std::vector<Engine> listed = {{.name = "Mine", .file = (scratch / "link").string()}};

    // Fresh never holds a file that a compared engine links to, so the disk is not asked again.
    EXPECT_EQ(Detect::stillUnlisted(listed, listed, fresh).size(), 1U);
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
    ttk::Env::set("PATH", (std::string(SEPARATOR) + SEPARATOR + second.string() + SEPARATOR + first.string()).c_str());

    EXPECT_EQ(Detect::onPath("port"), second / "port");
}

TEST_F(DetectOnPath, AnEmptyPathFindsNothing) {
    place(bin() / "qzdl-test-port");
    ttk::Env::set("PATH", "");

    EXPECT_EQ(Detect::onPath("qzdl-test-port"), fs::path());
}

#ifdef _WIN32

TEST_F(DetectOnPath, OnWindowsAProgramIsFoundWithoutItsExtension) {
    place(bin() / "qzdl-test-port.exe");

    EXPECT_EQ(Detect::onPath("qzdl-test-port"), bin() / "qzdl-test-port.exe");
}

TEST_F(DetectOnPath, OnWindowsABatchFileOnThePathCounts) {
    place(bin() / "qzdl-test-wrapper.cmd");

    EXPECT_EQ(Detect::onPath("qzdl-test-wrapper"), bin() / "qzdl-test-wrapper.cmd");
}

TEST_F(DetectOnPath, OnWindowsAnExeIsPreferredToABatchFileBesideIt) {
    place(bin() / "qzdl-test-both.bat");
    place(bin() / "qzdl-test-both.exe");

    EXPECT_EQ(Detect::onPath("qzdl-test-both"), bin() / "qzdl-test-both.exe");
}

TEST(DetectSame, OnWindowsCaseDoesNotCount) {
    EXPECT_TRUE(Detect::same("C:/nowhere/GZDoom.exe", "c:\\nowhere\\gzdoom.exe"));
}

#endif
