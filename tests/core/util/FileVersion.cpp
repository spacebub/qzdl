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

#include <gtest/gtest.h>

#include "core/util/FileVersion.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

TEST(FileVersion, TheKeyNamesTheTranslationInEightHexDigits) {
    EXPECT_EQ(FileVersion::key(0x0409, 0x04b0), "\\StringFileInfo\\040904b0\\FileVersion");
    EXPECT_EQ(FileVersion::key(0x0000, 0x04e4), "\\StringFileInfo\\000004e4\\FileVersion");
    EXPECT_EQ(FileVersion::key(0xffff, 0xffff), "\\StringFileInfo\\ffffffff\\FileVersion");
}

TEST(FileVersion, AMissingFileHasNone) {
    EXPECT_EQ(FileVersion::of(tests::Sandbox::scratch("file-version") / "missing.exe"), "");
}

TEST(FileVersion, AFileWithoutAVersionResourceHasNone) {
    const fs::path file = tests::Sandbox::scratch("file-version") / "gzdoom.exe";

    std::ofstream(file, std::ios::binary) << "MZ";

    EXPECT_EQ(FileVersion::of(file), "");
}

}
