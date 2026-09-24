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

#include "ttk/draw/Anim.h"
#include "ttk/toolkit/controls/GlyphButton.h"

#include "gui/components/Reach.h"

namespace components {

// The brand, the tabs, and the window controls.
//
// The tabs and the buttons are the only things in the bar the window manager may
// not take a press on, so their boxes are also what the hit test asks about.
class TitleBar : public ttk::Widget {
public:
    explicit TitleBar(Reach *reach);

    void sync();

    // True where the window manager may take the press: anywhere but a control.
    [[nodiscard]] bool draggable(double x, double y) const;

    void arrange(ttk::Typeface &type) override;

    void paint(const ttk::Painter &painter) override;

    bool press(const ttk::Pointer &at) override;
    void release(const ttk::Pointer &at) override;
    void hover(const ttk::Pointer &at) override;
    void leave() override;

    bool advance(double now) override;

private:
    struct Tab {
        Tab(const State::Page key, std::string label) : key(key), label(std::move(label)) {}

        State::Page key;
        std::string label;
        bool badge = false;

        BLRect box{};
        double width = 0.0;

        ttk::Anim::Tween lit;
        ttk::Anim::Tween on;
    };

    Reach *_reach;

    std::vector<Tab> _tabs;

    ttk::GlyphButton *_shade = nullptr;
    ttk::GlyphButton *_minimize = nullptr;
    ttk::GlyphButton *_maximize = nullptr;
    ttk::GlyphButton *_close = nullptr;

    BLImage _mark;

    double _brandEnd = 0.0;

    int _over = -1;
};

}
