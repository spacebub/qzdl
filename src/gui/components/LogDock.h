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

#include "gui/components/Reach.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/controls/TextView.h"
#include "gui/toolkit/layout/Scroll.h"

namespace components {

class LogDock : public toolkit::Widget {
public:
    explicit LogDock(Reach *reach);

    void sync();

    [[nodiscard]] double wanted() const;

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
    };

    Reach *_reach;

    std::vector<Tab> _tabs;

    toolkit::Scroll *_scroll = nullptr;
    toolkit::TextView *_output = nullptr;
    toolkit::GlyphButton *_copy = nullptr;
    toolkit::GlyphButton *_fold = nullptr;

    // What the tabs were built from.
    std::string _mark;

    std::string _open;

    // Which run the rows in the view came from.
    std::string _showing;

    int _over = -1;
    bool _overShut = false;

    // Set while the newest line should be kept in view.
    bool _tailing = true;

    size_t _lines = 0;

    // Which run of lines the rows above came from.
    int _generation = -1;

    int _rev = -1;
};

}
