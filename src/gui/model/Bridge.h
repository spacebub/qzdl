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
#include <utility>
#include <vector>

#include "ttk/notices/Notifier.h"

#include "core/config/Config.h"
#include "core/config/Groups.h"
#include "core/config/Session.h"
#include "gui/state/State.h"

class ConfigBridge;

// The header of a group section. The caller fills in the lines.
inline State::Section sectionOf(const std::vector<Group> &groups, const std::uint16_t id) {
    const Group *group = Groups::find(groups, id);

    return State::Section{
            .group = id,
            .name = group != nullptr ? group->name : "Ungrouped",
            .first = 0,
            .count = 0,
            .loaded = 0,
            .collapsed = group != nullptr && group->collapsed,
    };
}

// One slice of the interface's state: the calls a view makes and the pushes that
// answer them.
class Bridge {
public:
    Bridge(ttk::Notifier *notifier, ConfigBridge *hub) : _notifier(notifier), _hub(hub) {}

protected:
    static State::Cfg &cfg() { return State::get().cfg; }

    static Config &config() { return Session::get().config(); }

    static Profile &active() { return config().activeProfile(); }

    // The sections of a picker. Empty groups are left out, and Ungrouped too when no named group holds a member.
    static std::vector<State::Section> pickerSections(const Groups::Layout &layout, const std::vector<Group> &groups) {
        std::vector<State::Section> sections;

        for (const Groups::Section &run : layout.sections) {
            if (run.count == 0 || (run.group == 0 && sections.empty())) {
                continue;
            }

            State::Section &section = sections.emplace_back(sectionOf(groups, run.group));

            section.first = run.first;
            section.count = run.count;
            section.collapsed = false;
        }

        return sections;
    }

    template <typename Item>
    static void moveTo(std::vector<Item> &list, const int from, const int to) {
        if (from == to || from < 0 || std::cmp_greater_equal(from, list.size()) || to < 0
            || std::cmp_greater_equal(to, list.size())) {
            return;
        }

        const auto first = list.begin();
        const auto at = first + from;
        const auto onto = first + to;

        if (to > from) {
            std::rotate(at, at + 1, onto + 1);
        } else {
            std::rotate(onto, at, at + 1);
        }
    }

    ttk::Notifier *_notifier;
    ConfigBridge *_hub;
};
