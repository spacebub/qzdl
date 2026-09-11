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

#include <algorithm>

#include "gui/draw/Glyphs.h"
#include "gui/draw/Theme.h"
#include "gui/draw/Typeface.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/controls/Button.h"

namespace {

constexpr double HOVER_SECONDS = 0.11;

}

namespace toolkit {

Button::Button(std::string text, std::function<void()> clicked)
    : _text(std::move(text)), _clicked(std::move(clicked)) {
    _takesPointer = true;
    cursor = Cursor::Pointer;
    _give.set(1.0F);
}

void Button::setText(std::string text) {
    if (_text == text) {
        return;
    }

    _text = std::move(text);

    invalidate();
}

Button *Button::kind(const Kind value) {
    _kind = value;

    return this;
}

Button *Button::glyph(std::string name) {
    _glyph = std::move(name);

    return this;
}

Button *Button::compact(const bool value) {
    _compact = value;

    return this;
}

Button *Button::busy(const bool value) {
    if (_busy == value) {
        return this;
    }

    _busy = value;

    if (_busy) {
        animate();
    }

    invalidate();

    return this;
}

Button *Button::tip(std::string text) {
    hint = std::move(text);

    return this;
}

BLRgba32 Button::ink() const {
    const Theme::Palette &palette = Theme::of();

    switch (_kind) {
        case Kind::Primary:
            return palette.accentText;

        case Kind::Danger:
            return palette.danger;

        case Kind::Ghost:
            return palette.accent;

        default:
            return palette.text;
    }
}

double Button::naturalWidth(Typeface &type) {
    if (fixedWidth >= 0.0) {
        return fixedWidth;
    }

    const double mark = 12.0 * (_compact ? 1.1 : 1.2);
    const BLFont &face = type.at(600, _compact ? Theme::fontSmall : Theme::fontBody);
    const double content = (_glyph.empty() ? 0.0 : mark + 7.0) + type.width(face, _text);

    return _compact ? content + 26.0 : std::max(content + 38.0, Theme::buttonWidth);
}

double Button::naturalHeight(Typeface & /*type*/, double /*width*/) {
    return fixedHeight >= 0.0 ? fixedHeight : (_compact ? Theme::controlSmall : Theme::control);
}

void Button::paint(const Painter &painter) {
    const Theme::Palette &palette = Theme::of();
    const double lit = _lit.value();
    const bool down = pressed();

    // The give is the box drawn a little smaller, which reads as a press.
    const double give = _give.value();
    const BLRect body{_box.x + (_box.w * (1.0 - give) / 2.0), _box.y + (_box.h * (1.0 - give) / 2.0),
                      _box.w * give, _box.h * give};

    BLRgba32 ground{};
    BLRgba32 edge = palette.borderStrong;
    double border = 1.0;

    switch (_kind) {
        case Kind::Primary:
            ground = down ? Theme::darker(palette.accent, 0.15)
                          : Theme::mix(palette.accent, palette.accentHover, lit);
            border = 0.0;

            break;

        case Kind::Ghost:
            ground = Theme::alpha(palette.accentSoft, lit);
            edge = Theme::alpha(palette.accentSoft, 0.0);

            break;

        case Kind::Danger:
            ground = down ? Theme::darker(palette.dangerSoft, 0.08)
                          : Theme::mix(palette.raised, palette.dangerSoft, lit);
            edge = Theme::mix(Theme::alpha(palette.danger, 0.5), palette.danger, lit);

            break;

        default:
            ground = down ? palette.sunken : palette.raised;
            edge = Theme::mix(palette.borderStrong, palette.accent, lit);

            break;
    }

    painter.round(body, Theme::radiusSmall, ground);

    if (_kind == Kind::Default && !down && lit > 0.0) {
        painter.round(body, Theme::radiusSmall, Theme::alpha(palette.hover, lit));
    }

    if (border > 0.0) {
        painter.outline(body, Theme::radiusSmall, border, edge);
    }

    if (_busy) {
        constexpr double dot = 6.0;
        const double span = (dot * 3.0) + (5.0 * 2.0);

        for (int at = 0; at < 3; ++at) {
            const BLRgba32 tone = _kind == Kind::Primary ? palette.accentText : palette.accent;

            painter.circle(BLPoint{_box.x + ((_box.w - span) / 2.0) + (at * (dot + 5.0)) + (dot / 2.0),
                                   _box.y + (_box.h / 2.0)},
                           dot / 2.0, _tick == at ? Theme::alpha(tone, 0.25) : tone);
        }

        return;
    }

    const double mark = 12.0 * (_compact ? 1.1 : 1.2);
    const BLFont &face = painter.font(600, _compact ? Theme::fontSmall : Theme::fontBody);
    const double label = painter.width(face, _text);
    const double content = (_glyph.empty() ? 0.0 : mark + 7.0) + label;

    double x = _box.x + ((_box.w - content) / 2.0);

    const BLRgba32 tint = enabled() ? ink() : Theme::alpha(ink(), 0.45);

    if (!_glyph.empty()) {
        Glyphs::draw(painter.context(), _glyph.c_str(),
                     BLPoint{x, _box.y + ((_box.h - mark) / 2.0)},
                     static_cast<float>(_compact ? 1.1 : 1.2), tint);

        x += mark + 7.0;
    }

    painter.label(face, BLRect{x, _box.y, label + 2.0, _box.h}, Align::Start, _text, tint);
}

bool Button::press(const Pointer & /*at*/) {
    if (!enabled() || _busy) {
        return false;
    }

    _give.run(0.985F, now(), 0.09, Anim::Curve::CubicOut);
    animate();

    return true;
}

void Button::release(const Pointer &at) {
    _give.run(1.0F, now(), 0.09, Anim::Curve::CubicOut);
    animate();

    if (holds(at.x, at.y) && enabled() && !_busy && _clicked) {
        _clicked();
    }
}

void Button::enter() {
    Widget::enter();

    _lit.toward(1.0F, now(), HOVER_SECONDS, Anim::Curve::CubicOut);
    animate();
}

void Button::leave() {
    Widget::leave();

    _lit.toward(0.0F, now(), HOVER_SECONDS, Anim::Curve::CubicOut);
    animate();
}

bool Button::key(const Key &pressed) {
    if (pressed.code != Code::Return && pressed.text != " ") {
        return false;
    }

    if (enabled() && !_busy && _clicked) {
        _clicked();
    }

    return true;
}

bool Button::advance(const double now) {
    _lit.advance(now);
    _give.advance(now);

    if (_busy && now - _ticked > 0.3) {
        _ticked = now;
        _tick = (_tick + 1) % 3;
    }

    invalidate();

    return _lit.live() || _give.live() || _busy;
}

}
