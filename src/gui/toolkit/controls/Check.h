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

#include <functional>

#include "gui/draw/Anim.h"
#include "gui/toolkit/Widget.h"

namespace toolkit {

class Check : public Widget {
public:
    explicit Check(std::function<void(bool)> toggled);

    bool checked = false;

    double naturalWidth(Typeface & /*type*/) override { return 18.0; }
    double naturalHeight(Typeface & /*type*/, double /*width*/) override { return 18.0; }

    void paint(const Painter &painter) override;

    bool press(const Pointer &at) override;
    void release(const Pointer &at) override;
    void enter() override;
    void leave() override;

    bool advance(double now) override;

private:
    std::function<void(bool)> _toggled;

    Anim::Tween _on;
};

}
