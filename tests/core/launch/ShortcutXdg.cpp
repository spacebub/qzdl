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
#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "ttk/system/Paths.h"

#include "core/launch/Shortcut.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

std::string read(const fs::path &path) {
    std::ifstream in(path);

    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

class ShortcutXdgTest : public ::testing::Test {
protected:
    fs::path zdl = tests::Sandbox::scratch("shortcuts") / "a b$c.zdl";
};

TEST_F(ShortcutXdgTest, AnEntryLaunchesTheZdlFromTheApplicationsMenu) {
    std::string why;
    const fs::path made = Shortcut::make("My Doom", zdl, {}, "png", &why);

    ASSERT_FALSE(made.empty()) << why;
    EXPECT_EQ(made.parent_path(), ttk::Paths::data_directory().parent_path() / "applications");

    const std::string entry = read(made);

    EXPECT_NE(entry.find("\nName=My Doom\n"), std::string::npos);
    EXPECT_NE(entry.find("\" --launch \"" + zdl.parent_path().string() + "/a b\\\\$c.zdl\"\n"), std::string::npos);
    EXPECT_NE(entry.find("\nIcon=" + (zdl.parent_path() / "zdl4.png").string() + "\n"), std::string::npos);
    EXPECT_EQ(read(zdl.parent_path() / "zdl4.png"), "png");
}

TEST_F(ShortcutXdgTest, MakingItAgainWritesOverTheSameEntry) {
    const fs::path first = Shortcut::make("Old", zdl, {}, {}, nullptr);
    const fs::path second = Shortcut::make("New", zdl, "/c/zdl.json", {}, nullptr);

    EXPECT_EQ(first, second);
    EXPECT_NE(read(second).find("\nName=New\n"), std::string::npos);
    EXPECT_NE(read(second).find(" \"/c/zdl.json\"\n"), std::string::npos);
}

}
