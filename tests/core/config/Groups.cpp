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

#include <cstddef>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "core/config/Groups.h"

namespace {

struct Card {
    char name;
    int group;
};

// One letter per member, its group as a digit after it: "a1b1c0".
std::vector<Card> cards(const std::string &spelled) {
    std::vector<Card> out;

    for (std::size_t at = 0; at + 1 < spelled.size(); at += 2) {
        out.push_back({.name = spelled[at], .group = spelled[at + 1] - '0'});
    }

    return out;
}

std::string spelled(const std::vector<Card> &members) {
    std::string out;

    for (const Card &card : members) {
        out += card.name;
        out += static_cast<char>('0' + card.group);
    }

    return out;
}

std::vector<Group> two() {
    return {{.id = 1, .name = "Maps", .collapsed = false}, {.id = 2, .name = "Mods", .collapsed = false}};
}

}

TEST(Groups, AddingTakesTheHighestIdPlusOne) {
    std::vector<Group> groups;

    EXPECT_EQ(Groups::add(groups, "First"), 1);

    groups.front().id = 7;

    EXPECT_EQ(Groups::add(groups, "Second"), 8);
    ASSERT_EQ(groups.size(), 2U);
    EXPECT_EQ(groups.back().name, "Second");
    EXPECT_FALSE(groups.back().collapsed);
}

TEST(Groups, RemovingLeavesItsMembersUngroupedInPlace) {
    std::vector<Group> groups = two();
    std::vector<Card> members = cards("a1b2c1d0");

    Groups::remove(groups, members, 1);

    ASSERT_EQ(groups.size(), 1U);
    EXPECT_EQ(groups.front().id, 2);
    EXPECT_EQ(spelled(members), "a0b2c0d0");
}

TEST(Groups, FindingAnUnknownIdGivesNull) {
    std::vector<Group> groups = two();

    ASSERT_NE(Groups::find(groups, 2), nullptr);
    EXPECT_EQ(Groups::find(groups, 2)->name, "Mods");
    EXPECT_EQ(Groups::find(groups, 3), nullptr);
}

TEST(Groups, MovingSwapsWithTheNeighbourAndStopsAtTheEnds) {
    std::vector<Group> groups = two();

    Groups::move(groups, 2, -1);
    EXPECT_EQ(groups.front().id, 2);

    Groups::move(groups, 2, -1);
    EXPECT_EQ(groups.front().id, 2);

    Groups::move(groups, 1, 1);
    EXPECT_EQ(groups.back().id, 1);

    Groups::move(groups, 9, 1);
    EXPECT_EQ(groups.front().id, 2);
}

TEST(Groups, ADropMovesTheShortestWayOrNotAtAll) {
    struct Case {
        const char *what;
        const char *before;
        int from;
        int group;
        int to;
        const char *after;
    };

    const std::vector<Case> table = {
            {"already between", "a1b0c1d0", 1, 1, 2, "a1b1c1d0"},
            {"earlier", "a0b1c1d0", 0, 1, -1, "b1c1a1d0"},
            {"later", "a1b1c0d0", 3, 1, 2, "a1b1d1c0"},
            {"into the middle from above", "a0b1c1d1", 0, 1, 2, "b1a1c1d1"},
            {"into the middle from below", "a1b1c1d0", 3, 1, 1, "a1d1b1c1"},
            {"top of a group", "a1b1c0", 2, 1, 0, "c1a1b1"},
            {"top of a group with nothing of it ahead", "a0b0c1d1", 0, 1, 2, "a1b0c1d1"},
            {"bottom of a group", "a1b1c0", 0, 1, -1, "b1a1c0"},
            {"bottom from beyond it", "a1b1c0d2", 3, 1, -1, "a1b1c0d1"},
            {"before itself", "a1b0c1", 1, 1, 1, "a1b1c1"},
            {"empty group", "a1b1c0", 0, 2, -1, "a2b1c0"},
    };

    for (const Case &each : table) {
        SCOPED_TRACE(each.what);

        std::vector<Card> members = cards(each.before);

        Groups::drop(members, each.from, each.group, each.to);

        EXPECT_EQ(spelled(members), each.after);
    }
}

TEST(Groups, ArrangingGivesTheGroupsInOrderThenTheUngrouped) {
    const std::vector<Group> groups = {{.id = 2, .name = "Mods", .collapsed = false},
                                       {.id = 1, .name = "Maps", .collapsed = true}};
    Groups::Layout layout;

    Groups::arrange(cards("a1b0c2d1e2"), groups, true, layout);

    EXPECT_EQ(layout.order, (std::vector<int>{2, 4, 0, 3, 1}));
    ASSERT_EQ(layout.sections.size(), 3U);
    EXPECT_EQ(layout.sections[0].group, 2);
    EXPECT_EQ(layout.sections[0].first, 0);
    EXPECT_EQ(layout.sections[0].count, 2);
    EXPECT_EQ(layout.sections[1].group, 1);
    EXPECT_EQ(layout.sections[1].first, 2);
    EXPECT_EQ(layout.sections[1].count, 2);
    EXPECT_EQ(layout.sections[2].group, 0);
    EXPECT_EQ(layout.sections[2].first, 4);
    EXPECT_EQ(layout.sections[2].count, 1);
}

TEST(Groups, AnEmptyGroupKeepsItsSection) {
    Groups::Layout layout;

    Groups::arrange(cards("a1b0"), two(), true, layout);

    ASSERT_EQ(layout.sections.size(), 3U);
    EXPECT_EQ(layout.sections[1].group, 2);
    EXPECT_EQ(layout.sections[1].first, 1);
    EXPECT_EQ(layout.sections[1].count, 0);
}

TEST(Groups, AnUnknownIdIsUngrouped) {
    Groups::Layout layout;

    Groups::arrange(cards("a7b1c0"), two(), true, layout);

    EXPECT_EQ(layout.order, (std::vector<int>{1, 0, 2}));
    ASSERT_EQ(layout.sections.size(), 3U);
    EXPECT_EQ(layout.sections[2].group, 0);
    EXPECT_EQ(layout.sections[2].count, 2);
}

TEST(Groups, NotGroupedOrWithNoGroupsTheOrderStaysAndNoSections) {
    Groups::Layout layout;

    Groups::arrange(cards("a2b1c0"), two(), false, layout);

    EXPECT_EQ(layout.order, (std::vector<int>{0, 1, 2}));
    EXPECT_TRUE(layout.sections.empty());

    Groups::arrange(cards("a2b1"), {}, true, layout);

    EXPECT_EQ(layout.order, (std::vector<int>{0, 1}));
    EXPECT_TRUE(layout.sections.empty());
}
