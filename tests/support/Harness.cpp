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
#include <vector>

#include <gtest/gtest.h>

#include "ttk/draw/Theme.h"
#include "ttk/system/Env.h"
#include "ttk/system/Paths.h"

#include "core/config/ConfigPaths.h"
#include "core/config/Session.h"
#include "core/wad/Wad.h"
#include "gui/state/State.h"
#include "support/Canvas.h"
#include "support/Corpus.h"
#include "support/Fixtures.h"
#include "support/Rig.h"
#include "support/Sandbox.h"
#include "support/Shell.h"

namespace {

namespace fs = std::filesystem;

bool under(const fs::path &path, const fs::path &root) {
    return path.string().starts_with(root.string());
}

}

TEST(Sandbox, PointsEveryPathInsideItself) {
    const fs::path &root = tests::Sandbox::root();

    ASSERT_FALSE(root.empty());
    EXPECT_TRUE(fs::is_directory(root));

    EXPECT_TRUE(under(ttk::Paths::home_directory(), root));
    EXPECT_TRUE(under(ttk::Paths::data_directory(), root));
    EXPECT_TRUE(under(ConfigPaths::get().configPath(ConfigPaths::USER), root));
    EXPECT_TRUE(under(ttk::Env::get("PATH"), root));
}

TEST(Sandbox, ScratchStartsEmpty) {
    const fs::path first = tests::Sandbox::scratch("harness");

    ASSERT_TRUE(fs::is_directory(first));

    std::ofstream(first / "left.txt") << "x";

    const fs::path again = tests::Sandbox::scratch("harness");

    EXPECT_EQ(first, again);
    EXPECT_TRUE(fs::is_empty(again));
}

TEST(Corpus, TheGameFileIsAnIwadWith32Maps) {
    Wad wad(tests::Corpus::iwad());

    EXPECT_TRUE(wad.isGame());
    EXPECT_EQ(wad.mapNames().size(), 32U);
    EXPECT_EQ(wad.iwadinfoName(), "Test Doom");
}

TEST(Corpus, AddonsAreSmallPwads) {
    const std::vector<std::string> &paths = tests::Corpus::addons(3);

    ASSERT_EQ(paths.size(), 3U);

    for (const std::string &path : paths) {
        Wad wad(path);

        EXPECT_FALSE(wad.isGame()) << path;
        EXPECT_EQ(wad.mapNames().size(), 1U) << path;
    }
}

TEST(Fixtures, ConfigHasTheShapeAsked) {
    const Config made = tests::Fixtures::config(5, 4);

    EXPECT_EQ(made.profiles.size(), 5U);
    EXPECT_EQ(made.profiles.front().files.size(), 4U);
    EXPECT_EQ(made.activeProfileId, made.profiles.front().id);
    EXPECT_EQ(made.iwads.size(), 12U);
}

TEST(Fixtures, InstallReplacesTheSessionConfig) {
    tests::Fixtures::install(tests::Fixtures::config(2, 1));

    EXPECT_EQ(Session::get().config().profiles.size(), 2U);

    tests::Fixtures::install(Config{});

    EXPECT_TRUE(Session::get().config().profiles.empty());
}

TEST(Shell, ATimerFiresOnceTheClockPassesIt) {
    tests::Shell shell;
    int fired = 0;

    const int id = shell.after(1.0, [&fired] { ++fired; });

    shell.advance(0.5);

    EXPECT_EQ(fired, 0);
    EXPECT_TRUE(shell.holds(id));
    EXPECT_DOUBLE_EQ(shell.nextDue(), 0.5);

    shell.advance(0.5);

    EXPECT_EQ(fired, 1);
    EXPECT_FALSE(shell.holds(id));
    EXPECT_EQ(shell.pending(), 0U);
}

TEST(Shell, EveryRepeatsUntilCancelled) {
    tests::Shell shell;
    int fired = 0;

    const int id = shell.every(0.25, [&fired] { ++fired; });

    for (int step = 0; step < 4; ++step) {
        shell.advance(0.25);
    }

    EXPECT_EQ(fired, 4);

    shell.cancel(id);
    shell.advance(1.0);

    EXPECT_EQ(fired, 4);
}

TEST(Shell, ARepeatingTimerRunsOnceHoweverFarTheClockJumps) {
    tests::Shell shell;
    int fired = 0;

    shell.every(0.25, [&fired] { ++fired; });
    shell.advance(1.0);

    EXPECT_EQ(fired, 1);
    EXPECT_DOUBLE_EQ(shell.nextDue(), 0.25);
}

TEST(Shell, DueTimersRunInDueOrder) {
    tests::Shell shell;
    std::vector<int> order;

    shell.after(0.3, [&order] { order.push_back(3); });
    shell.after(0.1, [&order] { order.push_back(1); });
    shell.after(0.2, [&order] { order.push_back(2); });

    shell.advance(1.0);

    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
}

TEST(Shell, PostedFunctionsRunOnDrainInOrder) {
    tests::Shell shell;
    std::vector<int> order;

    shell.post([&order] { order.push_back(1); });
    shell.post([&order] { order.push_back(2); });

    EXPECT_EQ(shell.posted(), 2U);
    EXPECT_TRUE(order.empty());

    shell.drain();

    EXPECT_EQ(order, (std::vector<int>{1, 2}));
    EXPECT_EQ(shell.posted(), 0U);
}

TEST(Shell, ATimerCanCancelItself) {
    tests::Shell shell;
    int id = 0;
    int fired = 0;

    id = shell.every(0.1, [&] {
        ++fired;
        shell.cancel(id);
    });

    shell.advance(1.0);

    EXPECT_EQ(fired, 1);
}

TEST(Shell, WindowControlsAreCounted) {
    tests::Shell shell;

    shell.minimize();
    shell.toggle_maximize();

    EXPECT_EQ(shell.minimized, 1);
    EXPECT_TRUE(shell.maximized());

    shell.toggle_maximize();
    shell.stop();

    EXPECT_FALSE(shell.maximized());
    EXPECT_EQ(shell.toggled, 2);
    EXPECT_TRUE(shell.stopped);
}

class CanvasTest : public tests::Painted {};

TEST_F(CanvasTest, AFrameFillsTheBackground) {
    tests::Canvas canvas(64, 48);

    EXPECT_GT(canvas.full(), 0U);
    EXPECT_EQ(canvas.at(0, 0) & 0x00ffffffU, ttk::Theme::palette().background.value & 0x00ffffffU);
    EXPECT_EQ(canvas.at(63, 47) & 0x00ffffffU, ttk::Theme::palette().background.value & 0x00ffffffU);
    EXPECT_EQ(canvas.at(64, 48), 0U);
}

TEST_F(CanvasTest, NothingDamagedPaintsNothing) {
    tests::Canvas canvas(64, 48);

    canvas.full();

    EXPECT_EQ(canvas.frame(), 0U);
    EXPECT_TRUE(canvas.matchesFull());
}

TEST_F(CanvasTest, TicksMoveTheClock) {
    tests::Canvas canvas(64, 48);

    EXPECT_DOUBLE_EQ(canvas.now(), 0.0);

    canvas.run(6, 60.0);

    EXPECT_NEAR(canvas.now(), 0.1, 1e-9);
    EXPECT_DOUBLE_EQ(canvas.ui().now(), canvas.now());
}

class RigTest : public tests::Rigged {};

TEST_F(RigTest, StartsOnAnEmptyLibrary) {
    EXPECT_TRUE(Session::get().config().profiles.empty());
    EXPECT_EQ(State::get().sys.page, State::Page::Library);

    rig().sync();

    EXPECT_TRUE(rig().library().visible());
    EXPECT_FALSE(rig().hasProfile());
    EXPECT_GT(canvas().full(), 0U);
}

TEST_F(RigTest, AFixturePushedIntoTheTreeShowsOnTheShelf) {
    tests::Fixtures::install(tests::Fixtures::config(3, 2));

    rig().config().reload();
    rig().sync();

    EXPECT_EQ(State::get().cfg.profileCards.size(), 3U);
    EXPECT_EQ(State::get().cfg.shelfProfiles.size(), 3U);
}

TEST_F(RigTest, GoingToAPageBuildsItOnce) {
    rig().go(State::Page::Settings);

    EXPECT_TRUE(rig().dirty());

    rig().turn();

    EXPECT_FALSE(rig().dirty());
    EXPECT_TRUE(rig().hasSettings());
    EXPECT_TRUE(rig().settings().visible());
    EXPECT_FALSE(rig().library().visible());
    EXPECT_EQ(rig().visited, (std::vector<State::Page>{State::Page::Settings}));
}

TEST_F(RigTest, StateChangesMarkTheRigDirty) {
    rig().sync();

    EXPECT_FALSE(rig().dirty());

    State::get().touch();

    EXPECT_TRUE(rig().dirty());
}

TEST_F(RigTest, ReadyLeavesNothingPosted) {
    tests::Fixtures::install(tests::Fixtures::grounded(2, 1));

    rig().config().reload();
    rig().ready();

    EXPECT_EQ(rig().shell().posted(), 0U);
}

TEST_F(RigTest, ANoticeTouchesTheState) {
    rig().sync();
    rig().notify().info("hello");

    EXPECT_TRUE(rig().dirty());
    EXPECT_EQ(rig().notify().messages().size(), 1U);
}
