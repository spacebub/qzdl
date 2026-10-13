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
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Process.h"

#include "Launch.h"
#include "core/launch/Dos.h"
#include "core/ports/Detect.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

using launch::Strings;

std::string mounted(const char drive, const fs::path &directory) {
    return std::string("mount ") + drive + " " + fs::weakly_canonical(directory).string();
}

Strings commands(const Dos::Command &command) {
    Strings out;

    for (size_t at = 0; at + 1 < command.arguments.size(); at++) {
        if (command.arguments[at] == "-c") {
            out.push_back(command.arguments[at + 1]);
        }
    }

    return out;
}

std::string run(const Dos::Command &command) {
    const Strings all = commands(command);
    const auto change = std::ranges::find(all, "c:");

    return change == all.end() || change + 1 == all.end() ? std::string() : *(change + 1);
}

class DosTest : public ::testing::Test {
protected:
    fs::path scratch;
    fs::path port;
    fs::path dosbox;

    void SetUp() override {
        fs::remove_all(launch::profiles());

        scratch = tests::Sandbox::scratch("dos");
        port = scratch / "port";
        dosbox = scratch / "dosbox";

        launch::write(dosbox, "");
        fs::create_directories(port);
        fs::copy_file(tests::Corpus::iwad(), port / "doom2.wad");
    }

    [[nodiscard]] Config on(const char *program) const {
        launch::write(port / program, "MZ");

        Config config = launch::single(port / program, true);

        config.general.dosbox = dosbox.string();
        config.iwads.front().file = (port / "doom2.wad").string();

        return config;
    }

    static Dos::Command built(const Config &config) {
        std::string error;
        Dos::Command command = Dos::command(config, &error);

        EXPECT_EQ(error, "");

        return command;
    }

    static std::string refusal(const Config &config) {
        std::string error;
        const Dos::Command command = Dos::command(config, &error);

        EXPECT_TRUE(command.arguments.empty());

        return error;
    }

    [[nodiscard]] fs::path addon(const std::string &directory, const std::string &name) const {
        const fs::path file = scratch / directory / name;

        launch::write(file, "PWAD");

        return file;
    }
};

}

TEST_F(DosTest, ABareLaunchMountsThePortAsCAndRunsItThere) {
    const Dos::Command command = built(on("DOOM2.EXE"));

    EXPECT_EQ(command.dosbox, dosbox);
    EXPECT_EQ(command.arguments,
              (Strings{"-c", mounted('c', port), "-c", "c:", "-c", "DOOM2.EXE", "-c", "exit", "-fullscreen"}));
}

TEST_F(DosTest, NoDosboxAnywhereIsRefused) {
    if (!Detect::dosbox().empty()) {
        GTEST_SKIP() << "a DOSBox outside the sandbox was found at " << Detect::dosbox();
    }

    Config config = on("DOOM2.EXE");

    config.general.dosbox.clear();

    EXPECT_EQ(refusal(config),
              "This machine has no DOSBox on it, and none is set. A DOS source port needs one to run in, which goes in "
              "Settings.");
}

TEST_F(DosTest, ADosboxThatIsGoneIsRefused) {
    Config config = on("DOOM2.EXE");

    fs::remove(dosbox);

    EXPECT_EQ(refusal(config), "DOSBox is not at " + dosbox.string() + " any more.");
}

TEST_F(DosTest, APortNameDosCannotSpellIsRefused) {
    EXPECT_EQ(refusal(on("chocolate-doom.exe")),
              "DOSBox renames chocolate-doom.exe on the way in, and then there is nothing there by that name to run. "
              "Eight characters and three is all DOS can spell.");
}

TEST_F(DosTest, ABatchFileIsCalledSoTheShellCarriesOn) {
    EXPECT_EQ(run(built(on("PLAY.BAT"))), "call PLAY.BAT");
    EXPECT_EQ(run(built(on("play.cmd"))), "call play.cmd");
}

TEST_F(DosTest, ExitIsLeftOutWhenDosboxShouldStayOpen) {
    Config config = on("DOOM2.EXE");

    config.activeProfile().dosExit = false;

    EXPECT_FALSE(std::ranges::contains(commands(built(config)), "exit"));
}

TEST_F(DosTest, ExitComesAfterTheRun) {
    const Strings all = commands(built(on("DOOM2.EXE")));

    ASSERT_FALSE(all.empty());
    EXPECT_EQ(all.back(), "exit");
}

TEST_F(DosTest, FullscreenIsLeftOutForAWindow) {
    Config config = on("DOOM2.EXE");

    config.activeProfile().dosFullscreen = false;

    EXPECT_FALSE(std::ranges::contains(built(config).arguments, "-fullscreen"));
}

TEST_F(DosTest, ATuningFileBesideThePortIsLoadedFirst) {
    launch::write(port / "dosbox.conf", "[cpu]\ncycles=max\n");

    const Dos::Command command = built(on("DOOM2.EXE"));

    ASSERT_GE(command.arguments.size(), 2U);
    EXPECT_EQ(command.arguments[0], "-conf");
    EXPECT_EQ(command.arguments[1], (port / "dosbox.conf").string());
}

TEST_F(DosTest, ADirectoryWithASpaceIsQuotedInItsMount) {
    port = scratch / "my port";

    fs::create_directories(port);
    fs::copy_file(tests::Corpus::iwad(), port / "doom2.wad");

    EXPECT_EQ(commands(built(on("DOOM2.EXE"))).front(), "mount c \"" + fs::weakly_canonical(port).string() + "\"");
}

TEST_F(DosTest, AGameElsewhereUnderTheNameThePortWantsIsPointedAt) {
    Config config = on("DOOM2.EXE");
    const fs::path games = scratch / "games";

    fs::create_directories(games);
    fs::copy_file(tests::Corpus::iwad(), games / "doom2.wad");
    config.iwads.front().file = (games / "doom2.wad").string();

    EXPECT_EQ(commands(built(config)),
              (Strings{mounted('c', port), mounted('d', games), "set DOOMWADDIR=d:", "c:", "DOOM2.EXE", "exit"}));
}

TEST_F(DosTest, AGameUnderAnotherNameIsStagedIntoTheProfilesFolder) {
    Config config = on("DOOM2.EXE");

    config.iwads.front().file = tests::Corpus::iwad().string();
    config.activeProfile().config = "alpha.cfg";

    EXPECT_EQ(commands(built(config)),
              (Strings{mounted('c', port),
                       mounted('d', launch::profiles() / "alpha"),
                       "set DOOMWADDIR=d:",
                       "c:",
                       "DOOM2.EXE",
                       "exit"}));
}

TEST_F(DosTest, APortThatTakesAConfigIsHandedOneInTheProfilesFolder) {
    Config config = on("MBF.EXE");

    config.activeProfile().config = "alpha.cfg";

    EXPECT_EQ(run(built(config)), "MBF.EXE -config d:\\zdl.cfg -iwad c:\\doom2.wad");
}

TEST_F(DosTest, ALongFileNameIsReachedThroughAShortCopy) {
    Config config = on("MBF.EXE");

    config.activeProfile().files = {{.file = addon("mods", "my-long-addon-name.wad").string()}};

    EXPECT_EQ(run(built(config)), "MBF.EXE -iwad c:\\doom2.wad -file c:\\dosfiles\\my-long-.wad");
}

TEST_F(DosTest, TwoLongNamesAlikeAreNumberedApart) {
    Config config = on("MBF.EXE");

    config.activeProfile().files = {{.file = addon("mods", "my-long-addon-one.wad").string()},
                                    {.file = addon("mods", "my-long-addon-two.wad").string()}};

    EXPECT_TRUE(run(built(config)).ends_with("-file c:\\dosfiles\\my-long-.wad c:\\dosfiles\\my-long1.wad"));
}

TEST_F(DosTest, AShortFileNameIsReachedWhereItIs) {
    Config config = on("MBF.EXE");

    config.activeProfile().files = {{.file = addon("mods", "short.wad").string()}};

    const Dos::Command command = built(config);

    EXPECT_TRUE(std::ranges::contains(commands(command), mounted('d', scratch / "mods")));
    EXPECT_TRUE(run(command).ends_with("-file d:\\short.wad"));
}

TEST_F(DosTest, FilesInOneDirectoryShareADrive) {
    Config config = on("MBF.EXE");

    config.activeProfile().files = {{.file = addon("mods", "one.wad").string()},
                                    {.file = addon("mods", "two.wad").string()}};

    EXPECT_TRUE(run(built(config)).ends_with("-file d:\\one.wad d:\\two.wad"));
}

TEST_F(DosTest, AMissingFileIsPassedAsWritten) {
    Config config = on("MBF.EXE");

    config.activeProfile().files = {{.file = "/nowhere/gone.wad"}};

    EXPECT_TRUE(run(built(config)).ends_with("-file /nowhere/gone.wad"));
}

TEST_F(DosTest, AFolderIsRefused) {
    Config config = on("MBF.EXE");
    const fs::path folder = scratch / "folder";

    fs::create_directories(folder);
    config.activeProfile().files = {{.file = folder.string()}};

    EXPECT_EQ(refusal(config),
              "A DOS port cannot load a folder. " + folder.string()
                      + " would have to be a WAD or a PK3 for this profile to launch.");
}

TEST_F(DosTest, EightDrivesFitAndSpendAllElevenCommands) {
    Config config = on("DOOM2.EXE");

    for (int at = 0; at < 7; at++) {
        config.activeProfile().files.push_back({.file = addon("dir" + std::to_string(at), "a.wad").string()});
    }

    const Dos::Command command = built(config);

    EXPECT_EQ(Dos::spent(command), Dos::COMMANDS);
    EXPECT_TRUE(std::ranges::contains(commands(command), mounted('j', scratch / "dir6")));
}

TEST_F(DosTest, ANinthDirectoryIsRefused) {
    Config config = on("DOOM2.EXE");

    for (int at = 0; at < 8; at++) {
        config.activeProfile().files.push_back({.file = addon("dir" + std::to_string(at), "a.wad").string()});
    }

    EXPECT_TRUE(
            refusal(config).starts_with("This launch reaches into more directories than DOSBox will mount at once."));
}

TEST_F(DosTest, PointingAtTheGameCostsADrive) {
    Config config = on("DOOM2.EXE");
    const fs::path games = scratch / "games";

    fs::create_directories(games);
    fs::copy_file(tests::Corpus::iwad(), games / "doom2.wad");
    config.iwads.front().file = (games / "doom2.wad").string();

    for (int at = 0; at < 5; at++) {
        config.activeProfile().files.push_back({.file = addon("dir" + std::to_string(at), "a.wad").string()});
    }

    EXPECT_EQ(Dos::spent(built(config)), Dos::COMMANDS);

    config.activeProfile().files.push_back({.file = addon("dir5", "a.wad").string()});

    EXPECT_TRUE(refusal(config).starts_with("This launch reaches into more directories"));
}

TEST_F(DosTest, StayingOpenFreesNoDriveBeyondJ) {
    Config config = on("DOOM2.EXE");

    config.activeProfile().dosExit = false;

    for (int at = 0; at < 8; at++) {
        config.activeProfile().files.push_back({.file = addon("dir" + std::to_string(at), "a.wad").string()});
    }

    EXPECT_TRUE(refusal(config).starts_with("This launch reaches into more directories"));
}

TEST_F(DosTest, ALineTooLongForDosGoesIntoAResponseFile) {
    Config config = on("DOOM2.EXE");
    std::string extra;

    for (int at = 0; at < 30; at++) {
        extra += " -switch" + std::to_string(at);
    }

    config.activeProfile().extra = extra;

    EXPECT_EQ(run(built(config)), "DOOM2.EXE @c:\\zdl.rsp");
}

TEST_F(DosTest, ALineThatFitsStaysOnTheLine) {
    Config config = on("DOOM2.EXE");

    config.activeProfile().extra = "-nomusic -nosfx";

    EXPECT_EQ(run(built(config)), "DOOM2.EXE -nomusic -nosfx");
}

TEST_F(DosTest, RecordingWritesIntoTheProfilesReplaysFolder) {
    Config config = on("DOOM2.EXE");

    config.activeProfile().config = "alpha.cfg";
    config.activeProfile().replay = {.mode = ReplayMode::Record, .file = "demo1"};

    const Dos::Command command = built(config);

    EXPECT_EQ(run(command), "DOOM2.EXE -record d:\\replays\\demo1");
    EXPECT_TRUE(std::ranges::contains(commands(command), mounted('d', launch::profiles() / "alpha")));
}

TEST_F(DosTest, ADemoNameDosCannotSpellIsRefused) {
    Config config = on("DOOM2.EXE");

    config.activeProfile().config = "alpha.cfg";
    config.activeProfile().replay = {.mode = ReplayMode::Record, .file = "a-very-long-demo"};

    EXPECT_EQ(refusal(config),
              "DOS cannot spell a-very-long-demo.lmp, so there would be nothing by that name to record into. Eight "
              "characters and three is all it can spell.");
}

TEST_F(DosTest, SpentCountsOnlyTheDashCs) {
    const Dos::Command command{.dosbox = "dosbox", .arguments = {"-conf", "x", "-c", "a", "-c", "b", "-fullscreen"}};

    EXPECT_EQ(Dos::spent(command), 2);
    EXPECT_EQ(Dos::spent({}), 0);
}

class DosStart : public DosTest {
protected:
    fs::path record;

    void SetUp() override {
        DosTest::SetUp();

        record = scratch / "ran";
        launch::recorder(dosbox, record);
    }

    bool start(const Config &config, std::string *error = nullptr) const {
        ttk::Process::Id id = 0;

        if (!Dos::start(config, &id, nullptr, error)) {
            return false;
        }

        EXPECT_EQ(launch::finish(id), ttk::Process::State::Finished);

        return true;
    }
};

TEST_F(DosStart, RunsDosboxFromThePortsDirectoryWithTheBuiltCommand) {
    const Config config = on("DOOM2.EXE");

    ASSERT_TRUE(start(config));

    EXPECT_EQ(launch::lines(record.string() + ".args"), Dos::command(config).arguments);
    EXPECT_EQ(fs::path(launch::lines(record.string() + ".cwd").front()), fs::canonical(port));
}

TEST_F(DosStart, CopiesAStagedGameUnderTheNameThePortSearchesFor) {
    Config config = on("DOOM2.EXE");

    config.iwads.front().file = tests::Corpus::iwad().string();
    config.activeProfile().config = "alpha.cfg";

    ASSERT_TRUE(start(config));

    const fs::path staged = launch::profiles() / "alpha" / "doom2.wad";

    ASSERT_TRUE(fs::is_regular_file(staged));
    EXPECT_EQ(fs::file_size(staged), fs::file_size(tests::Corpus::iwad()));
}

TEST_F(DosStart, CopiesALongNameUnderItsShortOne) {
    Config config = on("MBF.EXE");
    const fs::path file = addon("mods", "my-long-addon-name.wad");

    config.activeProfile().files = {{.file = file.string()}};

    ASSERT_TRUE(start(config));

    EXPECT_EQ(launch::slurp(port / "dosfiles" / "my-long-.wad"), "PWAD");
}

TEST_F(DosStart, WritesTheResponseFileOneArgumentPerLine) {
    Config config = on("DOOM2.EXE");
    std::string extra;
    Strings expected;

    for (int at = 0; at < 30; at++) {
        extra += " -switch" + std::to_string(at);
        expected.push_back("-switch" + std::to_string(at));
    }

    config.activeProfile().extra = extra;

    ASSERT_TRUE(start(config));

    EXPECT_EQ(launch::lines(port / "zdl.rsp"), expected);
}

TEST_F(DosStart, PrunesCopiesAndGamesThisLaunchDidNotStage) {
    Config config = on("DOOM2.EXE");
    const fs::path own = launch::profiles() / "alpha";

    config.iwads.front().file = tests::Corpus::iwad().string();
    config.activeProfile().config = "alpha.cfg";

    launch::write(own / "dosfiles" / "old.wad", "PWAD");
    launch::write(own / "tnt.wad", "IWAD");
    launch::write(own / "zdl.cfg", "settings");

    ASSERT_TRUE(start(config));

    EXPECT_FALSE(fs::exists(own / "dosfiles" / "old.wad"));
    EXPECT_FALSE(fs::exists(own / "tnt.wad"));
    EXPECT_TRUE(fs::exists(own / "zdl.cfg"));
    EXPECT_TRUE(fs::exists(own / "doom2.wad"));
}

TEST_F(DosStart, LeavesThePortsOwnWadsAloneWithoutAProfileFolder) {
    launch::write(port / "music.wad", "PWAD");

    ASSERT_TRUE(start(on("DOOM2.EXE")));

    EXPECT_TRUE(fs::exists(port / "music.wad"));
}

TEST_F(DosStart, RefusesWhatTheCommandRefuses) {
    std::string error;

    EXPECT_FALSE(start(on("chocolate-doom.exe"), &error));
    EXPECT_TRUE(error.starts_with("DOSBox renames chocolate-doom.exe"));
    EXPECT_FALSE(fs::exists(record.string() + ".args"));
}
