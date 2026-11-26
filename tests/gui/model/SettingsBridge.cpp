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
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/notices/Notice.h"
#include "ttk/util/Format.h"

#include "Model.h"
#include "core/config/Config.h"
#include "core/config/ConfigPaths.h"
#include "core/config/Session.h"
#include "core/config/Versions.h"
#include "core/ports/Detect.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/SettingsBridge.h"
#include "gui/state/State.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

class SettingsBridgeTest : public model::Bridged {
protected:
    SettingsBridge &settings() const { return bridge().settings(); }

    static fs::path user() { return ConfigPaths::get().configPath(ConfigPaths::USER); }

    // A second config on disk to open.
    fs::path other(const int profiles) const {
        Config made = model::world(profiles);

        made.general.alwaysAdd = "-other";

        const fs::path file = folder() / "other.json";

        EXPECT_TRUE(made.save(file));

        return file;
    }

    void TearDown() override {
        std::error_code code;

        fs::remove_all(user(), code);

        model::Bridged::TearDown();
    }
};

TEST_F(SettingsBridgeTest, PushCopiesTheGeneralSettings) {
    Config made = model::world(1);

    made.general.alwaysAdd = "-nomusic";
    made.general.dosbox = "/opt/dosbox";
    made.general.autoClose = true;
    made.general.launchZdlImmediately = true;
    made.general.showPaths = false;
    made.general.startView = StartView::Games;
    made.general.profileConfigs = true;
    made.general.groupLibrary = true;
    made.general.groupAddons = true;
    made.general.gameEngine = "Woof";
    install(made);

    EXPECT_EQ(cfg().alwaysAdd, "-nomusic");
    EXPECT_EQ(cfg().dosbox, "/opt/dosbox");
    EXPECT_TRUE(cfg().autoClose);
    EXPECT_TRUE(cfg().launchZdlImmediately);
    EXPECT_FALSE(cfg().showPaths);
    EXPECT_EQ(cfg().startView, StartView::Games);
    EXPECT_TRUE(cfg().profileConfigs);
    EXPECT_TRUE(cfg().groupLibrary);
    EXPECT_TRUE(cfg().groupAddons);
    EXPECT_EQ(cfg().gameEngine, "Woof");
}

TEST_F(SettingsBridgeTest, TheDosboxFoundOnThisMachineIsPushed) {
    install(model::world(1));

    EXPECT_EQ(cfg().systemDosbox, ttk::Format::from_path(Detect::dosbox()));
}

TEST_F(SettingsBridgeTest, SettingTheGameEngineWritesAndPushesIt) {
    install(model::world(1));

    settings().setGameEngine("Chocolate");

    EXPECT_EQ(config().general.gameEngine, "Chocolate");
    EXPECT_EQ(cfg().gameEngine, "Chocolate");
}

TEST_F(SettingsBridgeTest, SettingTheGameEngineIsSaved) {
    install(model::world(1));

    settings().setGameEngine("Chocolate");
    settle();

    EXPECT_EQ(saved().general.gameEngine, "Chocolate");
}

TEST_F(SettingsBridgeTest, SettingTheGameEngineMovesTheGameRevision) {
    install(model::world(1));

    const int before = cfg().gameRev;

    settings().setGameEngine("Chocolate");

    EXPECT_GT(cfg().gameRev, before);
}

TEST_F(SettingsBridgeTest, ArgumentsAlwaysAddedReachTheCommandLine) {
    install(model::world(1));

    settings().setAlwaysAdd("-nosound -nomusic");
    preview();

    EXPECT_EQ(cfg().alwaysAdd, "-nosound -nomusic");
    EXPECT_TRUE(cfg().commandLine.contains("-nosound -nomusic")) << cfg().commandLine;
}

TEST_F(SettingsBridgeTest, ArgumentsAlwaysAddedAreSaved) {
    install(model::world(1));

    settings().setAlwaysAdd("-nosound");
    settle();

    EXPECT_EQ(saved().general.alwaysAdd, "-nosound");
}

TEST_F(SettingsBridgeTest, ADosPortLaunchesThroughTheDosboxSet) {
    Config made = model::world(1);

    made.profiles.front().engine = "Vanilla";
    install(made);

    const fs::path dosbox = folder() / "dosbox";

    model::write(dosbox);

    settings().setDosbox(dosbox.string());
    preview();

    EXPECT_EQ(cfg().dosbox, dosbox.string());
#ifdef _WIN32
    // A backslash is enough to have the line quote a path on Windows.
    EXPECT_TRUE(cfg().commandLine.starts_with("\"" + dosbox.string() + "\"")) << cfg().commandLine;
#else
    EXPECT_TRUE(cfg().commandLine.starts_with(dosbox.string())) << cfg().commandLine;
#endif
    EXPECT_GT(cfg().dosCommands, 0);
}

TEST_F(SettingsBridgeTest, TheDosboxSetIsSaved) {
    install(model::world(1));

    settings().setDosbox("/opt/dosbox/dosbox");
    settle();

    EXPECT_EQ(saved().general.dosbox, "/opt/dosbox/dosbox");
}

TEST_F(SettingsBridgeTest, AutoCloseIsWrittenPushedAndSaved) {
    install(model::world(1));

    settings().setAutoClose(true);

    EXPECT_TRUE(config().general.autoClose);
    EXPECT_TRUE(cfg().autoClose);

    settle();

    EXPECT_TRUE(saved().general.autoClose);
}

TEST_F(SettingsBridgeTest, LaunchingZdlFilesAtOnceIsWrittenPushedAndSaved) {
    install(model::world(1));

    settings().setLaunchZdlImmediately(true);

    EXPECT_TRUE(config().general.launchZdlImmediately);
    EXPECT_TRUE(cfg().launchZdlImmediately);

    settle();

    EXPECT_TRUE(saved().general.launchZdlImmediately);
}

TEST_F(SettingsBridgeTest, ShowingPathsIsWrittenPushedAndSaved) {
    install(model::world(1));

    settings().setShowPaths(false);

    EXPECT_FALSE(config().general.showPaths);
    EXPECT_FALSE(cfg().showPaths);

    settle();

    EXPECT_FALSE(saved().general.showPaths);
}

TEST_F(SettingsBridgeTest, GroupingIsWrittenPushedAndSavedForEachList) {
    install(model::world(1));

    settings().setGroupLibrary(true);

    EXPECT_TRUE(config().general.groupLibrary);
    EXPECT_TRUE(cfg().groupLibrary);
    EXPECT_FALSE(cfg().groupAddons);

    settings().setGroupAddons(true);
    settle();

    EXPECT_TRUE(saved().general.groupLibrary);
    EXPECT_TRUE(saved().general.groupAddons);
}

TEST_F(SettingsBridgeTest, TheStartViewIsWrittenPushedAndSaved) {
    install(model::world(1));

    settings().setStartView(StartView::Games);

    EXPECT_EQ(config().general.startView, StartView::Games);
    EXPECT_EQ(cfg().startView, StartView::Games);

    settle();

    EXPECT_EQ(saved().general.startView, StartView::Games);
}

TEST_F(SettingsBridgeTest, PerProfileConfigsHandTheProfileAConfig) {
    install(model::world(1));
    preview();

    EXPECT_FALSE(cfg().commandLine.contains("-config")) << cfg().commandLine;

    settings().setProfileConfigs(true);
    preview();

    EXPECT_TRUE(cfg().profileConfigs);
    EXPECT_TRUE(cfg().commandLine.contains("-config")) << cfg().commandLine;
    EXPECT_TRUE(cfg().commandLine.contains("alpha.cfg")) << cfg().commandLine;
}

TEST_F(SettingsBridgeTest, PerProfileConfigsAreSaved) {
    install(model::world(1));

    settings().setProfileConfigs(true);
    settle();

    EXPECT_TRUE(saved().general.profileConfigs);
}

TEST_F(SettingsBridgeTest, PortableModeIsOffByDefault) {
    install(model::world(1));

    EXPECT_FALSE(cfg().portableWanted);
    EXPECT_FALSE(cfg().portableRunning);
}

TEST_F(SettingsBridgeTest, TurningPortableModeOnIsWrittenToTheUserConfigForTheNextStart) {
    const tests::Sandbox::Portable program;

    install(model::world(1));

    settings().setPortableWanted(true);

    EXPECT_TRUE(cfg().portableWanted);
    EXPECT_FALSE(cfg().portableRunning);
    EXPECT_TRUE(notifier.messages().empty());

    Config written;

    ASSERT_TRUE(written.load(user()));
    EXPECT_TRUE(written.general.noUserConf);
    EXPECT_TRUE(written.general.moveAtNextStart);
    EXPECT_FALSE(config().general.noUserConf);
    EXPECT_FALSE(fs::exists(program.directory() / "zdl.json"));
}

TEST_F(SettingsBridgeTest, TurningPortableModeBackOffLeavesNoMove) {
    const tests::Sandbox::Portable program;

    install(model::world(1));

    settings().setPortableWanted(true);
    settings().setPortableWanted(false);

    EXPECT_FALSE(cfg().portableWanted);

    Config written;

    ASSERT_TRUE(written.load(user()));
    EXPECT_FALSE(written.general.moveAtNextStart);
}

TEST_F(SettingsBridgeTest, TurningOffPortableModeThatIsOffWritesNothing) {
    install(model::world(1));

    settings().setPortableWanted(false);

    EXPECT_FALSE(fs::exists(user()));
    EXPECT_TRUE(notifier.messages().empty());
}

TEST_F(SettingsBridgeTest, AUserConfigThatCannotBeWrittenIsReported) {
    const tests::Sandbox::Portable program;

    install(model::world(1));

    std::error_code code;

    fs::create_directories(user(), code);

    settings().setPortableWanted(true);

    const std::vector<ttk::Notice> errors = said(ttk::Severity::Error);

    ASSERT_EQ(errors.size(), 1U);
    EXPECT_TRUE(errors.front().body.starts_with("Could not write the user config: ")) << errors.front().body;
    EXPECT_FALSE(cfg().portableWanted);
}

TEST_F(SettingsBridgeTest, AProgramFolderThatCannotBeWrittenIsReported) {
    const tests::Sandbox::Portable program;

    install(model::world(1));

    fs::permissions(program.directory(), fs::perms::owner_read | fs::perms::owner_exec, fs::perm_options::replace);

    if (std::ofstream(program.directory() / "probe")) {
        fs::permissions(program.directory(), fs::perms::owner_all, fs::perm_options::replace);

        GTEST_SKIP() << "the read-only folder still takes writes, as it does for root";
    }

    settings().setPortableWanted(true);

    fs::permissions(program.directory(), fs::perms::owner_all, fs::perm_options::replace);

    const std::vector<ttk::Notice> errors = said(ttk::Severity::Error);

    ASSERT_EQ(errors.size(), 1U);
    EXPECT_EQ(errors.front().body,
              "ZDL4 cannot write to " + program.directory().string() + ", so portable mode stays off");
    EXPECT_FALSE(cfg().portableWanted);
    EXPECT_FALSE(fs::exists(user()));
}

TEST_F(SettingsBridgeTest, ClearingEverythingKeepsPortableMode) {
    const tests::Sandbox::Portable program;

    install(model::world(2));

    settings().adoptAsUserConfig();
    settings().setPortableWanted(true);
    settings().clearEverything();
    settle();

    EXPECT_TRUE(config().profiles.empty());
    EXPECT_TRUE(saved().general.noUserConf);
    EXPECT_TRUE(saved().general.moveAtNextStart);
    EXPECT_TRUE(cfg().portableWanted);
}

TEST_F(SettingsBridgeTest, ThePathRowsShowTheOpenConfigAndTheDataDirectory) {
    install(model::world(1));

    EXPECT_EQ(cfg().path, ttk::Format::from_path(folder() / "zdl.json"));
    EXPECT_EQ(cfg().dataDirectory, ttk::Format::from_path(ConfigPaths::dataDirectory()));
    EXPECT_TRUE(cfg().openedByHand);
}

TEST_F(SettingsBridgeTest, TheDataDirectoryFollowsPortableMode) {
    const tests::Sandbox::Portable program;

    install(model::world(1));

    EXPECT_EQ(cfg().dataDirectory, ttk::Format::from_path(program.directory()));
}

TEST_F(SettingsBridgeTest, SavingAsMovesToTheNewFile) {
    install(model::world(1));

    const fs::path target = folder() / "copy.json";

    settings().saveAs(target.string());

    EXPECT_EQ(Session::get().path(), target);
    EXPECT_EQ(cfg().path, ttk::Format::from_path(target));
    EXPECT_TRUE(fs::exists(target));

    const std::vector<ttk::Notice> done = said(ttk::Severity::Success);

    ASSERT_EQ(done.size(), 1U);
    EXPECT_EQ(done.front().body, "Saved to " + target.string() + ".");
}

TEST_F(SettingsBridgeTest, SavingAsSomewhereUnwritableIsReportedAndStays) {
    install(model::world(1));

    const fs::path before = Session::get().path();

    settings().saveAs(folder().string());

    EXPECT_EQ(Session::get().path(), before);

    const std::vector<ttk::Notice> errors = said(ttk::Severity::Error);

    ASSERT_EQ(errors.size(), 1U);
    EXPECT_TRUE(errors.front().body.starts_with("Could not save to " + folder().string() + ": "))
            << errors.front().body;
}

TEST_F(SettingsBridgeTest, LoadingAConfigPushesIt) {
    install(model::world(1));

    const fs::path file = other(4);

    settings().load(file.string());

    EXPECT_EQ(Session::get().path(), file);
    EXPECT_EQ(cfg().path, ttk::Format::from_path(file));
    EXPECT_EQ(cfg().profileCards.size(), 4U);
    EXPECT_EQ(cfg().alwaysAdd, "-other");
    EXPECT_EQ(said(ttk::Severity::Success).size(), 1U);
}

TEST_F(SettingsBridgeTest, LoadingAConfigSavesWhatWasPendingFirst) {
    install(model::world(1));

    const fs::path before = Session::get().path();
    const fs::path file = other(2);

    settings().setAlwaysAdd("-kept");
    settings().load(file.string());

    Config left;

    ASSERT_TRUE(left.load(before));
    EXPECT_EQ(left.general.alwaysAdd, "-kept");
}

TEST_F(SettingsBridgeTest, LoadingAConfigSaysItWasReplacedAndToDetect) {
    install(model::world(1));

    std::vector<bool> asked;

    bridge().replaced = [&asked](const bool detect) { asked.push_back(detect); };

    settings().load(other(1).string());

    EXPECT_EQ(asked, (std::vector<bool>{true}));
}

TEST_F(SettingsBridgeTest, LoadingAnOlderConfigSaysSoAndANewerOneWarns) {
    install(model::world(1));

    const fs::path older = folder() / "older.json";
    const fs::path newer = folder() / "newer.json";

    model::write(older, R"({"version": 1, "engine": "ZDL4", "appVersion": "4.5.0", "profiles": []})");
    model::write(newer,
                 R"({"version": )" + std::to_string(Versions::CURRENT + 1)
                         + R"(, "engine": "ZDL4", "appVersion": "5.0.0", "profiles": []})");

    settings().load(older.string());

    const std::vector<ttk::Notice> infos = said(ttk::Severity::Info);

    ASSERT_EQ(infos.size(), 1U);
    EXPECT_EQ(infos.front().body, Session::get().formatNote().text);
    EXPECT_NE(infos.front().body.find("older.json.v8"), std::string::npos) << infos.front().body;

    settings().load(newer.string());

    const std::vector<ttk::Notice> warnings = said(ttk::Severity::Warning);

    ASSERT_EQ(warnings.size(), 1U);
    EXPECT_EQ(warnings.front().body, Session::get().formatNote().text);
    EXPECT_NE(warnings.front().body.find("5.0.0"), std::string::npos) << warnings.front().body;
    EXPECT_EQ(said(ttk::Severity::Info).size(), 1U);
}

TEST_F(SettingsBridgeTest, AConfigThatCannotBeReadIsReportedAndKept) {
    install(model::world(3));

    std::vector<bool> asked;

    bridge().replaced = [&asked](const bool detect) { asked.push_back(detect); };

    const fs::path missing = folder() / "missing.json";

    settings().load(missing.string());

    EXPECT_EQ(config().profiles.size(), 3U);
    EXPECT_TRUE(asked.empty());

    const std::vector<ttk::Notice> errors = said(ttk::Severity::Error);

    ASSERT_EQ(errors.size(), 1U);
    EXPECT_TRUE(errors.front().body.starts_with("Could not read " + missing.string() + ": ")) << errors.front().body;
}

TEST_F(SettingsBridgeTest, AdoptingMakesThisTheUserConfig) {
    install(model::world(2));

    settings().adoptAsUserConfig();

    EXPECT_FALSE(cfg().openedByHand);
    EXPECT_EQ(Session::get().path(), user());
    EXPECT_TRUE(fs::exists(user()));

    const std::vector<ttk::Notice> done = said(ttk::Severity::Success);

    ASSERT_EQ(done.size(), 1U);
    EXPECT_EQ(done.front().body, "This config is now the one ZDL4 opens by default.");
}

TEST_F(SettingsBridgeTest, AdoptingRecordsWhereItCameFrom) {
    install(model::world(2));

    const fs::path before = Session::get().path();

    settings().adoptAsUserConfig();

    Config written;

    ASSERT_TRUE(written.load(user()));
    EXPECT_TRUE(written.general.isImported);
    EXPECT_EQ(written.general.importedFrom, before.string());
}

TEST_F(SettingsBridgeTest, AdoptingWhereNothingCanBeWrittenIsReported) {
    install(model::world(2));

    std::error_code code;

    fs::create_directories(user(), code);

    settings().adoptAsUserConfig();

    EXPECT_TRUE(cfg().openedByHand);

    const std::vector<ttk::Notice> errors = said(ttk::Severity::Error);

    ASSERT_EQ(errors.size(), 1U);
    EXPECT_TRUE(errors.front().body.starts_with("Could not write the user config: ")) << errors.front().body;
}

TEST_F(SettingsBridgeTest, ClearingEverythingEmptiesTheConfig) {
    install(model::world(3));

    settings().clearEverything();

    EXPECT_TRUE(config().profiles.empty());
    EXPECT_TRUE(config().iwads.empty());
    EXPECT_TRUE(config().engines.empty());
    EXPECT_TRUE(cfg().profileCards.empty());
    EXPECT_TRUE(cfg().engineNames.empty());
    EXPECT_EQ(cfg().profileIndex, -1);
}

TEST_F(SettingsBridgeTest, ClearingEverythingIsSaved) {
    install(model::world(3));

    settings().clearEverything();
    settle();

    EXPECT_TRUE(saved().profiles.empty());
}

TEST_F(SettingsBridgeTest, ClearingEverythingSaysNotToDetect) {
    install(model::world(1));

    std::vector<bool> asked;

    bridge().replaced = [&asked](const bool detect) { asked.push_back(detect); };

    settings().clearEverything();

    EXPECT_EQ(asked, (std::vector<bool>{false}));
}

}
