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

#include <gtest/gtest.h>

#include "ttk/system/Paths.h"
#include "ttk/toolkit/controls/StatusIndicator.h"

#include "gui/draw/Hues.h"
#include "support/Sandbox.h"

namespace {

class Sandboxed final : public ::testing::Environment {
public:
    void SetUp() override { tests::Sandbox::enter(); }

    void TearDown() override { tests::Sandbox::leave(); }
};

}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new Sandboxed);

    // What qzdl_gui.cpp and App::start set up before anything reads a path or paints
    // a status. Paths caches what it finds, so the sandbox is entered first, and the
    // executable is put inside it so the portable config lookup stays there too.
    tests::Sandbox::enter();

    ttk::Paths::set_application("qzdl");
    ttk::Paths::set_executable(tests::Sandbox::root() / "bin" / "ZDL4");
    ttk::StatusIndicator::set_tones(&Hues::statusTones);

    const int failed = RUN_ALL_TESTS();

    // A run that only listed the tests never reached the environment's TearDown.
    tests::Sandbox::leave();

    return failed;
}
