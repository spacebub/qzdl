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
#include "gui/toolkit/Root.h"
#include "gui/toolkit/controls/GlyphButton.h"

namespace toolkit {

GlyphButton::GlyphButton(Glyphs::Glyph glyph, std::function<void()> clicked)
    : _glyph(glyph), _clicked(std::move(clicked)) {
    _takesPointer = true;
    cursor = Cursor::Pointer;
}

GlyphButton *GlyphButton::glyph(Glyphs::Glyph glyph) {
    if (_glyph != glyph) {
        _glyph = glyph;

        invalidate();
    }

    return this;
}

GlyphButton *GlyphButton::size(const double value) {
    _size = value;

    return this;
}

GlyphButton *GlyphButton::tone(const BLRgba32 rest, const BLRgba32 lit) {
    _rest = rest;
    _hot = lit;
    _toneDark = Theme::dark();

    return this;
}

GlyphButton *GlyphButton::outlined(const bool value) {
    _outlined = value;

    return this;
}

GlyphButton *GlyphButton::turn(const double degrees) {
    _turn = degrees;

    return this;
}

GlyphButton *GlyphButton::tooltip(std::string text) {
    hint = std::move(text);

    return this;
}

double GlyphButton::naturalWidth(Typeface & /*type*/) {
    return fixedWidth >= 0.0 ? fixedWidth : _size;
}

double GlyphButton::naturalHeight(Typeface & /*type*/, double /*width*/) {
    return fixedHeight >= 0.0 ? fixedHeight : _size;
}

void GlyphButton::paint(const Painter &painter) {
    const Theme::Palette &palette = Theme::of();
    const double lit = _lit.value();

    const BLRect body{_box.x + ((_box.w - _size) / 2.0), _box.y + ((_box.h - _size) / 2.0), _size,
                      _size};

    if (_outlined) {
        painter.round(body, Theme::radiusSmall, palette.raised);
    }

    if (lit > 0.0) {
        painter.round(body, Theme::radiusSmall, Theme::alpha(palette.hover, lit));
    }

    if (_outlined) {
        painter.outline(body, Theme::radiusSmall, 1.0,
                        Theme::mix(palette.borderStrong, Theme::restated(_hot, _toneDark), lit));
    }

    constexpr float weight = 1.2F;
    const double side = Glyphs::span(weight);
    const BLRgba32 ink = Theme::mix(Theme::restated(_rest, _toneDark),
                                    Theme::restated(_hot, _toneDark), lit);

    Glyphs::draw(painter.context(), _glyph,
                 BLPoint{body.x + ((body.w - side) / 2.0), body.y + ((body.h - side) / 2.0)}, weight,
                 enabled() ? ink : Theme::alpha(ink, 0.4), static_cast<float>(_turn));
}

bool GlyphButton::press(const Pointer & /*at*/) {
    return enabled();
}

void GlyphButton::release(const Pointer &at) {
    if (holds(at.x, at.y) && enabled() && _clicked) {
        _clicked();
    }
}

void GlyphButton::enter() {
    Widget::enter();

    _lit.toward(1.0F, now(), 0.1, Anim::Curve::CubicOut);
    animate();
}

void GlyphButton::leave() {
    Widget::leave();

    _lit.toward(0.0F, now(), 0.1, Anim::Curve::CubicOut);
    animate();
}

bool GlyphButton::advance(const double now) {
    _lit.advance(now);

    invalidate();

    return _lit.live();
}

}
