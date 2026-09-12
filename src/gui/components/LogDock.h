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

#include <string>
#include <vector>

#include "gui/app/Reach.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/controls/TextView.h"
#include "gui/toolkit/layout/Scroll.h"

namespace components {

// The tabs for runs that printed something, and the output of the
// one being shown.
class LogDock : public toolkit::Widget {
public:
    explicit LogDock(Reach *reach);

    void sync();

    // Zero when nothing is docked, which is what keeps it out of the page's room.
    [[nodiscard]] static double wanted();

    void arrange(Typeface &type) override;

    void paint(const toolkit::Painter &painter) override;

    bool press(const toolkit::Pointer &at) override;
    void release(const toolkit::Pointer &at) override;
    void hover(const toolkit::Pointer &at) override;
    void leave() override;

private:
    struct Tab {
        std::string key;
        std::string label;
        BLRect box{};
        BLRect shut{};
        bool alive = false;
    };

    Reach *_reach;

    std::vector<Tab> _tabs;

    toolkit::Scroll *_scroll = nullptr;
    toolkit::TextView *_output = nullptr;
    toolkit::GlyphButton *_copy = nullptr;
    toolkit::GlyphButton *_fold = nullptr;

    // What the tabs were built from.
    std::string _mark;

    // Which run the rows in the view came from.
    std::string _showing;

    int _over = -1;
    bool _overShut = false;

    // Set while the newest line should be kept in view.
    bool _tailing = true;

    size_t _lines = 0;

    int _rev = -1;
};

}
