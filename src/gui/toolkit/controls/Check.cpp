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

#include "gui/draw/Glyphs.h"
#include "gui/draw/Theme.h"
#include "gui/toolkit/controls/Check.h"

namespace toolkit {

Check::Check(std::function<void(bool)> toggled) : _toggled(std::move(toggled)) {
    _takesPointer = true;
    cursor = Cursor::Pointer;
}

void Check::paint(const Painter &painter) {
    const Theme::Palette &palette = Theme::of();
    const double on = _on.value();

    const BLRect body{_box.x + ((_box.w - 18.0) / 2.0), _box.y + ((_box.h - 18.0) / 2.0), 18.0, 18.0};

    painter.round(body, 5.0, Theme::mix(palette.field, palette.accent, on));
    painter.outline(body, 5.0, 1.0,
                    Theme::mix(hovered() ? palette.borderStrong : palette.border, palette.accent,
                               on));

    if (on <= 0.0) {
        return;
    }

    constexpr float weight = 0.85F;
    const double side = Glyphs::span(weight);

    Glyphs::draw(painter.context(), Glyphs::Glyph::Check,
                 BLPoint{body.x + ((body.w - side) / 2.0), body.y + ((body.h - side) / 2.0)}, weight,
                 Theme::alpha(palette.accentText, on));
}

bool Check::press(const Pointer & /*at*/) {
    return enabled();
}

void Check::release(const Pointer &at) {
    if (holds(at.x, at.y) && enabled() && _toggled) {
        _toggled(!checked);
    }
}

void Check::enter() {
    Widget::enter();
}

void Check::leave() {
    Widget::leave();
}

bool Check::advance(const double now) {
    _on.advance(now);

    invalidate();

    return _on.live();
}

}
