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
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ttk/system/Json.h"

// A group a member names by id. Members with no group, or one the list lacks, are ungrouped.
struct Group {
    std::uint16_t id{0};
    std::string name;
    bool collapsed{false};

    friend bool operator==(const Group &, const Group &) = default;
};

// The group of a profile or a game, and its place among the other members there.
struct Slot {
    std::uint16_t group{0};
    std::uint16_t place{0};

    friend bool operator==(const Slot &, const Slot &) = default;
};

namespace Groups {

// Add-on files have a group and no place, so a group shows them in load order.
template <typename Item>
concept Placed = requires(Item item) { item.slot; };

template <typename Item>
[[nodiscard]] auto &groupOf(Item &item) {
    if constexpr (Placed<Item>) {
        return item.slot.group;
    } else {
        return item.group;
    }
}

// The id of the group made.
std::uint16_t add(std::vector<Group> &groups, std::string name);

// Null when no group has the id.
[[nodiscard]] Group *find(std::vector<Group> &groups, std::uint16_t id);
[[nodiscard]] const Group *find(const std::vector<Group> &groups, std::uint16_t id);

// Swaps the group with its neighbour, the one above for -1 and below for 1. Nothing moves past an end.
void move(std::vector<Group> &groups, std::uint16_t id, int by);

[[nodiscard]] std::uint16_t clampedTo16(const yyjson_val *val);

void read(yyjson_val *arr, std::vector<Group> &groups);
[[nodiscard]] yyjson_mut_val *write(const ttk::Json::Builder &out, const std::vector<Group> &groups);

// The members of the group become ungrouped.
template <typename Item>
void remove(std::vector<Group> &groups, std::vector<Item> &members, const std::uint16_t id) {
    std::erase_if(groups, [id](const Group &group) { return group.id == id; });

    for (Item &member : members) {
        if (groupOf(member) == id) {
            groupOf(member) = 0;
        }
    }
}

// A run of the order: one group, or the ungrouped rest as group 0.
struct Section {
    std::uint16_t group{0};
    int first{0};
    int count{0};
};

struct Layout {
    std::vector<int> order;
    std::vector<Section> sections;

    // The section of each group id, filled by arrange.
    std::vector<std::uint16_t> byId;

    [[nodiscard]] std::span<int> run(const Section &section) {
        return std::span(order).subspan(static_cast<std::size_t>(section.first),
                                        static_cast<std::size_t>(section.count));
    }
};

// The groups in list order, then the ungrouped. A group sorts its members by place, and the ungrouped keep their order.
// The order is unchanged and there are no sections when grouping is off or the list has no groups.
template <typename Item>
void arrange(const std::vector<Item> &members, const std::vector<Group> &groups, const bool grouped, Layout &into) {
    into.order.resize(members.size());
    into.sections.clear();

    if (!grouped || groups.empty()) {
        std::ranges::iota(into.order, 0);

        return;
    }

    const auto sectionOf = [&into](const Item &member) -> Section & {
        const std::uint16_t id = groupOf(member);

        return into.sections[id < into.byId.size() ? into.byId[id] : into.sections.size() - 1];
    };

    into.byId.assign(std::ranges::max(groups, {}, &Group::id).id + 1, static_cast<std::uint16_t>(groups.size()));
    into.sections.reserve(groups.size() + 1);

    for (const Group &group : groups) {
        into.byId[group.id] = static_cast<std::uint16_t>(into.sections.size());
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
        const int place = section.first + section.count++;

        into.order[static_cast<std::size_t>(place)] = static_cast<int>(at);
    }

    if constexpr (Placed<Item>) {
        for (const Section &section : std::span(into.sections).first(groups.size())) {
            std::ranges::sort(into.run(section), {}, [&members](const int at) {
                return std::pair(members[static_cast<std::size_t>(at)].slot.place, at);
            });
        }
    }
}

// Puts members[from] at `place` among the other members of the group, as arrange shows them. A named group then
// numbers its places from 0, and the order stays. Group 0 moves the member in the order instead.
template <Placed Item>
void drop(std::vector<Item> &members,
          const std::vector<Group> &groups,
          const int from,
          const std::uint16_t group,
          const int place) {
    Layout layout;

    arrange(members, groups, true, layout);

    const std::span<int> run = layout.run(*std::ranges::find(layout.sections, group, &Section::group));
    std::vector<int> others(run.begin(), run.end());

    std::erase(others, from);

    const auto before = others.begin() + std::min<std::ptrdiff_t>(place, std::ssize(others));

    if (group != 0) {
        members[static_cast<std::size_t>(from)].slot.group = group;
        others.insert(before, from);

        for (std::uint16_t next = 0; const int at : others) {
            members[static_cast<std::size_t>(at)].slot.place = next++;
        }

        return;
    }

    const int to = before != others.end() ? *before : others.empty() ? from : others.back() + 1;
    const auto first = members.begin();

    members[static_cast<std::size_t>(from)].slot = {};

    if (from < to) {
        std::rotate(first + from, first + from + 1, first + to);
    } else {
        std::rotate(first + to, first + from, first + from + 1);
    }
}

}
