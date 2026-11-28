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

#include "Model.h"
#include "core/config/Config.h"
#include "gui/model/BrowserBridge.h"
#include "gui/model/ConfigBridge.h"
#include "gui/state/State.h"

namespace {

class BrowserBridgeTest : public model::Bridged {
protected:
    BrowserBridge &browser() const { return bridge().browser(); }

    static State::BrowserState &state() { return State::get().browser; }

    [[nodiscard]] std::string maps() const { return (folder() / "maps").generic_string(); }
    [[nodiscard]] std::string other() const { return (folder() / "other").generic_string(); }
    [[nodiscard]] std::string map(const std::string &name) const { return maps() + "/" + name; }

    // Alpha with one add-on and a.wad picked, from a folder of three maps and a text file.
    void loaded(const bool on = true, const bool open = true) const {
        model::write(folder() / "maps" / "a.wad");
        model::write(folder() / "maps" / "b.wad");
        model::write(folder() / "maps" / "sub" / "c.pk3");
        model::write(folder() / "maps" / "notes.txt");
        model::write(folder() / "other" / "x.wad");

        Config made = model::world(2);

        made.general.fileBrowser = on;
        made.general.fileBrowserOpen = open;
        made.general.fileBrowserFolder = maps();
        made.profiles.front().files = {
                {.file = "/addons/one.wad", .enabled = true, .gone = false, .group = 0},
                {.file = map("a.wad"), .enabled = true, .gone = false, .group = 0},
        };

        install(made);
    }

    static std::vector<std::string> names() {
        std::vector<std::string> out;

        for (const State::BrowserRow &row : state().rows) {
            out.push_back(row.name + (row.folder ? "/" : "") + std::string(static_cast<size_t>(row.picks), '*'));
        }

        return out;
    }

    static std::vector<std::string> files() {
        std::vector<std::string> out;

        for (const FileEntry &entry : config().activeProfile().files) {
            out.push_back(entry.file);
        }

        return out;
    }
};

TEST_F(BrowserBridgeTest, TheFolderIsListedWithThePicksMarked) {
    loaded();

    EXPECT_EQ(names(), (std::vector<std::string>{"sub/", "c.pk3", "a.wad*", "b.wad"}));
    EXPECT_EQ(state().rows[1].path, map("sub/c.pk3"));
    EXPECT_EQ(state().rows[1].depth, 1U);
    EXPECT_EQ(state().rows[0].end, 2U);
    EXPECT_EQ(state().files, 3U);
    EXPECT_EQ(state().folder, maps());
    EXPECT_FALSE(state().ownFolder);
    EXPECT_TRUE(state().open);
    EXPECT_EQ(state().shown, (std::vector<int>{0, 2, 3}));
}

TEST_F(BrowserBridgeTest, NothingIsListedWhileTheSwitchIsOff) {
    loaded(false);

    EXPECT_TRUE(state().rows.empty());
    EXPECT_EQ(state().folder, "");

    bridge().settings().setFileBrowser(true);

    EXPECT_TRUE(cfg().fileBrowser);
    EXPECT_EQ(names().size(), 4U);
}

TEST_F(BrowserBridgeTest, AFoldedBandListsTheFolderOnlyOnceItOpens) {
    loaded(true, false);

    EXPECT_TRUE(state().rows.empty());
    EXPECT_EQ(state().folder, maps());

    browser().setOpen(true);

    EXPECT_EQ(names(), (std::vector<std::string>{"sub/", "c.pk3", "a.wad*", "b.wad"}));

    browser().setOpen(false);
    bridge().lists().addFiles({map("b.wad")});

    EXPECT_EQ(names(), (std::vector<std::string>{"sub/", "c.pk3", "a.wad*", "b.wad"})) << "folded rows stay";
}

TEST_F(BrowserBridgeTest, AFileListChangeMarksThePicksWithoutListingAgain) {
    loaded();
    model::write(folder() / "maps" / "d.wad");

    const int rev = state().rev;

    bridge().lists().addFiles({map("b.wad")});

    EXPECT_EQ(names(), (std::vector<std::string>{"sub/", "c.pk3", "a.wad*", "b.wad*"}));
    EXPECT_GT(state().rev, rev);

    browser().setOpen(true);

    EXPECT_TRUE(state().open);
    EXPECT_TRUE(config().general.fileBrowserOpen);
    EXPECT_EQ(names(), (std::vector<std::string>{"sub/", "c.pk3", "a.wad*", "b.wad*", "d.wad"}));
}

TEST_F(BrowserBridgeTest, NewPicksReplaceTheOldOnesInTheFileListAndAreSaved) {
    loaded();

    browser().applyPicks({map("b.wad"), map("sub/c.pk3")});

    const std::vector<std::string> want = {"/addons/one.wad", map("b.wad"), map("sub/c.pk3")};

    EXPECT_EQ(files(), want);
    EXPECT_EQ(names(), (std::vector<std::string>{"sub/*", "c.pk3*", "a.wad", "b.wad*"}));
    ASSERT_EQ(cfg().files.size(), 3U);
    EXPECT_EQ(cfg().files[1].name, "b.wad");

    settle();

    std::vector<std::string> kept;

    for (const FileEntry &entry : saved().activeProfile().files) {
        kept.push_back(entry.file);
    }

    EXPECT_EQ(kept, want);
}

TEST_F(BrowserBridgeTest, TheOwnFolderOfAProfileWinsUntilItIsCleared) {
    loaded();

    browser().setOwnFolder(other());

    EXPECT_EQ(config().activeProfile().fileBrowserFolder, other());
    EXPECT_TRUE(state().ownFolder);
    EXPECT_EQ(state().folder, other());
    EXPECT_EQ(names(), (std::vector<std::string>{"x.wad"}));

    browser().setOwnFolder({});

    EXPECT_TRUE(config().activeProfile().fileBrowserFolder.empty());
    EXPECT_FALSE(state().ownFolder);
    EXPECT_EQ(names().size(), 4U);
}

TEST_F(BrowserBridgeTest, AnotherProfileListsItsOwnFolder) {
    loaded();
    config().profiles[1].fileBrowserFolder = other();

    bridge().profile().setProfileIndex(1);

    EXPECT_EQ(names(), (std::vector<std::string>{"x.wad"}));
    EXPECT_TRUE(state().ownFolder);
}

TEST_F(BrowserBridgeTest, TheFilterLetsThroughNamesThatHoldItInAnyCase) {
    loaded();

    BrowserBridge::setFilter("B.W");

    EXPECT_EQ(state().filter, "B.W");
    EXPECT_EQ(state().shown, (std::vector<int>{3}));

    BrowserBridge::setFilter("");

    EXPECT_EQ(state().shown, (std::vector<int>{0, 2, 3}));
}

TEST_F(BrowserBridgeTest, TheFilterOpensTheFoldersAboveAMatchAndTheFoldsComeBack) {
    loaded();
    model::write(folder() / "maps" / "sub" / "deep" / "e.pk3");
    browser().setOpen(true);

    ASSERT_EQ(names(), (std::vector<std::string>{"sub/", "deep/", "e.pk3", "c.pk3", "a.wad*", "b.wad"}));
    EXPECT_EQ(state().shown, (std::vector<int>{0, 4, 5}));

    BrowserBridge::setFilter("pk3");

    EXPECT_EQ(state().shown, (std::vector<int>{0, 1, 2, 3}));

    BrowserBridge::setFilter("");

    EXPECT_EQ(state().shown, (std::vector<int>{0, 4, 5}));
}

TEST_F(BrowserBridgeTest, AFolderHoldingAPickStartsOpenAndCountsThePicksBelow) {
    model::write(folder() / "maps" / "sub" / "deep" / "e.pk3");
    loaded();
    config().activeProfile().files.push_back(
            {.file = map("sub/deep/e.pk3"), .enabled = true, .gone = false, .group = 0});
    browser().setOwnFolder(other());
    browser().setOwnFolder({});

    EXPECT_EQ(names(), (std::vector<std::string>{"sub/*", "deep/*", "e.pk3*", "c.pk3", "a.wad*", "b.wad"}));
    EXPECT_EQ(state().shown, (std::vector<int>{0, 1, 2, 3, 4, 5}));

    browser().applyPicks({map("sub/deep/e.pk3"), map("sub/c.pk3")});

    EXPECT_EQ(names(), (std::vector<std::string>{"sub/**", "deep/*", "e.pk3*", "c.pk3*", "a.wad", "b.wad"}));
}

TEST_F(BrowserBridgeTest, AFoldHidesTheRowsBelowAndOutlivesARescan) {
    loaded();
    BrowserBridge::fold(0, false);

    EXPECT_EQ(state().shown, (std::vector<int>{0, 1, 2, 3}));

    browser().setOpen(true);

    EXPECT_FALSE(state().rows[0].folded);

    BrowserBridge::fold(0, true);

    EXPECT_EQ(state().shown, (std::vector<int>{0, 2, 3}));
}

TEST_F(BrowserBridgeTest, AFolderTypedInSettingsIsListedOnceTheTypingPauses) {
    loaded();

    bridge().settings().setFileBrowserFolder(other());

    EXPECT_EQ(cfg().fileBrowserFolder, other());
    EXPECT_EQ(names().size(), 4U);

    settle();

    EXPECT_EQ(names(), (std::vector<std::string>{"x.wad"}));
}

}
