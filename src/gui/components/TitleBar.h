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
#include <utility>
#include <vector>

#include "gui/app/Reach.h"
#include "gui/draw/Anim.h"
#include "gui/toolkit/controls/GlyphButton.h"

namespace components {

// The brand, the tabs, and the window controls.
//
// The tabs and the buttons are the only things in the bar the window manager may
// not take a press on, so their boxes are also what the hit test asks about.
class TitleBar : public toolkit::Widget {
public:
    explicit TitleBar(Reach *reach);

    void sync();

    // True where the window manager may take the press: anywhere but a control.
    [[nodiscard]] bool draggable(double x, double y) const;

    void arrange(Typeface &type) override;

    void paint(const toolkit::Painter &painter) override;

    bool press(const toolkit::Pointer &at) override;
    void release(const toolkit::Pointer &at) override;
    void hover(const toolkit::Pointer &at) override;
    void leave() override;

    bool advance(double now) override;

private:
    struct Tab {
        Tab(std::string key, std::string label) : key(std::move(key)), label(std::move(label)) {}

        std::string key;
        std::string label;
        bool badge = false;

        BLRect box{};
        double width = 0.0;

        Anim::Tween lit;
        Anim::Tween on;
    };

    Reach *_reach;

    std::vector<Tab> _tabs;

    toolkit::GlyphButton *_shade = nullptr;
    toolkit::GlyphButton *_minimize = nullptr;
    toolkit::GlyphButton *_maximize = nullptr;
    toolkit::GlyphButton *_close = nullptr;

    BLImage _mark;

    double _brandEnd = 0.0;

    int _over = -1;
};

}
