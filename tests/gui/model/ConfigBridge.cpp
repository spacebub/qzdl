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
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/notices/Notice.h"

#include "Model.h"
#include "core/config/Session.h"
#include "gui/model/ConfigBridge.h"
#include "gui/state/State.h"

namespace {

namespace fs = std::filesystem;

using model::AUTOSAVE;

class ConfigBridgeTest : public model::Bridged {
protected:
    // The port a launch runs: a script that prints `said` and leaves.
    fs::path script(const std::string &said = {}) const {
        const fs::path file = folder() / "bin" / "gzdoom";

        model::write(file, "#!/bin/sh\n" + (said.empty() ? std::string() : "echo " + said + "\n"));

        fs::permissions(file, fs::perms::owner_all, fs::perm_options::replace);

        return file;
    }

    // The saved file made a folder, so the next save cannot write it.
    static void blockSaves() {
        std::error_code code;

        fs::remove(Session::get().path(), code);
        fs::create_directories(Session::get().path(), code);
    }

    static void unblockSaves() {
        std::error_code code;

        fs::remove_all(Session::get().path(), code);
    }
};

TEST_F(ConfigBridgeTest, ReloadPushesTheConfigIntoTheTree) {
    install(model::world(3));

    EXPECT_EQ(cfg().profileCards.size(), 3U);
    EXPECT_EQ(cfg().shelfProfiles.size(), 3U);
    EXPECT_EQ(cfg().profileName, "Alpha");
    EXPECT_EQ(cfg().iwadNames, (std::vector<std::string>{"Doom II", "Plutonia"}));
    EXPECT_EQ(cfg().portNames.size(), 7U);
    EXPECT_EQ(cfg().path, Session::get().path().string());
}

TEST_F(ConfigBridgeTest, ReloadMovesTheRevision) {
    const int before = cfg().rev;

    bridge().reload();

    EXPECT_GT(cfg().rev, before);
}

TEST_F(ConfigBridgeTest, ReloadTouchesTheTree) {
    touches = 0;

    bridge().reload();

    EXPECT_GT(touches, 0);
}

TEST_F(ConfigBridgeTest, BumpRevCountsUpAndTouches) {
    const int before = cfg().rev;

    touches = 0;
    bridge().bumpRev();

    EXPECT_EQ(cfg().rev, before + 1);
    EXPECT_EQ(touches, 1);
}

TEST_F(ConfigBridgeTest, AScheduledSaveWaitsOutTheDebounce) {
    config().general.alwaysAdd = "-nosound";
    bridge().scheduleSave();

    shell.advance(AUTOSAVE - 0.05);

    EXPECT_EQ(saved().general.alwaysAdd, "");

    shell.advance(0.05);

    EXPECT_EQ(saved().general.alwaysAdd, "-nosound");
}

TEST_F(ConfigBridgeTest, AnotherChangeRestartsTheDebounce) {
    config().general.alwaysAdd = "-nosound";
    bridge().scheduleSave();
    shell.advance(0.3);

    bridge().scheduleSave();
    shell.advance(0.3);

    EXPECT_EQ(saved().general.alwaysAdd, "");

    shell.advance(0.1);

    EXPECT_EQ(saved().general.alwaysAdd, "-nosound");
}

TEST_F(ConfigBridgeTest, ADebouncedSaveHoldsOneTimer) {
    preview();
    bridge().scheduleSave();
    bridge().scheduleSave();
    bridge().scheduleSave();

    EXPECT_EQ(shell.pending(), 1U);
}

TEST_F(ConfigBridgeTest, FlushWritesAtOnceAndDropsTheTimer) {
    preview();

    config().general.alwaysAdd = "-nosound";
    bridge().scheduleSave();
    bridge().flush();

    EXPECT_EQ(saved().general.alwaysAdd, "-nosound");
    EXPECT_EQ(shell.pending(), 0U);
}

TEST_F(ConfigBridgeTest, FlushWithNothingScheduledWritesNothing) {
    config().general.alwaysAdd = "-nosound";
    bridge().flush();

    EXPECT_EQ(saved().general.alwaysAdd, "");
}

TEST_F(ConfigBridgeTest, AFailedSaveIsReportedAsAnError) {
    blockSaves();
    bridge().scheduleSave();
    settle();

    const std::vector<ttk::Notice> errors = said(ttk::Severity::Error);

    ASSERT_EQ(errors.size(), 1U);
    EXPECT_TRUE(errors.front().body.starts_with("Could not save the config: ")) << errors.front().body;

    unblockSaves();
}

TEST_F(ConfigBridgeTest, ASaveThatKeepsFailingIsReportedOnce) {
    blockSaves();

    for (int at = 0; at < 3; ++at) {
        bridge().scheduleSave();
        settle();
    }

    EXPECT_EQ(said(ttk::Severity::Error).size(), 1U);

    unblockSaves();
}

TEST_F(ConfigBridgeTest, ASaveFailingAgainAfterOneWorkedIsReportedAgain) {
    blockSaves();
    bridge().scheduleSave();
    settle();

    unblockSaves();
    bridge().scheduleSave();
    settle();

    blockSaves();
    bridge().scheduleSave();
    settle();

    EXPECT_EQ(said(ttk::Severity::Error).size(), 2U);

    unblockSaves();
}

TEST_F(ConfigBridgeTest, ThePreviewIsDueOnTheNextTurn) {
    install(model::world(1));

    config().activeProfile().skill = 4;
    bridge().schedulePreview();

    EXPECT_DOUBLE_EQ(shell.nextDue(), 0.0);

    preview();

    EXPECT_TRUE(cfg().commandLine.contains("-skill 4")) << cfg().commandLine;
}

TEST_F(ConfigBridgeTest, PreviewsAskedTogetherRunOnce) {
    install(model::world(1));
    preview();

    const std::size_t before = shell.pending();

    bridge().schedulePreview();
    bridge().schedulePreview();
    bridge().schedulePreview();

    EXPECT_EQ(shell.pending(), before + 1);
}

TEST_F(ConfigBridgeTest, APreviewCanBeAskedForAgainOnceItRan) {
    install(model::world(1));
    preview();

    bridge().schedulePreview();
    preview();

    config().activeProfile().skill = 2;
    bridge().schedulePreview();
    preview();

    EXPECT_TRUE(cfg().commandLine.contains("-skill 2")) << cfg().commandLine;
}

TEST_F(ConfigBridgeTest, KeysNameWhatTheyBelongTo) {
    EXPECT_EQ(ConfigBridge::profileKey("abc"), "profile:abc");
    EXPECT_EQ(ConfigBridge::gameKey("Doom II"), "game:Doom II");
    EXPECT_NE(ConfigBridge::profileKey("x"), ConfigBridge::gameKey("x"));
}

TEST_F(ConfigBridgeTest, NothingIsBusyWithoutARun) {
    install(model::world(1));

    EXPECT_FALSE(bridge().busy("id-alpha"));
}

TEST_F(ConfigBridgeTest, AProfileWithARunUpIsBusy) {
    install(model::world(2));
    hold("id-alpha");

    EXPECT_TRUE(bridge().busy("id-alpha"));
    EXPECT_FALSE(bridge().busy("id-bravo"));
}

TEST_F(ConfigBridgeTest, AnEmptyIdIsNeverBusy) {
    hold("");

    EXPECT_FALSE(bridge().busy(""));
}

TEST_F(ConfigBridgeTest, StartWithoutAPortIsRefusedAndSaysSo) {
    Config what = model::world(1);

    what.profiles.front().port.clear();
    install(what);

    int launched = 0;

    bridge().launched = [&launched] { ++launched; };

    EXPECT_FALSE(bridge().start("profile:id-alpha", "Alpha", config()));

    const std::vector<ttk::Notice> errors = said(ttk::Severity::Error);

    ASSERT_EQ(errors.size(), 1U);
    EXPECT_EQ(errors.front().title, "Nothing was launched");
    EXPECT_EQ(errors.front().body, "No source port is selected.");
    EXPECT_EQ(launched, 0);
}

TEST_F(ConfigBridgeTest, ARefusedStartMarksTheRunFailed) {
    Config what = model::world(1);

    what.profiles.front().port.clear();
    install(what);

    std::ignore = bridge().start("profile:id-alpha", "Alpha", config());

    EXPECT_EQ(runs().stateOf("profile:id-alpha"), State::RunState::Failed);
    EXPECT_EQ(runs().reasonOf("profile:id-alpha"), "No source port is selected.");
}

TEST_F(ConfigBridgeTest, StartRunsThePortAndSaysItLaunched) {
    Config what = model::world(1);

    what.ports.front().file = script().string();
    install(what);

    int launched = 0;

    bridge().launched = [&launched] { ++launched; };

    EXPECT_TRUE(bridge().start("profile:id-alpha", "Alpha", config()));
    EXPECT_EQ(launched, 1);
    EXPECT_EQ(runs().stateOf("profile:id-alpha"), State::RunState::Launching);
    EXPECT_TRUE(bridge().busy("id-alpha"));
    EXPECT_TRUE(said(ttk::Severity::Error).empty());

    waitOut("profile:id-alpha");
}

TEST_F(ConfigBridgeTest, ClosingOnLaunchLeavesTheOutputUncaptured) {
    Config what = model::world(1);

    what.ports.front().file = script("started").string();
    what.profiles.front().captureOutput = true;
    what.general.autoClose = true;
    install(what);

    ASSERT_TRUE(bridge().start("profile:id-alpha", "Alpha", config()));

    runs().show("profile:id-alpha");

    EXPECT_TRUE(runs().text().contains("not recording the game's output")) << runs().text();

    waitOut("profile:id-alpha");
}

TEST_F(ConfigBridgeTest, ALaunchedPortThatEndsIsClosed) {
    Config what = model::world(1);

    what.ports.front().file = script().string();
    install(what);

    ASSERT_TRUE(bridge().start("profile:id-alpha", "Alpha", config()));

    waitOut("profile:id-alpha");

    EXPECT_EQ(runs().stateOf("profile:id-alpha"), State::RunState::Closed);
    EXPECT_FALSE(bridge().busy("id-alpha"));
}

TEST_F(ConfigBridgeTest, AStartWithItsOutputCapturedHearsThePort) {
    Config what = model::world(1);

    what.ports.front().file = script("started").string();
    what.profiles.front().captureOutput = true;
    install(what);

    ASSERT_TRUE(bridge().start("profile:id-alpha", "Alpha", config()));

    bool heard = false;

    runs().show("profile:id-alpha");

    // The reader thread fills a buffer the clock harvests, so time is given to both.
    for (int at = 0; at < 100 && !heard; ++at) {
        shell.await(0.05);
        shell.advance(0.15);

        heard = runs().text().contains("started");
    }

    EXPECT_TRUE(heard) << runs().text();

    waitOut("profile:id-alpha");
}

}
