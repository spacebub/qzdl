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

#include <vector>

#include <gtest/gtest.h>

#include "Model.h"
#include "core/config/Config.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/GroupsBridge.h"
#include "gui/state/State.h"

namespace {

using List = GroupsBridge::List;

class GroupsBridgeTest : public model::Bridged {
protected:
    GroupsBridge &groups() const { return bridge().groups(); }

    static std::vector<Group> &fileGroups() { return config().activeProfile().fileGroups; }

    static std::vector<int> ids(const std::vector<Group> &list) {
        std::vector<int> out;

        for (const Group &group : list) {
            out.push_back(group.id);
        }

        return out;
    }
};

TEST_F(GroupsBridgeTest, AddingAGroupShowsItEmptyAndOpensTheGroupsView) {
    Config made = model::grouped();

    made.profiles.front().fileGroups.clear();

    for (FileEntry &entry : made.profiles.front().files) {
        entry.group = 0;
    }

    install(made);
    bridge().lists().setAddonView(State::AddonView::LoadOrder);

    groups().add(List::Files, "  Maps ");

    EXPECT_EQ(ids(fileGroups()), (std::vector<int>{1}));
    EXPECT_EQ(fileGroups().front().name, "Maps");
    EXPECT_EQ(State::get().nav.addons, State::AddonView::Groups);
    EXPECT_EQ(model::lineFiles(), (std::vector<int>{-1, -1, 0, 1, 2, 3}));
}

TEST_F(GroupsBridgeTest, AddingABlankNameMakesNoGroup) {
    install(model::grouped());

    groups().add(List::Files, "  ");

    EXPECT_EQ(fileGroups().size(), 2U);
}

TEST_F(GroupsBridgeTest, AnAddedGroupIsSaved) {
    install(model::grouped());

    groups().add(List::Files, "Music");
    settle();

    EXPECT_EQ(saved().activeProfile().fileGroups.back().id, 3);

    EXPECT_EQ(saved().activeProfile().fileGroups.back().name, "Music");
}

TEST_F(GroupsBridgeTest, RenamingAGroupMovesTheFilesRevision) {
    install(model::grouped());

    const int before = cfg().filesRev;

    groups().rename(List::Files, 1, "Levels");

    EXPECT_EQ(cfg().fileSections[0].name, "Levels");
    EXPECT_EQ(cfg().files[1].group, "Levels");
    EXPECT_GT(cfg().filesRev, before);
}

TEST_F(GroupsBridgeTest, ABlankRenameIsIgnored) {
    install(model::grouped());

    groups().rename(List::Files, 1, " ");

    EXPECT_EQ(fileGroups().front().name, "Maps");
}

TEST_F(GroupsBridgeTest, MovingAGroupChangesTheLinesAndNotTheFiles) {
    install(model::grouped());

    groups().move(List::Files, 2, -1);

    EXPECT_EQ(ids(fileGroups()), (std::vector<int>{2, 1}));
    EXPECT_EQ(model::lineFiles(), (std::vector<int>{-1, 0, -1, 1, 3, -1, 2}));
    EXPECT_EQ(cfg().files[0].name, "one.wad");

    groups().move(List::Files, 2, 1);

    EXPECT_EQ(ids(fileGroups()), (std::vector<int>{1, 2}));
}

TEST_F(GroupsBridgeTest, RemovingAGroupLeavesItsFilesUngrouped) {
    install(model::grouped());

    groups().remove(List::Files, 1);

    std::vector<int> groupOfFile;

    for (const FileEntry &entry : config().activeProfile().files) {
        groupOfFile.push_back(entry.group);
    }

    EXPECT_EQ(ids(fileGroups()), (std::vector<int>{2}));
    EXPECT_EQ(groupOfFile, (std::vector<int>{2, 0, 0, 0}));
    EXPECT_EQ(model::lineFiles(), (std::vector<int>{-1, 0, -1, 1, 2, 3}));
}

TEST_F(GroupsBridgeTest, FoldingAGroupMovesTheFilesRevisionAndIsSaved) {
    install(model::grouped());

    const int before = cfg().filesRev;

    groups().fold(List::Files, 2, true);

    EXPECT_EQ(model::lineFiles(), (std::vector<int>{-1, 1, 3, -1, -1, 2}));
    EXPECT_GT(cfg().filesRev, before);

    settle();

    EXPECT_TRUE(saved().activeProfile().fileGroups[1].collapsed);

    groups().fold(List::Files, 2, false);

    EXPECT_EQ(model::lineFiles(), (std::vector<int>{-1, 1, 3, -1, 0, -1, 2}));
}

TEST_F(GroupsBridgeTest, TheGamesHaveGroupsOfTheirOwn) {
    install(model::world(1));

    groups().add(List::Games, "Doom");
    groups().add(List::Games, "Heretic");

    const int doom = config().gameGroups.front().id;
    const int heretic = config().gameGroups.back().id;

    config().iwads.front().group = doom;
    groups().rename(List::Games, doom, "id");
    groups().move(List::Games, heretic, -1);
    groups().fold(List::Games, doom, true);

    EXPECT_EQ(ids(config().gameGroups), (std::vector<int>{heretic, doom}));
    EXPECT_EQ(config().gameGroups[1].name, "id");
    EXPECT_TRUE(config().gameGroups[1].collapsed);
    EXPECT_TRUE(fileGroups().empty());

    groups().remove(List::Games, doom);

    EXPECT_EQ(ids(config().gameGroups), (std::vector<int>{heretic}));
    EXPECT_EQ(config().iwads.front().group, 0);
    EXPECT_EQ(config().iwads.size(), 2U);
}

TEST_F(GroupsBridgeTest, AProfileGroupIsSaved) {
    install(model::world(2));

    groups().add(List::Profiles, "Megawads");

    const int id = config().profileGroups.back().id;

    config().profiles.back().group = id;
    groups().rename(List::Profiles, id, "Wads");
    settle();

    EXPECT_EQ(saved().profileGroups.front().name, "Wads");
    EXPECT_TRUE(config().gameGroups.empty());

    groups().remove(List::Profiles, id);

    EXPECT_TRUE(config().profileGroups.empty());
    EXPECT_EQ(config().profiles.back().group, 0);
}

}
