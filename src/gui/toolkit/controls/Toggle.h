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
#include <string>

#include "gui/draw/Anim.h"
#include "gui/toolkit/Widget.h"

namespace toolkit {

// A switch: the track, the knob, and an optional label beside them.
class Toggle : public Widget {
public:
    Toggle(std::string text, std::function<void(bool)> toggled);

    bool checked = false;

    void setChecked(bool value);

    void setText(std::string text);

    double naturalWidth(Typeface &type) override;
    double naturalHeight(Typeface &type, double width) override;

    void arrange(Typeface &type) override;

    void paint(const Painter &painter) override;

    bool press(const Pointer &at) override;
    void release(const Pointer &where) override;
    void enter() override;
    void leave() override;

    [[nodiscard]] bool takesFocus() const override { return enabled(); }
    bool key(const Key &pressed) override;

    bool advance(double now) override;

    // The track and its label, not whatever width the row handed over: a switch
    // alone on a row must not answer a press at the far end of it.
    Widget *at(double x, double y) override;

private:
    std::string _text;
    std::function<void(bool)> _toggled;

    // The track and the label together, which is all the hit test answers to.
    double reach(Typeface &type) const;

    double _reach = 0.0;

    Anim::Tween _on;
};

}
