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

#include "ttk/system/Env.h"
#include "ttk/system/Process.h"

#include "Launch.h"
#include "core/launch/Arguments.h"
#include "core/launch/Dos.h"
#include "core/launch/Launcher.h"
#include "core/ports/Detect.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

using launch::Strings;

class LauncherTest : public ::testing::Test {
protected:
    fs::path scratch;
    fs::path record;
    fs::path port;
    Config config;

    void SetUp() override {
        fs::remove_all(launch::profiles());
        ttk::Env::set("DOOMWADPATH", "");
        ttk::Env::set("DOOMWADDIR", "");

        scratch = tests::Sandbox::scratch("launcher");
        record = scratch / "ran";
        port = launch::recorder(scratch / "port" / "gzdoom", record);
        config = launch::single(port);
    }

    void TearDown() override {
        ttk::Env::set("DOOMWADPATH", "");

        for (const fs::path &placed : onPath) {
            fs::remove(placed);
        }
    }

    bool launched(std::string *error = nullptr) {
        ttk::Process::Id id = 0;

        if (!Launcher::launch(config, &id, nullptr, error)) {
            return false;
        }

        EXPECT_EQ(launch::finish(id), ttk::Process::State::Finished);

        return true;
    }

    [[nodiscard]] Strings arguments() const { return launch::lines(record.string() + ".args"); }

    [[nodiscard]] fs::path directory() const { return launch::lines(record.string() + ".cwd").at(0); }

    [[nodiscard]] std::string doomwaddir() const { return launch::lines(record.string() + ".env").at(0); }

    [[nodiscard]] std::string doomwadpath() const { return launch::lines(record.string() + ".env").at(1); }

    fs::path placeOnPath(const std::string &name) {
        onPath.push_back(launch::recorder(tests::Sandbox::root() / "bin" / name, record));

        return onPath.back();
    }

    Profile &profile() { return config.activeProfile(); }

private:
    std::vector<fs::path> onPath;
};

}

TEST_F(LauncherTest, TheExecutableIsTheProfilesPort) {
    EXPECT_EQ(Launcher::executable(config), port);

    profile().port = "Nothing";

    EXPECT_EQ(Launcher::executable(config), fs::path());
}

TEST_F(LauncherTest, APortRunsUnderDosboxWhenItsRowSaysSo) {
    EXPECT_FALSE(Launcher::isDosPort(config));

    config.ports.front().dosbox = true;

    EXPECT_TRUE(Launcher::isDosPort(config));
}

TEST_F(LauncherTest, DosboxIsTheSettingWhenThereIsOne) {
    config.general.dosbox = "/somewhere/dosbox-x";

    EXPECT_EQ(Launcher::dosbox(config), fs::path("/somewhere/dosbox-x"));
}

TEST_F(LauncherTest, DosboxIsWhatDetectionFoundWhenUnset) {
    EXPECT_EQ(Launcher::dosbox(config), Detect::dosbox());
}

TEST_F(LauncherTest, NoPortIsRefused) {
    std::string error;

    profile().port.clear();

    EXPECT_FALSE(launched(&error));
    EXPECT_EQ(error, "No source port is selected.");
}

TEST_F(LauncherTest, APortThatIsNotThereIsRefusedWithTheReason) {
    std::string error;

    config.ports.front().file = (scratch / "gone" / "gzdoom").string();

    EXPECT_FALSE(launched(&error));
    EXPECT_FALSE(error.empty());
}

TEST_F(LauncherTest, APortIsHandedTheArgumentsBuiltForIt) {
    profile().skill = 3;
    profile().extra = "-fast 'two words'";

    ASSERT_TRUE(launched());

    EXPECT_EQ(arguments(), Arguments::of(config, port.parent_path()));
    EXPECT_EQ(arguments(), (Strings{"-iwad", tests::Corpus::iwad().string(), "-skill", "3", "-fast", "two words"}));
}

TEST_F(LauncherTest, AGameIsNotRequired) {
    profile().iwad.clear();

    ASSERT_TRUE(launched());

    EXPECT_TRUE(arguments().empty());
    EXPECT_EQ(doomwaddir(), "");
}

TEST_F(LauncherTest, WithoutAProfileFolderThePortRunsInItsOwnDirectory) {
    ASSERT_TRUE(launched());

    EXPECT_EQ(directory(), fs::canonical(port.parent_path()));
}

TEST_F(LauncherTest, AProfileWithAFolderRunsThere) {
    profile().config = "alpha.cfg";

    ASSERT_TRUE(launched());

    EXPECT_EQ(directory(), fs::canonical(launch::profiles() / "alpha"));
}

TEST_F(LauncherTest, DoomwaddirIsTheGamesDirectory) {
    ASSERT_TRUE(launched());

    EXPECT_EQ(doomwaddir(), tests::Corpus::iwad().parent_path().string());
}

TEST_F(LauncherTest, DoomwadpathListsEachGameDirectoryOnce) {
    const fs::path other = scratch / "other";

    launch::write(other / "heretic.wad", "IWAD");

    config.iwads.push_back({.name = "Again", .file = tests::Corpus::pk3().string()});
    config.iwads.push_back({.name = "Other", .file = (other / "heretic.wad").string()});
    config.iwads.push_back({.name = "Gone", .file = (scratch / "gone" / "x.wad").string()});

    ASSERT_TRUE(launched());

    EXPECT_EQ(doomwadpath(), tests::Corpus::iwad().parent_path().string() + ":" + other.string());
}

TEST_F(LauncherTest, DoomwadpathKeepsWhatTheEnvironmentAlreadyHas) {
    ttk::Env::set("DOOMWADPATH", "/shared/wads");

    ASSERT_TRUE(launched());

    EXPECT_EQ(doomwadpath(), "/shared/wads:" + tests::Corpus::iwad().parent_path().string());
}

TEST_F(LauncherTest, RunningAwayFromThePortPutsItsDirectoryFirstOnTheSearchPath) {
    profile().config = "alpha.cfg";

    ASSERT_TRUE(launched());

    EXPECT_EQ(doomwadpath(), port.parent_path().string() + ":" + tests::Corpus::iwad().parent_path().string());
}

TEST_F(LauncherTest, ProfileConfigsMakeTheConfigAndSaveFolders) {
    config.general.profileConfigs = true;
    profile().config = "alpha.cfg";

    ASSERT_TRUE(launched());

    EXPECT_TRUE(fs::is_directory(launch::profiles() / "alpha" / "saves"));
}

TEST_F(LauncherTest, RecordingMakesTheReplaysFolder) {
    profile().config = "alpha.cfg";
    profile().replay = {.mode = ReplayMode::Record, .file = "run1"};

    ASSERT_TRUE(launched());

    EXPECT_TRUE(fs::is_directory(launch::profiles() / "alpha" / "replays"));
}

TEST_F(LauncherTest, ATypedCommandRunsInsteadWithItsPlaceholdersFilled) {
    profile().customCommand = true;
    profile().command = "{source_port} -x {game}";

    ASSERT_TRUE(launched());

    EXPECT_EQ(arguments(), (Strings{"-x", tests::Corpus::iwad().string()}));
}

TEST_F(LauncherTest, ATypedCommandFindsABareProgramOnThePath) {
    placeOnPath("wrapper");

    profile().customCommand = true;
    profile().command = "wrapper -a";

    ASSERT_TRUE(launched());

    EXPECT_EQ(arguments(), (Strings{"-a"}));
}

TEST_F(LauncherTest, ATypedCommandThatCannotBeFilledIsRefusedWithWhy) {
    std::string error;

    profile().customCommand = true;
    profile().command = "{nonsense}";

    EXPECT_FALSE(launched(&error));
    EXPECT_TRUE(error.starts_with("{nonsense} is not one ZDL4 knows."));
}

TEST_F(LauncherTest, ATypedCommandNeedsNoPort) {
    placeOnPath("wrapper");

    profile().port.clear();
    profile().customCommand = true;
    profile().command = "wrapper";

    EXPECT_TRUE(launched());
}

TEST_F(LauncherTest, ATypedProgramNotOnThePathIsRefused) {
    std::string error;

    profile().customCommand = true;
    profile().command = "qzdl-no-such-program -a";

    EXPECT_FALSE(launched(&error));
    EXPECT_FALSE(error.empty());
}

TEST_F(LauncherTest, ATypedCommandMakesTheFoldersItNames) {
    config.general.profileConfigs = true;
    profile().config = "alpha.cfg";
    profile().customCommand = true;
    profile().command = "{source_port} {savedir} {replaydir}";

    ASSERT_TRUE(launched());

    EXPECT_TRUE(fs::is_directory(launch::profiles() / "alpha" / "saves"));
    EXPECT_TRUE(fs::is_directory(launch::profiles() / "alpha" / "replays"));
}

TEST_F(LauncherTest, ADosPortIsHandedToDosbox) {
    const fs::path dos = scratch / "dos";
    const fs::path dosbox = launch::recorder(scratch / "dosbox", record);

    launch::write(dos / "DOOM2.EXE", "MZ");
    config.ports.front() = {.name = "Port", .file = (dos / "DOOM2.EXE").string(), .dosbox = true};
    config.general.dosbox = dosbox.string();

    ASSERT_TRUE(launched());

    EXPECT_EQ(arguments(), Dos::command(config).arguments);
    EXPECT_EQ(directory(), fs::canonical(dos));
}
