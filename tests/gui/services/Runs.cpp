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
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Process.h"

#include "Spawn.h"
#include "gui/services/Runs.h"
#include "gui/state/State.h"
#include "support/Sandbox.h"
#include "support/Shell.h"

namespace {

namespace fs = std::filesystem;
namespace Spawn = tests::Spawn;

using Keys = std::vector<std::string>;

// How often runs are swept.
constexpr double TICK = 0.5;

constexpr const char *KEY = "profile:one";

bool holds(const Keys &keys, const std::string &key) {
    return std::ranges::find(keys, key) != keys.end();
}

class RunsTest : public ::testing::Test {
protected:
    void SetUp() override {
        State::get() = State::All{};

        runs = std::make_unique<Runs>(&shell);
    }

    void TearDown() override {
        runs.reset();
        Spawn::end(started);
    }

    Spawn::Child waiting(const bool capture, const std::string &before = "") {
        return launch(before + "read x < \"" + Spawn::fifo(directory).string() + "\"", capture);
    }

    Spawn::Child launch(const std::string &body, const bool capture) {
        const Spawn::Child child =
                Spawn::start(Spawn::script(directory, "port" + std::to_string(started.size()), body), capture);

        EXPECT_NE(child.id, 0U);

        started.push_back(child.id);

        return child;
    }

    void began(const Spawn::Child &child, const std::string &key = KEY) {
        runs->began(key, "Doom", "doom -iwad doom.wad", child.id, child.output);
    }

    bool settles(const std::string &key = KEY) {
        return Spawn::until(shell, TICK, [this, key] { return !runs->alive(key); });
    }

    std::vector<std::string> shown() const {
        std::vector<std::string> out;

        for (const State::LogRow &row : State::get().runs.lines) {
            out.push_back(row.line);
        }

        return out;
    }

    fs::path directory = tests::Sandbox::scratch("runs");

    tests::Shell shell;
    std::unique_ptr<Runs> runs;
    std::vector<ttk::Process::Id> started;
};

TEST_F(RunsTest, AKeyNeverRunIsNone) {
    EXPECT_EQ(runs->stateOf(KEY), State::RunState::None);
    EXPECT_EQ(runs->reasonOf(KEY), "");
    EXPECT_EQ(runs->titleOf(KEY), "");
    EXPECT_FALSE(runs->alive(KEY));
}

TEST_F(RunsTest, TheDockStartsEmpty) {
    const State::RunsState &state = State::get().runs;

    EXPECT_GT(state.rev, 0);
    EXPECT_FALSE(state.busy);
    EXPECT_TRUE(state.docked.empty());
    EXPECT_EQ(state.showing, "");
    EXPECT_TRUE(state.logged.empty());
    EXPECT_EQ(runs->text(), "");
}

TEST_F(RunsTest, ARefusalFailsWithItsReason) {
    runs->refused(KEY, "Doom", "There is no game file");

    EXPECT_EQ(runs->stateOf(KEY), State::RunState::Failed);
    EXPECT_EQ(runs->reasonOf(KEY), "There is no game file");
    EXPECT_EQ(runs->titleOf(KEY), "Doom");
    EXPECT_FALSE(runs->alive(KEY));
    EXPECT_FALSE(State::get().runs.busy);
}

TEST_F(RunsTest, ARefusalIsLoggedButNotDocked) {
    runs->refused(KEY, "Doom", "There is no game file");

    EXPECT_TRUE(holds(State::get().runs.logged, KEY));
    EXPECT_TRUE(State::get().runs.docked.empty());
}

TEST_F(RunsTest, ARefusalStartsTheSweep) {
    runs->refused(KEY, "Doom", "There is no game file");

    EXPECT_EQ(shell.pending(), 1U);
    EXPECT_DOUBLE_EQ(shell.nextDue(), TICK);
}

TEST_F(RunsTest, TheShownLogSaysWhyItWouldNotStart) {
    runs->refused(KEY, "Doom", "There is no game file");
    runs->show(KEY);

    EXPECT_EQ(runs->text(), "It would not start: There is no game file");
}

TEST_F(RunsTest, ShowDocksAndShowsAKey) {
    runs->show(KEY);

    EXPECT_EQ(State::get().runs.docked, (Keys{KEY}));
    EXPECT_EQ(State::get().runs.showing, KEY);
}

TEST_F(RunsTest, ShowingAnotherKeySwitchesTheDock) {
    runs->show("a");
    runs->show("b");

    EXPECT_EQ(State::get().runs.docked, (Keys{"a", "b"}));
    EXPECT_EQ(State::get().runs.showing, "b");
}

TEST_F(RunsTest, HideFoldsTheDockAwayAndKeepsItsTabs) {
    runs->show(KEY);
    runs->hide();

    EXPECT_EQ(State::get().runs.docked, (Keys{KEY}));
    EXPECT_EQ(State::get().runs.showing, "");
}

TEST_F(RunsTest, ToggleShowsThenHides) {
    runs->toggle(KEY);

    EXPECT_EQ(State::get().runs.showing, KEY);

    runs->toggle(KEY);

    EXPECT_EQ(State::get().runs.showing, "");
}

TEST_F(RunsTest, ToggleOfAnotherKeyShowsIt) {
    runs->show("a");
    runs->toggle("b");

    EXPECT_EQ(State::get().runs.showing, "b");
}

TEST_F(RunsTest, ClosingAKeyWithNoRunTakesItsTab) {
    runs->show("a");
    runs->show("b");
    runs->close("b");

    EXPECT_EQ(State::get().runs.docked, (Keys{"a"}));
    EXPECT_EQ(State::get().runs.showing, "");
}

TEST_F(RunsTest, ClosingAHiddenTabKeepsWhatIsShown) {
    runs->show("a");
    runs->show("b");
    runs->close("a");

    EXPECT_EQ(State::get().runs.docked, (Keys{"b"}));
    EXPECT_EQ(State::get().runs.showing, "b");
}

TEST_F(RunsTest, TheShownLogIsMirroredInTheState) {
    runs->refused(KEY, "Doom", "There is no game file");
    runs->show(KEY);

    const State::RunsState &state = State::get().runs;

    ASSERT_EQ(state.lines.size(), 1U);
    EXPECT_EQ(state.lines.front().line, "It would not start: There is no game file");
    EXPECT_TRUE(state.lines.front().own);
    EXPECT_FALSE(state.live);
}

TEST_F(RunsTest, HidingEmptiesTheMirroredLines) {
    runs->refused(KEY, "Doom", "There is no game file");
    runs->show(KEY);

    const int generation = State::get().runs.lineGeneration;

    runs->hide();

    EXPECT_TRUE(State::get().runs.lines.empty());
    EXPECT_NE(State::get().runs.lineGeneration, generation);
    EXPECT_EQ(runs->text(), "");
}

TEST_F(RunsTest, AShownKeyWithNoLogHasNoText) {
    runs->show("nothing ran");

    EXPECT_EQ(runs->text(), "");
    EXPECT_TRUE(State::get().runs.lines.empty());
}

TEST_F(RunsTest, OnlyTheEightNewestLogsAreKept) {
    for (int at = 0; at < 10; ++at) {
        runs->refused("k" + std::to_string(at), "T" + std::to_string(at), "why");
    }

    const Keys &logged = State::get().runs.logged;

    EXPECT_EQ(logged.size(), 8U);
    EXPECT_FALSE(holds(logged, "k0"));
    EXPECT_FALSE(holds(logged, "k1"));
    EXPECT_TRUE(holds(logged, "k2"));
    EXPECT_TRUE(holds(logged, "k9"));
}

TEST_F(RunsTest, AForgottenLogKeepsItsTitleWhileItsRunStays) {
    for (int at = 0; at < 10; ++at) {
        runs->refused("k" + std::to_string(at), "T" + std::to_string(at), "why");
    }

    EXPECT_EQ(runs->titleOf("k0"), "T0");
    EXPECT_EQ(runs->stateOf("k0"), State::RunState::Failed);
}

TEST_F(RunsTest, AShownLogIsKeptPastTheLimit) {
    runs->refused("k0", "T0", "why");
    runs->show("k0");

    for (int at = 1; at < 10; ++at) {
        runs->refused("k" + std::to_string(at), "T" + std::to_string(at), "why");
    }

    const Keys &logged = State::get().runs.logged;

    EXPECT_EQ(logged.size(), 8U);
    EXPECT_TRUE(holds(logged, "k0"));
    EXPECT_FALSE(holds(logged, "k1"));
    EXPECT_FALSE(holds(logged, "k2"));
}

TEST_F(RunsTest, ARefusalWithEveryLogDockedKeepsItsOwnLog) {
    for (int at = 0; at < 8; ++at) {
        runs->refused("k" + std::to_string(at), "T", "why");
        runs->show("k" + std::to_string(at));
    }

    runs->refused("new", "New", "why");
    runs->show("new");

    EXPECT_EQ(runs->text(), "It would not start: why");
}

TEST_F(RunsTest, ALaunchIsLaunchingAndAlive) {
    began(waiting(false));

    EXPECT_EQ(runs->stateOf(KEY), State::RunState::Launching);
    EXPECT_EQ(runs->titleOf(KEY), "Doom");
    EXPECT_TRUE(runs->alive(KEY));
    EXPECT_TRUE(State::get().runs.busy);
}

TEST_F(RunsTest, ALaunchNotesItselfAndItsCommandLine) {
    began(waiting(false));
    runs->show(KEY);

    const std::vector<std::string> lines = shown();

    ASSERT_GE(lines.size(), 2U);
    EXPECT_EQ(lines[0], "Launching Doom.");
    EXPECT_EQ(lines[1], "$ doom -iwad doom.wad");
}

TEST_F(RunsTest, ALaunchWithoutOutputIsNotDockedAndSaysSo) {
    began(waiting(false));

    EXPECT_TRUE(State::get().runs.docked.empty());

    runs->show(KEY);

    EXPECT_EQ(shown().back(),
              "This profile is not recording the game's output, so only ZDL4's own side of it is here.");
}

TEST_F(RunsTest, ALaunchWithOutputIsDockedButNotShown) {
    began(waiting(true));

    EXPECT_EQ(State::get().runs.docked, (Keys{KEY}));
    EXPECT_EQ(State::get().runs.showing, "");
}

TEST_F(RunsTest, TheShownLogFollowsTheGame) {
    began(launch("echo hello from the game; read x < \"" + Spawn::fifo(directory).string() + "\"", true));
    runs->show(KEY);

    EXPECT_TRUE(State::get().runs.live);
    EXPECT_TRUE(
            Spawn::until(shell, 0.06, [this] { return !shown().empty() && shown().back() == "hello from the game"; }));
    EXPECT_FALSE(State::get().runs.lines.back().own);
}

TEST_F(RunsTest, ARelaunchStartsTheLogAfresh) {
    runs->refused(KEY, "Doom", "why");
    runs->show(KEY);

    const int generation = State::get().runs.lineGeneration;

    began(waiting(false));
    shell.advance(0.06);

    ASSERT_FALSE(shown().empty());
    EXPECT_EQ(shown().front(), "Launching Doom.");
    EXPECT_NE(State::get().runs.lineGeneration, generation);
}

TEST_F(RunsTest, ARunThatExitsCleanlyCloses) {
    began(launch("exit 0", false));

    ASSERT_TRUE(settles());

    EXPECT_EQ(runs->stateOf(KEY), State::RunState::Closed);
    EXPECT_EQ(runs->reasonOf(KEY), "");
    EXPECT_FALSE(State::get().runs.busy);

    runs->show(KEY);

    EXPECT_EQ(shown().back(), "It closed.");
}

TEST_F(RunsTest, ARunThatExitsWithAnErrorFails) {
    began(launch("exit 3", false));

    ASSERT_TRUE(settles());

    EXPECT_EQ(runs->stateOf(KEY), State::RunState::Failed);
    EXPECT_EQ(runs->reasonOf(KEY), "It stopped with an error (code 3).");
}

TEST_F(RunsTest, ARunKilledFromOutsideFails) {
    const Spawn::Child child = waiting(false);

    began(child);
    ttk::Process::force(child.id);

    ASSERT_TRUE(settles());

    EXPECT_EQ(runs->stateOf(KEY), State::RunState::Failed);
    EXPECT_EQ(runs->reasonOf(KEY), "It was killed (signal 9).");
}

TEST_F(RunsTest, AnEndedRunKeepsItsTab) {
    began(launch("exit 0", true));

    ASSERT_TRUE(settles());

    EXPECT_EQ(State::get().runs.docked, (Keys{KEY}));
}

TEST_F(RunsTest, CloseAsksTheRunToStop) {
    began(waiting(true));
    runs->show(KEY);
    runs->close(KEY);

    EXPECT_EQ(runs->stateOf(KEY), State::RunState::Stopping);
    EXPECT_TRUE(runs->alive(KEY));
    EXPECT_EQ(State::get().runs.showing, "");
}

TEST_F(RunsTest, AStoppedRunClosesWithoutBlameAndLosesItsTab) {
    began(waiting(true));
    runs->close(KEY);

    ASSERT_TRUE(settles());

    EXPECT_EQ(runs->stateOf(KEY), State::RunState::Closed);
    EXPECT_EQ(runs->reasonOf(KEY), "");
    EXPECT_TRUE(State::get().runs.docked.empty());

    runs->show(KEY);

    const std::vector<std::string> lines = shown();

    EXPECT_TRUE(std::ranges::find(lines, "ZDL4 asked it to quit.") != lines.end());
    EXPECT_EQ(lines.back(), "It was stopped.");
}

TEST_F(RunsTest, ASecondCloseForcesARunThatIgnoresTheFirst) {
    began(waiting(true, "trap '' TERM; echo ready; "));
    runs->show(KEY);

    ASSERT_TRUE(Spawn::until(shell, 0.06, [this] { return !shown().empty() && shown().back() == "ready"; }));

    runs->close(KEY);
    shell.advance(TICK);

    EXPECT_EQ(runs->stateOf(KEY), State::RunState::Stopping);

    runs->close(KEY);

    ASSERT_TRUE(settles());

    EXPECT_EQ(runs->stateOf(KEY), State::RunState::Closed);

    runs->show(KEY);

    const std::vector<std::string> lines = shown();

    EXPECT_TRUE(std::ranges::find(lines, "ZDL4 took it down.") != lines.end());
}

TEST_F(RunsTest, ALaunchOverARunningOneKeepsTheFirstAlive) {
    const Spawn::Child first = waiting(false);
    const Spawn::Child second = launch("exit 0", false);

    began(first);
    began(second);

    ASSERT_TRUE(Spawn::until(shell, TICK, [this] { return runs->stateOf(KEY) == State::RunState::Closed; }));

    EXPECT_TRUE(runs->alive(KEY));
    EXPECT_TRUE(State::get().runs.busy);
}

TEST_F(RunsTest, ClosingTheCardStopsItsOrphan) {
    const Spawn::Child first = waiting(false);
    const Spawn::Child second = launch("exit 0", false);

    began(first);
    began(second);

    ASSERT_TRUE(Spawn::until(shell, TICK, [this] { return runs->stateOf(KEY) == State::RunState::Closed; }));

    runs->close(KEY);

    ASSERT_TRUE(settles());

    EXPECT_FALSE(State::get().runs.busy);
}

TEST_F(RunsTest, ARefusalOverARunningLaunchKeepsItAlive) {
    began(waiting(false));

    runs->refused(KEY, "Doom", "The second launch would not start");

    EXPECT_TRUE(runs->alive(KEY));
    EXPECT_TRUE(State::get().runs.busy);
}

}
