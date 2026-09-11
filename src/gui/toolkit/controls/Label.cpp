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

#include "gui/draw/Typeface.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/util/Format.h"

namespace {

std::string upper(const std::string &value) {
    std::string out = value;

    for (char &letter : out) {
        if (letter >= 'a' && letter <= 'z') {
            letter = static_cast<char>(letter - 'a' + 'A');
        }
    }

    return out;
}

}

namespace toolkit {

void Label::setText(std::string text) {
    if (_text == text) {
        return;
    }

    _text = std::move(text);

    // Deliberately no relayout: a label changing under a fixed box is the common
    // case, and laying the window out again would repaint all of it per keystroke.
    invalidate();
}

Label *Label::font(const int weight, const float size) {
    _weight = weight;
    _size = size;

    return this;
}

Label *Label::tone(const BLRgba32 tone) {
    _tone = tone;
    _toneDark = Theme::dark();
    _toneSet = true;

    return this;
}

Label *Label::place(const Align where) {
    _place = where;

    return this;
}

Label *Label::wrap(const bool value) {
    _wrap = value;

    return this;
}

Label *Label::section() {
    _weight = 600;
    _size = Theme::fontTiny;
    _tracked = true;
    _tone = Theme::of().faint;
    _toneDark = Theme::dark();
    _toneSet = true;

    return this;
}

Label *Label::mono(const bool value) {
    _mono = value;

    return this;
}

Label *Label::onClick(std::function<void()> clicked) {
    _clicked = std::move(clicked);
    _takesPointer = true;
    cursor = Cursor::Pointer;

    return this;
}

Label *Label::path(const bool value) {
    _path = value;
    _mono = value;

    return this;
}

bool Label::press(const Pointer & /*at*/) {
    return _clicked != nullptr;
}

void Label::release(const Pointer &at) {
    if (_clicked && holds(at.x, at.y)) {
        _clicked();
    }
}

void Label::enter() {
    Widget::enter();
}

void Label::leave() {
    Widget::leave();
}

double Label::naturalWidth(Typeface &type) {
    if (fixedWidth >= 0.0) {
        return fixedWidth;
    }

    const BLFont &face = type.at(Typeface::pick(_weight, _mono), _size);

    return _tracked ? type.widthTracked(face, upper(_text), 0.9F) : type.width(face, _text);
}

double Label::naturalHeight(Typeface &type, const double width) {
    if (fixedHeight >= 0.0) {
        return fixedHeight;
    }

    const BLFont &face = type.at(Typeface::pick(_weight, _mono), _size);

    if (!_wrap || _text.empty()) {
        return type.lineHeight(face);
    }

    return wrapHeight(type, face, _text, width);
}

void Label::paint(const Painter &painter) {
    if (_text.empty()) {
        return;
    }

    const BLFont &face = painter.font(Typeface::pick(_weight, _mono), _size);
    const BLRgba32 ink = _clicked && hovered() ? Theme::of().accent
                       : _toneSet              ? Theme::restated(_tone, _toneDark)
                                               : Theme::of().text;

    if (_path) {
        // A fixed width face, so what fits is a division.
        const double unit = painter.width(face, "M");
        const int room = unit > 0.0 ? std::max(1, static_cast<int>(_box.w / unit)) : 0;

        painter.label(face, _box, _place, Format::fitPath(_text, room), ink);

        return;
    }

    if (_wrap) {
        painter.paragraph(face, _box, _text, ink);

        return;
    }

    if (_tracked) {
        const std::string shown = upper(_text);
        const double taken = painter.type().widthTracked(face, shown, 0.9F);
        const double height = painter.lineHeight(face);

        double x = _box.x;

        if (_place == Align::Centre) {
            x = _box.x + ((_box.w - taken) / 2.0);
        } else if (_place == Align::End) {
            x = _box.x + _box.w - taken;
        }

        painter.tracked(face, BLPoint{x, _box.y + ((_box.h - height) / 2.0)}, shown, ink, 0.9);

        return;
    }

    painter.label(face, _box, _place, _text, ink);
}

}
