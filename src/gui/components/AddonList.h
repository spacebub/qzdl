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

#include <cstddef>

#include "gui/components/Reach.h"
#include "gui/draw/Anim.h"
#include "gui/toolkit/layout/Scroll.h"

namespace components {

// The add-on list: a row per file, reordered by its grip.
class AddonList : public toolkit::Scroll {
public:
    explicit AddonList(Reach *reach);

    [[nodiscard]] static double rowHeight();

    [[nodiscard]] toolkit::Cursor cursorAt(double x, double y) const override;

    void arrange(Typeface &type) override;

    void paint(const toolkit::Painter &painter) override;

    void hover(const toolkit::Pointer &at) override;
    void leave() override;

    bool press(const toolkit::Pointer &at) override;
    void drag(const toolkit::Pointer &at) override;
    void release(const toolkit::Pointer &at) override;

    bool advance(double now) override;

private:
    // The one place the list changes.
    void land();

    [[nodiscard]] int rowAt(double y) const;

    // Rows the carried one passed close up behind it.
    [[nodiscard]] double shiftOf(size_t index, double step) const;

    Reach *_reach;

    int _over = -1;
    int _overGrip = -1;
    bool _overShut = false;

    int _carrying = -1;
    int _target = -1;
    bool _dragging = false;

    double _grabY = 0.0;
    double _landing = 0.0;

    Anim::Tween _carryY;
};

}
