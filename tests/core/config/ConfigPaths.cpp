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

#include "ttk/system/Env.h"

#include "core/config/ConfigPaths.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

using Paths = std::vector<fs::path>;

fs::path sandbox(const char *part) {
    return tests::Sandbox::root() / part;
}

}

TEST(ConfigPaths, IsOneInstance) {
    EXPECT_EQ(&ConfigPaths::get(), &ConfigPaths::get());
}

TEST(ConfigPaths, TheUserConfigIsInTheConfigDirectory) {
    EXPECT_EQ(ConfigPaths::get().configPath(ConfigPaths::USER), sandbox("config") / "qzdl" / "zdl.json");
}

TEST(ConfigPaths, TheSystemConfigIsInTheLastSystemDirectory) {
    EXPECT_EQ(ConfigPaths::get().configPath(ConfigPaths::SYSTEM), sandbox("etc") / "qzdl" / "zdl.json");
}

TEST(ConfigPaths, TheFileConfigIsABareName) {
    EXPECT_EQ(ConfigPaths::get().configPath(ConfigPaths::FILE), fs::path("zdl.json"));
}

TEST(ConfigPaths, TheLegacyUserIniIsTheOldVendorFile) {
    EXPECT_EQ(ConfigPaths::get().legacyConfigPath(ConfigPaths::USER),
              (Paths{sandbox("config") / "Vectec Software" / "qZDL.conf"}));
}

TEST(ConfigPaths, TheLegacySystemIniIsTheOldVendorFile) {
    EXPECT_EQ(ConfigPaths::get().legacyConfigPath(ConfigPaths::SYSTEM),
              (Paths{sandbox("etc") / "Vectec Software" / "qZDL.conf"}));
}

TEST(ConfigPaths, TheLegacyFileIniIsABareName) {
    EXPECT_EQ(ConfigPaths::get().legacyConfigPath(ConfigPaths::FILE), (Paths{fs::path("zdl.ini")}));
}

TEST(ConfigPaths, AScopeOutOfRangeHasNoPath) {
    EXPECT_TRUE(ConfigPaths::get().configPath(ConfigPaths::NUM_CONFS).empty());
    EXPECT_TRUE(ConfigPaths::get().legacyConfigPath(ConfigPaths::NUM_CONFS).empty());
    EXPECT_TRUE(ConfigPaths::get().configPath(static_cast<ConfigPaths::Scope>(200)).empty());
}

TEST(ConfigPaths, ThePathsAreFixedAtFirstUse) {
    const fs::path before = ConfigPaths::get().configPath(ConfigPaths::USER);
    const std::string saved = ttk::Env::get("XDG_CONFIG_HOME");

    ttk::Env::set("XDG_CONFIG_HOME", tests::Sandbox::scratch("config-elsewhere").string().c_str());

    const fs::path after = ConfigPaths::get().configPath(ConfigPaths::USER);

    ttk::Env::set("XDG_CONFIG_HOME", saved.c_str());

    EXPECT_EQ(after, before);
}
