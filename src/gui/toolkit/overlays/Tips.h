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

#include "gui/draw/Anim.h"
#include "gui/toolkit/Widget.h"

namespace toolkit {

// One tooltip for the window, armed by whatever the pointer rests on.
class Tips : public Widget {
public:
    // Called every frame with what the pointer is over and where the pointer is;
    // an empty text takes the tip down.
    void point(const std::string &text, const BLRect &over, double x, double y, double now);

    void paint(const Painter &painter) override;

    bool advance(double now) override;

    // Nothing under a tooltip is ever pressed through it.
    Widget *at(double /*x*/, double /*y*/) override { return nullptr; }

private:
    // Where the words would go, given what they are and where the pointer is.
    [[nodiscard]] BLRect measure(const std::string &said) const;

    // Puts `_pending` up, damaging what it leaves and what it takes.
    void raise();

    static constexpr double DELAY = 0.45;
    static constexpr double ROOM = 320.0;

    std::string _pending;
    std::string _shown;

    BLRect _over{};
    double _x = 0.0;
    double _y = 0.0;

    double _armed = 0.0;
    bool _up = false;

    BLRect _frame{};

    Anim::Tween _fade;
};

}
