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

#include "core/config/Config.h"
#include "core/config/Session.h"
#include "gui/model/Notifier.h"
#include "gui/model/State.h"

class ConfigBridge;

// One slice of the interface's state: the calls a view makes and the pushes that
// answer them.
class Bridge {
public:
    Bridge(Notifier *notifier, ConfigBridge *hub) : _notifier(notifier), _hub(hub) {}

protected:
    static State::Cfg &cfg() {
        return State::get().cfg;
    }

    static Config &config() {
        return Session::get().config();
    }

    static Profile &active() {
        return config().activeProfile();
    }

    template <typename Item>
    static void moveTo(std::vector<Item> &list, const int from, const int to) {
        if (from == to || from < 0 || std::cmp_greater_equal(from, list.size())
            || to < 0 || std::cmp_greater_equal(to, list.size())) {
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

    Notifier *_notifier;
    ConfigBridge *_hub;
};
