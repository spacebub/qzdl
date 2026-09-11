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
#include "gui/draw/Theme.h"
#include "gui/toolkit/Widget.h"

namespace toolkit {

class GlyphButton : public Widget {
public:
    GlyphButton(std::string glyph, std::function<void()> clicked);

    GlyphButton *glyph(std::string name);
    GlyphButton *size(double value);
    GlyphButton *tone(BLRgba32 rest, BLRgba32 lit);
    GlyphButton *outlined(bool value = true);
    GlyphButton *turn(double degrees);
    GlyphButton *tip(std::string text);

    double naturalWidth(Typeface &type) override;
    double naturalHeight(Typeface &type, double width) override;

    void paint(const Painter &painter) override;

    bool press(const Pointer &at) override;
    void release(const Pointer &at) override;
    void enter() override;
    void leave() override;

    bool advance(double now) override;

private:
    std::string _glyph;
    std::function<void()> _clicked;

    double _size = Theme::controlSmall;
    double _turn = 0.0;

    BLRgba32 _rest = Theme::of().muted;
    BLRgba32 _hot = Theme::of().text;
    bool _toneDark = Theme::dark();

    bool _outlined = false;

    Anim::Tween _lit;
};

}
