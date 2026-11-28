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

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "core/config/FileBrowser.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

FileEntry at(const std::string &file, const std::uint16_t group, const bool enabled = true) {
    return FileEntry{.file = file, .enabled = enabled, .gone = false, .group = group};
}

// Each file name with its group, as in "a.wad:1 m.pk3:0".
std::string spelled(const std::vector<FileEntry> &files) {
    std::string out;

    for (const FileEntry &entry : files) {
        out += (out.empty() ? "" : " ") + fs::path(entry.file).filename().string() + ":" + std::to_string(entry.group);
    }

    return out;
}

std::vector<std::string> keys(const std::vector<std::string> &paths) {
    std::vector<std::string> out;

    out.reserve(paths.size());

    for (const std::string &path : paths) {
        out.push_back(fileKey(path));
    }

    return out;
}

std::string
picked(std::vector<FileEntry> files, const std::vector<std::string> &rows, const std::vector<std::string> &picks) {
    FileBrowser::pick(files, keys(rows), picks);

    return spelled(files);
}

bool wad(const std::string_view name) {
    return name.ends_with(".wad") || name.ends_with(".WAD") || name.ends_with(".pk3");
}

void touch(const fs::path &file) {
    fs::create_directories(file.parent_path());
    std::ofstream(file) << "x";
}

}

TEST(FileBrowserPick, ASwapLandsWhereTheOldPickWasInItsGroup) {
    std::vector<FileEntry> files = {at("/mods/m.pk3", 0), at("/maps/a.wad", 2, false), at("/mods/x.pk3", 1)};

    EXPECT_TRUE(FileBrowser::pick(files, keys({"/maps/a.wad", "/maps/b.wad"}), {"/maps/b.wad"}));
    EXPECT_EQ(spelled(files), "m.pk3:0 b.wad:2 x.pk3:1");
    EXPECT_TRUE(files[1].enabled);
    EXPECT_EQ(picked({at("/maps/a.wad", 1), at("/mods/m.pk3", 0), at("/maps/b.wad", 2)},
                     {"/maps/a.wad", "/maps/b.wad", "/maps/c.wad"},
                     {"/maps/a.wad", "/maps/c.wad"}),
              "a.wad:1 m.pk3:0 c.wad:2");
}

TEST(FileBrowserPick, NewPicksFollowTheLastPickInLoadOrderAndJoinItsGroup) {
    EXPECT_EQ(picked({at("/maps/a.wad", 1), at("/mods/m.pk3", 0), at("/maps/b.wad", 2), at("/mods/x.pk3", 0)},
                     {"/maps/a.wad", "/maps/b.wad", "/maps/c.wad"},
                     {"/maps/a.wad", "/maps/b.wad", "/maps/c.wad"}),
              "a.wad:1 m.pk3:0 b.wad:2 c.wad:2 x.pk3:0");
}

TEST(FileBrowserPick, AFirstPickGoesToTheTopUngrouped) {
    EXPECT_EQ(picked({at("/mods/m.pk3", 1), at("/mods/x.pk3", 2)}, {"/maps/a.wad"}, {"/maps/a.wad"}),
              "a.wad:0 m.pk3:1 x.pk3:2");
}

TEST(FileBrowserPick, KeptPicksStayWhereTheyAre) {
    std::vector<FileEntry> files = {at("/maps/b.wad", 1, false), at("/mods/m.pk3", 0), at("/maps/a.wad", 2)};

    EXPECT_FALSE(FileBrowser::pick(files, keys({"/maps/a.wad", "/maps/b.wad"}), {"/maps/a.wad", "/maps/b.wad"}));
    EXPECT_EQ(spelled(files), "b.wad:1 m.pk3:0 a.wad:2");
    EXPECT_FALSE(files[0].enabled);
}

TEST(FileBrowserPick, RowsThatAreNotPickedGo) {
    const std::vector<FileEntry> files = {
            at("/maps/a.wad", 1), at("/mods/m.pk3", 0), at("/maps/deep/c.wad", 0), at("/maps2/z.wad", 0)};
    const std::vector<std::string> rows = {"/maps/a.wad", "/maps/deep/c.wad"};

    EXPECT_EQ(picked(files, rows, {"/maps/a.wad"}), "a.wad:1 m.pk3:0 z.wad:0");
    EXPECT_EQ(picked(files, rows, {}), "m.pk3:0 z.wad:0");
}

TEST(FileBrowserPick, ListFilesBelowTheFolderThatAreNoRowsStay) {
    EXPECT_EQ(picked({at("/maps/a.wad", 1), at("/maps/sub", 0), at("/maps/new.wad", 0)},
                     {"/maps/a.wad", "/maps/b.wad", "/maps/sub/c.wad"},
                     {"/maps/b.wad"}),
              "b.wad:1 sub:0 new.wad:0");
}

TEST(FileBrowserPick, SeveralNewPicksKeepTheOrderGiven) {
    const std::vector<std::string> rows = {"/maps/c.wad", "/maps/a.wad", "/maps/sub/b.wad"};

    EXPECT_EQ(picked({}, rows, rows), "c.wad:0 a.wad:0 b.wad:0");
}

TEST(FileBrowserPick, APickTheListHoldsIsNotAddedTwice) {
    std::vector<FileEntry> files = {at("/maps/a.wad", 1)};

    EXPECT_FALSE(FileBrowser::pick(files, keys({"/maps/a.wad"}), {"/maps/a.wad", "/maps/./a.wad", "/maps/a.wad"}));
    EXPECT_EQ(spelled(files), "a.wad:1");
}

TEST(FileBrowserScan, ListsTheTreeFoldersFirstInNaturalOrderAndLeavesEmptyAndHiddenFoldersOut) {
    const fs::path folder = tests::Sandbox::scratch("browser") / "maps";

    touch(folder / "MAP10.wad");
    touch(folder / "map2.wad");
    touch(folder / "notes.txt");
    touch(folder / "sub" / "deep" / "b.pk3");
    touch(folder / "sub" / "a.wad");
    touch(folder / "megawads" / "Zed.WAD");
    touch(folder / "texts" / "readme.txt");
    touch(folder / ".cache" / "old.wad");
    fs::create_directories(folder / "empty" / "inner");

    const std::vector<FileBrowser::Entry> found = FileBrowser::scan(folder, wad);
    std::string tree;

    for (const FileBrowser::Entry &entry : found) {
        tree += std::string(entry.depth * 2, ' ') + entry.name + (entry.folder ? "/" : "") + "\n";
    }

    EXPECT_EQ(tree, "megawads/\n  Zed.WAD\nsub/\n  deep/\n    b.pk3\n  a.wad\nmap2.wad\nMAP10.wad\n");
    EXPECT_EQ(found[3].path, (folder / "sub" / "deep").generic_string());
}

TEST(FileBrowserScan, TheWalkStopsAfterTheCapCountingFolders) {
    const fs::path folder = tests::Sandbox::scratch("browser") / "capped";

    touch(folder / "a.wad");
    touch(folder / "sub" / "b.wad");

    EXPECT_EQ(FileBrowser::scan(folder, wad, 2).size(), 1U);
    EXPECT_EQ(FileBrowser::scan(folder, wad).size(), 3U);
}

TEST(FileBrowserScan, AFolderThatIsNotThereListsNothing) {
    EXPECT_TRUE(FileBrowser::scan(tests::Sandbox::scratch("browser") / "none", wad).empty());
    EXPECT_TRUE(FileBrowser::scan("", wad).empty());
}

TEST(FileBrowserScan, AFolderThatCannotBeReadIsLeftOut) {
    const fs::path folder = tests::Sandbox::scratch("browser") / "locked";

    touch(folder / "a.wad");
    touch(folder / "shut" / "b.wad");
    fs::permissions(folder / "shut", fs::perms::none);

    const std::vector<FileBrowser::Entry> found = FileBrowser::scan(folder, wad);

    fs::permissions(folder / "shut", fs::perms::owner_all);

    ASSERT_EQ(found.size(), 1U);
    EXPECT_EQ(found[0].name, "a.wad");
}
