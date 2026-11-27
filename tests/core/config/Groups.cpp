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
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "core/config/Groups.h"

namespace {

struct Card {
    char name;
    Slot slot;
};

// One letter per member, then its group and its place as digits: "a10b11c00".
std::vector<Card> cards(const std::string &spelled) {
    std::vector<Card> out;

    for (std::size_t at = 0; at + 2 < spelled.size(); at += 3) {
        out.push_back({.name = spelled[at],
                       .slot = {.group = static_cast<std::uint16_t>(spelled[at + 1] - '0'),
                                .place = static_cast<std::uint16_t>(spelled[at + 2] - '0')}});
    }

    return out;
}

std::string spelled(const std::vector<Card> &members) {
    std::string out;

    for (const Card &card : members) {
        out += card.name;
        out += static_cast<char>('0' + card.slot.group);
        out += static_cast<char>('0' + card.slot.place);
    }

    return out;
}

std::vector<Group> two() {
    return {{.id = 1, .name = "Maps", .collapsed = false}, {.id = 2, .name = "Mods", .collapsed = false}};
}

struct DropCase {
    const char *what;
    const char *before;
    int from;
    std::uint16_t group;
    int place;
    const char *after;
};

void dropEach(const std::vector<DropCase> &table) {
    for (const DropCase &each : table) {
        SCOPED_TRACE(each.what);

        std::vector<Card> members = cards(each.before);

        Groups::drop(members, two(), each.from, each.group, each.place);

        EXPECT_EQ(spelled(members), each.after);
    }
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

TEST(Groups, RemovingPutsItsMembersBackInFlatOrder) {
    std::vector<Group> groups = two();
    std::vector<Card> members = cards("a11b20c10d00");
    Groups::Layout layout;

    Groups::remove(groups, members, 1);
    Groups::arrange(members, groups, true, layout);

    ASSERT_EQ(groups.size(), 1U);
    EXPECT_EQ(groups.front().id, 2);
    EXPECT_EQ(spelled(members), "a00b20c00d00");
    EXPECT_EQ(layout.order, (std::vector<int>{1, 0, 2, 3}));
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

TEST(Groups, ADropIntoAGroupNumbersItAndKeepsTheOrder) {
    dropEach({
            {"into the middle", "a00b10c11d00", 0, 1, 1, "a11b10c12d00"},
            {"to the front", "a10b11c00", 2, 1, 0, "a11b12c10"},
            {"past the end", "a10b00", 1, 1, 5, "a10b11"},
            {"within its group", "a10b11c12", 0, 1, 2, "a12b10c11"},
            {"among ties in flat order", "a10b10c00", 2, 1, 1, "a10b12c11"},
            {"leaving a gap where it was", "a20b21c22d10", 1, 1, 0, "a20b10c22d11"},
            {"into an empty group", "a10b00", 1, 2, 0, "a10b20"},
    });
}

TEST(Groups, ADropIntoUngroupedMovesItInTheOrder) {
    dropEach({
            {"before a card", "a00b10c00d00", 3, 0, 1, "a00b10d00c00"},
            {"behind the last", "a00b10c00d10", 0, 0, 1, "b10c00a00d10"},
            {"out of a group in place", "a11b00c10", 0, 0, 0, "a00b00c10"},
            {"with no other ungrouped", "a10b11", 1, 0, 0, "a10b00"},
            {"before one of an unknown group", "a70b00c10", 2, 0, 0, "c00a70b00"},
    });
}

TEST(Groups, ArrangingGivesTheGroupsInOrderThenTheUngrouped) {
    const std::vector<Group> groups = {{.id = 2, .name = "Mods", .collapsed = false},
                                       {.id = 1, .name = "Maps", .collapsed = true}};
    Groups::Layout layout;

    Groups::arrange(cards("a10b00c20d10e20"), groups, true, layout);

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

TEST(Groups, AGroupSortsByPlaceWithTiesInFlatOrderAndTheUngroupedKeepTheirOrder) {
    Groups::Layout layout;

    Groups::arrange(cards("a12b05c10d03e11f10"), two(), true, layout);

    EXPECT_EQ(layout.order, (std::vector<int>{2, 5, 4, 0, 1, 3}));
}

TEST(Groups, AnEmptyGroupKeepsItsSection) {
    Groups::Layout layout;

    Groups::arrange(cards("a10b00"), two(), true, layout);

    ASSERT_EQ(layout.sections.size(), 3U);
    EXPECT_EQ(layout.sections[1].group, 2);
    EXPECT_EQ(layout.sections[1].first, 1);
    EXPECT_EQ(layout.sections[1].count, 0);
}

TEST(Groups, AnUnknownIdIsUngrouped) {
    Groups::Layout layout;

    Groups::arrange(cards("a70b10c00"), two(), true, layout);

    EXPECT_EQ(layout.order, (std::vector<int>{1, 0, 2}));
    ASSERT_EQ(layout.sections.size(), 3U);
    EXPECT_EQ(layout.sections[2].group, 0);
    EXPECT_EQ(layout.sections[2].count, 2);
}

TEST(Groups, NotGroupedOrWithNoGroupsTheOrderStaysAndNoSections) {
    Groups::Layout layout;

    Groups::arrange(cards("a20b10c00"), two(), false, layout);

    EXPECT_EQ(layout.order, (std::vector<int>{0, 1, 2}));
    EXPECT_TRUE(layout.sections.empty());

    Groups::arrange(cards("a20b10"), {}, true, layout);

    EXPECT_EQ(layout.order, (std::vector<int>{0, 1}));
    EXPECT_TRUE(layout.sections.empty());
}
