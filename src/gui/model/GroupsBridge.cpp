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

#include <string>
#include <utility>
#include <vector>

#include "ttk/system/Text.h"

#include "core/config/Groups.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/GroupsBridge.h"

using namespace ttk;

std::vector<Group> &GroupsBridge::groups(const List list) {
    if (list == List::Profiles) {
        return config().profileGroups;
    }

    return list == List::Games ? config().gameGroups : active().fileGroups;
}

void GroupsBridge::changed(const List list) const {
    _hub->scheduleSave();

    if (list == List::Profiles) {
        _hub->profile().pushCards();
    } else {
        _hub->lists().push();
    }
}

void GroupsBridge::add(const List list, const std::string &name) const {
    std::string named = Text::trim(name);

    if (named.empty()) {
        return;
    }

    Groups::add(groups(list), std::move(named));

    // A new group is empty, so the load order would not show it.
    if (list == List::Files) {
        State::get().nav.addons = State::AddonView::Groups;
    }

    changed(list);
}

void GroupsBridge::rename(const List list, const int id, const std::string &name) const {
    Group *group = Groups::find(groups(list), id);
    std::string named = Text::trim(name);

    if (group == nullptr || named.empty()) {
        return;
    }

    group->name = std::move(named);

    changed(list);
}

void GroupsBridge::move(const List list, const int id, const int by) const {
    Groups::move(groups(list), id, by);

    changed(list);
}

void GroupsBridge::remove(const List list, const int id) const {
    if (list == List::Profiles) {
        Groups::remove(config().profileGroups, config().profiles, id);
    } else if (list == List::Games) {
        Groups::remove(config().gameGroups, config().iwads, id);
    } else {
        Groups::remove(active().fileGroups, active().files, id);
    }

    changed(list);
}

void GroupsBridge::fold(const List list, const int id, const bool folded) const {
    Group *group = Groups::find(groups(list), id);

    if (group == nullptr) {
        return;
    }

    group->collapsed = folded;

    changed(list);
}
