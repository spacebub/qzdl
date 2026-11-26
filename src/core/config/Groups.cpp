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

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <string_view>
#include <utility>

#include "core/config/Groups.h"
#include "core/config/NonDefault.h"
#include "core/config/Schema.h"

using namespace ttk;

namespace Groups {

std::uint16_t add(std::vector<Group> &groups, std::string name) {
    const auto id = static_cast<std::uint16_t>(groups.empty() ? 1 : std::ranges::max(groups, {}, &Group::id).id + 1);

    groups.push_back({.id = id, .name = std::move(name), .collapsed = false});

    return id;
}

Group *find(std::vector<Group> &groups, const std::uint16_t id) {
    const auto found = std::ranges::find(groups, id, &Group::id);

    return found == groups.end() ? nullptr : &*found;
}

const Group *find(const std::vector<Group> &groups, const std::uint16_t id) {
    const auto found = std::ranges::find(groups, id, &Group::id);

    return found == groups.end() ? nullptr : &*found;
}

void move(std::vector<Group> &groups, const std::uint16_t id, const int by) {
    const auto at = std::ranges::find(groups, id, &Group::id) - groups.begin();
    const auto to = at + by;

    if (at < std::ssize(groups) && to >= 0 && to < std::ssize(groups)) {
        std::swap(groups[static_cast<std::size_t>(at)], groups[static_cast<std::size_t>(to)]);
    }
}

std::uint16_t clampedTo16(const yyjson_val *val) {
    return static_cast<std::uint16_t>(std::clamp(Json::as_int(val), 0, int{std::numeric_limits<std::uint16_t>::max()}));
}

void read(yyjson_val *arr, std::vector<Group> &groups) {
    groups.reserve(yyjson_arr_size(arr));

    Json::each_item(arr, [&groups](yyjson_val *item) {
        Group &group = groups.emplace_back();

        Json::each_field(item, [&group](const std::string_view key, const yyjson_val *val) {
            if (key == ConfigKey::ID) {
                group.id = clampedTo16(val);
            } else if (key == ConfigKey::NAME) {
                group.name = Json::as_string(val);
            } else if (key == ConfigKey::COLLAPSED) {
                group.collapsed = Json::as_bool(val);
            }
        });
    });
}

yyjson_mut_val *write(const Json::Builder &out, const std::vector<Group> &groups) {
    const NonDefault put(out);
    const Group fresh;
    yyjson_mut_val *arr = out.new_array();

    for (const Group &group : groups) {
        yyjson_mut_val *obj = out.new_object();

        put.field(obj, ConfigKey::ID, group.id, fresh.id);
        put.field(obj, ConfigKey::NAME, group.name, fresh.name);
        put.field(obj, ConfigKey::COLLAPSED, group.collapsed, fresh.collapsed);
        Json::Builder::append_value(arr, obj);
    }

    return arr;
}

}
