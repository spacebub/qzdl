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

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Process.h"

#include "Spawn.h"
#include "gui/services/RunLog.h"
#include "support/Sandbox.h"
#include "support/Shell.h"

namespace {

namespace fs = std::filesystem;
namespace Spawn = tests::Spawn;

// What the log polls on.
constexpr double POLL = 0.12;

// What it gathers lines for before a redraw.
constexpr double BATCH = 0.06;

std::vector<std::string> texts(const RunLog &log) {
    std::vector<std::string> out;

    for (const RunLog::Line &line : log.lines()) {
        out.push_back(line.text);
    }

    return out;
}

class RunLogTest : public ::testing::Test {
protected:
    void SetUp() override {
        log = std::make_unique<RunLog>(&shell);
        log->published = [this] { ++published; };
    }

    void TearDown() override {
        log.reset();
        Spawn::end(started);
    }

    void capture(const std::string &body) {
        const Spawn::Child child = Spawn::start(Spawn::script(directory, "port", body), true);

        ASSERT_NE(child.output, ttk::Process::NOTHING);

        started.push_back(child.id);
        log->watch(child.output);

        ASSERT_TRUE(Spawn::until(shell, POLL, [this] { return !log->live(); }));
    }

    fs::path directory = tests::Sandbox::scratch("runlog");

    tests::Shell shell;
    std::unique_ptr<RunLog> log;
    int published = 0;
    std::vector<ttk::Process::Id> started;
};

TEST_F(RunLogTest, StartsEmptyAndQuiet) {
    EXPECT_TRUE(log->lines().empty());
    EXPECT_EQ(log->generation(), 0);
    EXPECT_EQ(log->dropped(), 0U);
    EXPECT_FALSE(log->active());
    EXPECT_FALSE(log->live());
    EXPECT_EQ(log->text(), "");
}

TEST_F(RunLogTest, ANoteWhileInactiveLandsAtOnceAsOwn) {
    log->note("Launching Doom.");

    ASSERT_EQ(log->lines().size(), 1U);
    EXPECT_EQ(log->lines().front().text, "Launching Doom.");
    EXPECT_TRUE(log->lines().front().own);
    EXPECT_EQ(published, 0);
}

TEST_F(RunLogTest, ANoteWhileActiveWaitsForTheBatch) {
    log->setActive(true);
    log->note("one");
    log->note("two");

    EXPECT_TRUE(log->lines().empty());

    shell.advance(BATCH);

    EXPECT_EQ(texts(*log), (std::vector<std::string>{"one", "two"}));
    EXPECT_EQ(published, 1);
}

TEST_F(RunLogTest, ActivatingAfterMissedLinesStartsAGeneration) {
    log->note("missed");
    log->setActive(true);

    EXPECT_EQ(log->generation(), 1);
    EXPECT_EQ(published, 1);
}

TEST_F(RunLogTest, ActivatingWithNothingMissedKeepsTheGeneration) {
    log->setActive(true);
    log->setActive(false);
    log->setActive(true);

    EXPECT_EQ(log->generation(), 0);
    EXPECT_EQ(published, 0);
}

TEST_F(RunLogTest, ActivatingTwiceIsOnce) {
    log->note("missed");
    log->setActive(true);
    log->setActive(true);

    EXPECT_EQ(log->generation(), 1);
    EXPECT_TRUE(log->active());
}

TEST_F(RunLogTest, TextJoinsTheLines) {
    log->note("one");
    log->note("two");
    log->note("three");

    EXPECT_EQ(log->text(), "one\ntwo\nthree");
}

TEST_F(RunLogTest, ClearEmptiesAndStartsAGeneration) {
    log->note("one");
    log->clear();

    EXPECT_TRUE(log->lines().empty());
    EXPECT_EQ(log->generation(), 1);
    EXPECT_EQ(published, 1);
}

TEST_F(RunLogTest, ClearDropsANoteStillWaitingForItsBatch) {
    log->setActive(true);
    log->note("waiting");
    log->clear();

    shell.advance(BATCH);

    EXPECT_TRUE(log->lines().empty());
}

TEST_F(RunLogTest, PastTheLimitTheOldestGoAndAreCounted) {
    for (int at = 0; at < 4005; ++at) {
        log->note(std::to_string(at));
    }

    EXPECT_EQ(log->lines().size(), 4000U);
    EXPECT_EQ(log->dropped(), 5U);
    EXPECT_EQ(log->lines().front().text, "5");
    EXPECT_EQ(log->lines().back().text, "4004");
}

TEST_F(RunLogTest, ClearForgetsWhatWasDropped) {
    for (int at = 0; at < 4001; ++at) {
        log->note("x");
    }

    log->clear();

    EXPECT_EQ(log->dropped(), 0U);
}

TEST_F(RunLogTest, WatchingNothingStaysQuiet) {
    log->watch(ttk::Process::NOTHING);

    EXPECT_FALSE(log->live());
    EXPECT_EQ(shell.pending(), 0U);
}

TEST_F(RunLogTest, WatchingMakesItLiveAndPolls) {
    const fs::path fifo = Spawn::fifo(directory);
    const Spawn::Child child =
            Spawn::start(Spawn::script(directory, "port", "read x < \"" + fifo.string() + "\""), true);

    started.push_back(child.id);
    log->watch(child.output);

    EXPECT_TRUE(log->live());
    EXPECT_EQ(shell.pending(), 1U);
    EXPECT_EQ(published, 1);
}

TEST_F(RunLogTest, TheGamesLinesAreCaptured) {
    capture("echo one; echo two");

    EXPECT_EQ(texts(*log), (std::vector<std::string>{"one", "two"}));
    EXPECT_FALSE(log->lines().front().own);
}

TEST_F(RunLogTest, ThePollStopsWhenTheOutputEnds) {
    capture("echo done");

    EXPECT_FALSE(log->live());
    EXPECT_EQ(shell.pending(), 0U);
    EXPECT_GE(published, 1);
}

TEST_F(RunLogTest, ALastLineWithoutANewlineIsKept) {
    capture("echo first; printf tail");

    EXPECT_EQ(texts(*log), (std::vector<std::string>{"first", "tail"}));
}

TEST_F(RunLogTest, ACarriageReturnStartsTheLineOver) {
    capture(R"(printf 'loading 10%%\rloading 100%%\n')");

    EXPECT_EQ(texts(*log), (std::vector<std::string>{"loading 100%"}));
}

TEST_F(RunLogTest, TerminalControlSequencesAreDropped) {
    capture(R"(printf '\033[31mred\033[0m plain\n')");

    EXPECT_EQ(texts(*log), (std::vector<std::string>{"red plain"}));
}

TEST_F(RunLogTest, AnOverlongLineIsCutAtAThousandBytes) {
    capture(R"(i=0; s=; while [ $i -lt 2500 ]; do s="${s}x"; i=$((i+1)); done; printf '%s\n' "$s")");

    const std::vector<std::string> got = texts(*log);

    ASSERT_EQ(got.size(), 3U);
    EXPECT_EQ(got[0].size(), 1000U);
    EXPECT_EQ(got[1].size(), 1000U);
    EXPECT_EQ(got[2].size(), 500U);
}

TEST_F(RunLogTest, ACutBacksOffToACharacterBoundary) {
    capture(R"(i=0; s=; while [ $i -lt 999 ]; do s="${s}x"; i=$((i+1)); done; printf '%s\303\251\n' "$s")");

    const std::vector<std::string> got = texts(*log);

    ASSERT_EQ(got.size(), 2U);
    EXPECT_EQ(got[0], std::string(999, 'x'));
    EXPECT_EQ(got[1], "\xC3\xA9");
}

TEST_F(RunLogTest, NotesAndOutputShareTheLog) {
    log->note("$ port");

    capture("echo from the game");

    ASSERT_EQ(log->lines().size(), 2U);
    EXPECT_TRUE(log->lines()[0].own);
    EXPECT_FALSE(log->lines()[1].own);
}

TEST_F(RunLogTest, WhileActiveTheGamesLinesArriveInBatches) {
    log->setActive(true);

    capture("echo one");

    EXPECT_EQ(texts(*log), (std::vector<std::string>{"one"}));
    EXPECT_GE(published, 1);
}

TEST_F(RunLogTest, AFloodDropsTheOldestAndStartsAGeneration) {
    // The clock is held still until the child is gone, so the reader piles up more than it may hold.
    const Spawn::Child child = Spawn::start(
            Spawn::script(directory, "flood", "i=0; while [ $i -lt 20000 ]; do echo $i; i=$((i+1)); done"), true);

    ASSERT_NE(child.output, ttk::Process::NOTHING);

    log->watch(child.output);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);

    while (ttk::Process::poll(child.id) == ttk::Process::State::Running
           && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ASSERT_TRUE(Spawn::until(shell, POLL, [this] { return !log->live(); }));

    EXPECT_GE(log->generation(), 1);
    EXPECT_EQ(log->lines().size(), 4000U);
    EXPECT_EQ(log->lines().back().text, "19999");
}

TEST_F(RunLogTest, DestroyingItLeavesNoTimerBehind) {
    log->setActive(true);
    log->note("waiting");
    log.reset();

    EXPECT_EQ(shell.pending(), 0U);
}

TEST_F(RunLogTest, DestroyingItWhileTheGameRunsDoesNotWait) {
    const fs::path fifo = Spawn::fifo(directory);
    const Spawn::Child child =
            Spawn::start(Spawn::script(directory, "port", "read x < \"" + fifo.string() + "\""), true);

    started.push_back(child.id);
    log->watch(child.output);

    const auto began = std::chrono::steady_clock::now();

    log.reset();

    EXPECT_LT(std::chrono::steady_clock::now() - began, std::chrono::seconds(1));
}

TEST_F(RunLogTest, WatchingAgainLetsGoOfTheFirstStream) {
    const fs::path fifo = Spawn::fifo(directory);
    const Spawn::Child first =
            Spawn::start(Spawn::script(directory, "first", "read x < \"" + fifo.string() + "\""), true);

    started.push_back(first.id);
    log->watch(first.output);

    capture("echo second");

    EXPECT_EQ(texts(*log), (std::vector<std::string>{"second"}));
    EXPECT_EQ(shell.pending(), 0U);
}

}
