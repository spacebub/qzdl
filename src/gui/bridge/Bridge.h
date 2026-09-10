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
#include "main.h"
#include "gui/components/Notifier.h"

class ConfigBridge;

// One slice of the Cfg global: its callbacks and the pushes that answer them.
class Bridge {
public:
    Bridge(const ui::Zdl *window, Notifier *notifier, ConfigBridge *hub)
        : _window(window), _notifier(notifier), _hub(hub) {
    }

protected:
    [[nodiscard]] const ui::Cfg &cfg() const {
        return _window->global<ui::Cfg>();
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

    const ui::Zdl *_window;
    Notifier *_notifier;
    ConfigBridge *_hub;
};
