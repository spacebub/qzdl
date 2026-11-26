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
#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "ttk/system/Json.h"

// A group a member names by id. Members with no group, or one the list lacks, are ungrouped.
struct Group {
    int id{0};
    std::string name;
    bool collapsed{false};

    friend bool operator==(const Group &, const Group &) = default;
};

namespace Groups {

template <typename Item>
concept Member = requires(Item item) {
    { item.group } -> std::convertible_to<int>;
};

// The id of the group made.
int add(std::vector<Group> &groups, std::string name);

// Null when no group has the id.
[[nodiscard]] Group *find(std::vector<Group> &groups, int id);

// Swaps the group with its neighbour, the one above for -1 and below for 1. Nothing moves past an end.
void move(std::vector<Group> &groups, int id, int by);

void read(yyjson_val *arr, std::vector<Group> &groups);
[[nodiscard]] yyjson_mut_val *write(const ttk::Json::Builder &out, const std::vector<Group> &groups);

// The members of the group become ungrouped.
template <Member Item>
void remove(std::vector<Group> &groups, std::vector<Item> &members, const int id) {
    std::erase_if(groups, [id](const Group &group) { return group.id == id; });

    for (Item &member : members) {
        if (member.group == id) {
            member.group = 0;
        }
    }
}

// Puts members[from] in the group, just before members[before] as the group shows it. It moves the shortest way
// there, or not at all. A before of -1 is the end of the group.
template <Member Item>
void drop(std::vector<Item> &members, const int from, const int group, const int before) {
    const int end = before < 0 ? static_cast<int>(members.size()) : before;
    int ahead = -1;

    members[static_cast<std::size_t>(from)].group = group;

    for (int at = 0; at < end; ++at) {
        if (at != from && members[static_cast<std::size_t>(at)].group == group) {
            ahead = at;
        }
    }

    const auto first = members.begin();

    if (from < ahead) {
        std::rotate(first + from, first + from + 1, first + ahead + 1);
    } else if (from >= end) {
        std::rotate(first + end, first + from, first + from + 1);
    }
}

// A run of the order: one group, or the ungrouped rest as group 0.
struct Section {
    int group{0};
    int first{0};
    int count{0};
};

struct Layout {
    std::vector<int> order;
    std::vector<Section> sections;
};

// The groups in list order, then the ungrouped. Members keep their order inside a section. The order is unchanged
// and there are no sections when grouping is off or the list has no groups.
template <Member Item>
void arrange(const std::vector<Item> &members, const std::vector<Group> &groups, const bool grouped, Layout &into) {
    into.order.resize(members.size());
    into.sections.clear();

    if (!grouped || groups.empty()) {
        std::iota(into.order.begin(), into.order.end(), 0);

        return;
    }

    const auto sectionOf = [&groups, &into](const Item &member) -> Section & {
        const auto found = std::ranges::find(groups, static_cast<int>(member.group), &Group::id);

        return into.sections[static_cast<std::size_t>(found - groups.begin())];
    };

    for (const Group &group : groups) {
        into.sections.push_back({.group = group.id, .first = 0, .count = 0});
    }

    into.sections.push_back({.group = 0, .first = 0, .count = 0});

    for (const Item &member : members) {
        ++sectionOf(member).count;
    }

    int first = 0;

    for (Section &section : into.sections) {
        section.first = std::exchange(first, first + section.count);
        section.count = 0;
    }

    for (std::size_t at = 0; at < members.size(); ++at) {
        Section &section = sectionOf(members[at]);

        into.order[static_cast<std::size_t>(section.first + section.count++)] = static_cast<int>(at);
    }
}

}
