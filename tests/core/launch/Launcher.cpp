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

    [[nodiscard]] std::string probe() const { return launch::lines(record.string() + ".env").at(2); }

    fs::path placeOnPath(const std::string &name) {
        onPath.push_back(launch::recorder(tests::Sandbox::root() / "bin" / name, record));

        return onPath.back();
    }

    Profile &profile() { return config.activeProfile(); }

    // A config under the profile name, written by a port outside the ZDoom family.
    void oldConfigFor(const std::string &name) {
        port = launch::recorder(scratch / "port" / name, record);
        config = launch::single(port);
        config.general.profileConfigs = true;
        profile().config = "alpha.cfg";

        launch::write(launch::profiles() / "alpha" / "alpha.cfg", "mouse_sensitivity 10\n");
        launch::write(launch::profiles() / "alpha" / "alpha-extra.cfg", "fullscreen 0\n");
    }

private:
    std::vector<fs::path> onPath;
};

}

TEST_F(LauncherTest, TheExecutableIsTheProfilesPort) {
    EXPECT_EQ(Launcher::executable(config), port);

    profile().engine = "Nothing";

    EXPECT_EQ(Launcher::executable(config), fs::path());
}

TEST_F(LauncherTest, APortRunsUnderDosboxWhenItsRowSaysSo) {
    EXPECT_FALSE(Launcher::isDosPort(config));

    config.engines.front().dosbox = true;

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

    profile().engine.clear();

    EXPECT_FALSE(launched(&error));
    EXPECT_EQ(error, "No source port is selected.");
}

TEST_F(LauncherTest, APortThatIsNotThereIsRefusedWithTheReason) {
    std::string error;

    config.engines.front().file = (scratch / "gone" / "gzdoom").string();

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

TEST_F(LauncherTest, DsdaFindsItsSaveFolderMade) {
    port = launch::recorder(scratch / "port" / "dsda-doom", record);
    config = launch::single(port);
    config.general.profileConfigs = true;
    profile().config = "alpha.cfg";

    ASSERT_TRUE(launched());

    const fs::path saves = launch::profiles() / "alpha" / "saves";
    const Strings args = arguments();

    EXPECT_TRUE(fs::is_directory(saves));
    EXPECT_FALSE(std::ranges::search(args, Strings{"-save", saves.string()}).empty());
}

TEST_F(LauncherTest, RecordingMakesTheReplaysFolder) {
    profile().config = "alpha.cfg";
    profile().replay = {.mode = ReplayMode::Record, .file = "run1"};

    ASSERT_TRUE(launched());

    EXPECT_TRUE(fs::is_directory(launch::profiles() / "alpha" / "replays"));
}

TEST_F(LauncherTest, ATypedCommandRunsInsteadWithItsPlaceholdersFilled) {
    profile().set(Switch::customCommand, true);
    profile().command = "{source_port} -x {game}";

    ASSERT_TRUE(launched());

    EXPECT_EQ(arguments(), (Strings{"-x", tests::Corpus::iwad().string()}));
}

TEST_F(LauncherTest, ATypedCommandFindsABareProgramOnThePath) {
    placeOnPath("wrapper");

    profile().set(Switch::customCommand, true);
    profile().command = "wrapper -a";

    ASSERT_TRUE(launched());

    EXPECT_EQ(arguments(), (Strings{"-a"}));
}

TEST_F(LauncherTest, ATypedCommandThatCannotBeFilledIsRefusedWithWhy) {
    std::string error;

    profile().set(Switch::customCommand, true);
    profile().command = "{nonsense}";

    EXPECT_FALSE(launched(&error));
    EXPECT_TRUE(error.starts_with("{nonsense} is not one ZDL4 knows."));
}

TEST_F(LauncherTest, ATypedCommandNeedsNoPort) {
    placeOnPath("wrapper");

    profile().engine.clear();
    profile().set(Switch::customCommand, true);
    profile().command = "wrapper";

    EXPECT_TRUE(launched());
}

TEST_F(LauncherTest, ATypedProgramNotOnThePathIsRefused) {
    std::string error;

    profile().set(Switch::customCommand, true);
    profile().command = "qzdl-no-such-program -a";

    EXPECT_FALSE(launched(&error));
    EXPECT_FALSE(error.empty());
}

TEST_F(LauncherTest, ATypedCommandMakesTheFoldersItNames) {
    config.general.profileConfigs = true;
    profile().config = "alpha.cfg";
    profile().set(Switch::customCommand, true);
    profile().command = "{source_port} {savedir} {replaydir}";

    ASSERT_TRUE(launched());

    EXPECT_TRUE(fs::is_directory(launch::profiles() / "alpha" / "saves"));
    EXPECT_TRUE(fs::is_directory(launch::profiles() / "alpha" / "replays"));
}

TEST_F(LauncherTest, ALaunchCopiesTheOldProfileConfigToThePortName) {
    oldConfigFor("dsda-doom");

    ASSERT_TRUE(launched());

    const fs::path taken = launch::profiles() / "alpha" / "dsda.cfg";

    EXPECT_EQ(launch::slurp(taken), "mouse_sensitivity 10\n");
    EXPECT_EQ(launch::slurp(launch::profiles() / "alpha" / "dsda-extra.cfg"), "fullscreen 0\n");
    EXPECT_TRUE(std::ranges::contains(arguments(), taken.string()));
}

TEST_F(LauncherTest, ATypedCommandCopiesTheOldProfileConfigToThePortName) {
    oldConfigFor("dsda-doom");

    profile().set(Switch::customCommand, true);
    profile().command = "{source_port} {profile}";

    ASSERT_TRUE(launched());

    const fs::path taken = launch::profiles() / "alpha" / "dsda.cfg";

    EXPECT_EQ(launch::slurp(taken), "mouse_sensitivity 10\n");
    EXPECT_EQ(arguments(), (Strings{taken.parent_path().string()}));
}

TEST_F(LauncherTest, AProfileVariableReachesThePort) {
    profile().environment = {{.name = "QZDL_PROBE", .value = "seen"}};

    ASSERT_TRUE(launched());

    EXPECT_EQ(probe(), "seen");
}

TEST(LauncherValue, OnePairOfQuotesAroundAllOfItIsDropped) {
    EXPECT_EQ(Launcher::value("\"BAR BAZ\""), "BAR BAZ");
    EXPECT_EQ(Launcher::value("'x'"), "x");
    EXPECT_EQ(Launcher::value("\"\""), "");
}

TEST(LauncherValue, AnythingElseIsSetAsTyped) {
    EXPECT_EQ(Launcher::value("C:\\wads"), "C:\\wads");
    EXPECT_EQ(Launcher::value("it's"), "it's");
    EXPECT_EQ(Launcher::value("\"a\" \"b\""), "\"a\" \"b\"");
    EXPECT_EQ(Launcher::value("$HOME/x"), "$HOME/x");
    EXPECT_EQ(Launcher::value("\""), "\"");
    EXPECT_EQ(Launcher::value(""), "");
}

TEST_F(LauncherTest, AQuotedVariableValueReachesThePortWithoutItsQuotes) {
    profile().environment = {{.name = "QZDL_PROBE", .value = "\"seen it\""}};

    ASSERT_TRUE(launched());

    EXPECT_EQ(probe(), "seen it");
}

TEST_F(LauncherTest, AVariableValueOfSeveralWordsReachesThePortAsTyped) {
    profile().environment = {{.name = "QZDL_PROBE", .value = "a  \"b\""}};

    ASSERT_TRUE(launched());

    EXPECT_EQ(probe(), "a  \"b\"");
}

TEST_F(LauncherTest, AProfileVariableWinsOverTheOneALaunchSets) {
    profile().environment = {{.name = "DOOMWADDIR", .value = "/mine"}};

    ASSERT_TRUE(launched());

    EXPECT_EQ(doomwaddir(), "/mine");
}

TEST_F(LauncherTest, ATypedCommandRunsWithTheProfileVariables) {
    profile().set(Switch::customCommand, true);
    profile().command = "{source_port}";
    profile().environment = {{.name = "QZDL_PROBE", .value = "typed"}};

    ASSERT_TRUE(launched());

    EXPECT_EQ(probe(), "typed");
}

TEST_F(LauncherTest, APrefixRunsInFrontOfThePort) {
    placeOnPath("wrapper");

    profile().prefix = "wrapper -w";

    ASSERT_TRUE(launched());

    Strings want{"-w", port.string()};
    const Strings built = Arguments::of(config, port.parent_path());

    want.insert(want.end(), built.begin(), built.end());

    EXPECT_EQ(arguments(), want);
}

TEST_F(LauncherTest, APrefixNotOnThePathIsRefusedByName) {
    std::string error;

    profile().prefix = "qzdl-no-such-prefix -w";

    EXPECT_FALSE(launched(&error));
    EXPECT_EQ(error, "The prefix qzdl-no-such-prefix is not on the PATH.");
}

TEST_F(LauncherTest, ARelativePrefixCountsFromHereNotFromTheGame) {
    const fs::path wrapper = launch::recorder(scratch / "tools" / "wrap", record);

    profile().prefix = fs::relative(wrapper, fs::current_path()).string() + " -w";

    ASSERT_TRUE(launched());
    ASSERT_FALSE(arguments().empty());
    EXPECT_EQ(arguments().front(), "-w");
}

TEST_F(LauncherTest, ATypedCommandRunsWithoutThePrefix) {
    placeOnPath("wrapper");

    profile().prefix = "wrapper -w";
    profile().set(Switch::customCommand, true);
    profile().command = "{source_port} -x";

    ASSERT_TRUE(launched());

    EXPECT_EQ(arguments(), (Strings{"-x"}));
}

TEST_F(LauncherTest, ADosPortRunsDosboxBehindThePrefix) {
    const fs::path dos = scratch / "dos";
    const fs::path dosbox = launch::recorder(scratch / "dosbox", scratch / "dosbox-ran");

    placeOnPath("wrapper");
    launch::write(dos / "DOOM2.EXE", "MZ");
    config.engines.front() = {.name = "Port", .file = (dos / "DOOM2.EXE").string(), .dosbox = true};
    config.general.dosbox = dosbox.string();
    profile().prefix = "wrapper";
    profile().environment = {{.name = "QZDL_PROBE", .value = "dos"}};

    ASSERT_TRUE(launched());

    Strings want{dosbox.string()};
    const Strings built = Dos::command(config).arguments;

    want.insert(want.end(), built.begin(), built.end());

    EXPECT_EQ(arguments(), want);
    EXPECT_EQ(probe(), "dos");
}

TEST_F(LauncherTest, ADosPortIsHandedToDosbox) {
    const fs::path dos = scratch / "dos";
    const fs::path dosbox = launch::recorder(scratch / "dosbox", record);

    launch::write(dos / "DOOM2.EXE", "MZ");
    config.engines.front() = {.name = "Port", .file = (dos / "DOOM2.EXE").string(), .dosbox = true};
    config.general.dosbox = dosbox.string();

    ASSERT_TRUE(launched());

    EXPECT_EQ(arguments(), Dos::command(config).arguments);
    EXPECT_EQ(directory(), fs::canonical(dos));
}
