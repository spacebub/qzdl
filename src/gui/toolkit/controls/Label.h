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

#include "gui/draw/Theme.h"
#include "gui/toolkit/Widget.h"

namespace toolkit {

// Plain type, laid out as one line unless `wrap` is set.
class Label : public Widget {
public:
    explicit Label(std::string text = {}) : _text(std::move(text)) {}

    [[nodiscard]] const std::string &text() const { return _text; }

    void setText(std::string text);

    Label *font(int weight, float size);
    Label *tone(BLRgba32 tone);
    Label *place(Align where);

    // Wrapped to the width it is given, and as tall as that takes.
    Label *wrap(bool value = true);

    // Small, tracked and upper case: the caption over a group of controls.
    Label *section();

    // Fixed width face, for paths and command lines.
    Label *mono(bool value = true);

    // Takes the pointer and tints on hover; the tooltip is `hint` as usual.
    Label *onClick(std::function<void()> clicked);

    // Written as ~, and shortened by whole directories to whatever room it gets.
    Label *path(bool value = true);

    double naturalWidth(Typeface &type) override;
    double naturalHeight(Typeface &type, double width) override;

    void paint(const Painter &painter) override;

    void arrange(Typeface &type) override;

    bool press(const Pointer &at) override;
    void release(const Pointer &where) override;
    void enter() override;
    void leave() override;

    // A link answers where its text is, not across the row it was given.
    Widget *at(double x, double y) override;

private:
    // What paint() puts on the screen, which for a path is the shortened form.
    double reach(Typeface &type) const;

    std::string _text;

    std::function<void()> _clicked;

    int _weight = 400;
    float _size = Theme::fontBody;

    BLRgba32 _tone = Theme::of().text;
    bool _toneSet = false;

    // Which shade the tone was taken from, so a label built under one and shown
    // under the other reads as its own slot rather than a frozen colour.
    bool _toneDark = Theme::dark();

    Align _place = Align::Start;

    bool _wrap = false;
    bool _tracked = false;
    bool _mono = false;
    bool _path = false;

    double _reach = 0.0;
};

}
