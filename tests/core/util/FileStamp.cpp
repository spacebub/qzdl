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
#include <fstream>

#include <gtest/gtest.h>

#include "core/util/FileStamp.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

TEST(FileStamp, AMissingPathIsNotThere) {
    const FileStamp stamp = FileStamp::of(tests::Sandbox::scratch("stamp") / "missing.wad");

    EXPECT_FALSE(stamp.there);
    EXPECT_EQ(stamp, FileStamp{});
}

TEST(FileStamp, AFileCarriesItsSize) {
    const fs::path file = tests::Sandbox::scratch("stamp") / "doom.wad";

    std::ofstream(file, std::ios::binary) << "IWAD";

    const FileStamp stamp = FileStamp::of(file);

    EXPECT_TRUE(stamp.there);
    EXPECT_EQ(stamp.size, 4U);
    EXPECT_EQ(stamp, FileStamp::of(file));
}

TEST(FileStamp, AFolderIsThereWithNoSize) {
    const FileStamp stamp = FileStamp::of(tests::Sandbox::scratch("stamp"));

    EXPECT_TRUE(stamp.there);
    EXPECT_EQ(stamp.size, 0U);
}

TEST(FileStamp, ARewriteChangesTheStamp) {
    const fs::path file = tests::Sandbox::scratch("stamp") / "doom.wad";

    std::ofstream(file, std::ios::binary) << "IWAD";

    const FileStamp before = FileStamp::of(file);

    std::ofstream(file, std::ios::binary) << "IWAD";
    fs::last_write_time(file, fs::last_write_time(file) + std::chrono::seconds(5));

    EXPECT_NE(before, FileStamp::of(file));
}

TEST(FileStamp, AFarFutureDateStillTellsTwoTimesApart) {
    const fs::path file = tests::Sandbox::scratch("stamp") / "doom.wad";

    std::ofstream(file, std::ios::binary) << "IWAD";

    constexpr auto CENTURIES = std::chrono::years(300);

    fs::last_write_time(file, fs::last_write_time(file) + CENTURIES);

    const FileStamp first = FileStamp::of(file);

    fs::last_write_time(file, fs::last_write_time(file) + std::chrono::seconds(1));

    EXPECT_TRUE(first.there);
    EXPECT_NE(first, FileStamp::of(file));
    EXPECT_EQ(FileStamp::of(file.string()), FileStamp::of(file));
}

}
