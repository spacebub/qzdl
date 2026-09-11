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

#include "gui/draw/Theme.h"
#include "gui/draw/Typeface.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/controls/TextBox.h"
#include "gui/util/Clipboard.h"

namespace {

constexpr double BLINK = 0.53;

bool isWordChar(const char letter) {
    return (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z')
        || (letter >= '0' && letter <= '9') || letter == '_'
        || static_cast<unsigned char>(letter) >= 0x80;
}

}

namespace toolkit {

// --- TextBox -------------------------------------------------------------------

TextBox::TextBox(std::function<void(const std::string &)> edited) : _edited(std::move(edited)) {
    _takesPointer = true;
    cursor = Cursor::Text;
}

void TextBox::setText(std::string text) {
    if (_text == text) {
        return;
    }

    _text = std::move(text);
    _caret = std::min(_caret, _text.size());
    _anchor = _caret;

    invalidate();
}

TextBox *TextBox::placeholder(std::string text) {
    _placeholder = std::move(text);

    return this;
}

TextBox *TextBox::mono(const bool value) {
    _mono = value;

    return this;
}

TextBox *TextBox::readOnly(const bool value) {
    _readOnly = value;

    return this;
}

void TextBox::selectAll() {
    _anchor = 0;
    _caret = _text.size();

    invalidate();
}

double TextBox::naturalWidth(Typeface & /*type*/) {
    return fixedWidth >= 0.0 ? fixedWidth : 120.0;
}

double TextBox::naturalHeight(Typeface &type, double /*width*/) {
    return fixedHeight >= 0.0 ? fixedHeight : type.lineHeight(type.at(400, Theme::fontBody));
}

void TextBox::span(size_t &from, size_t &to) const {
    from = std::min(_caret, _anchor);
    to = std::max(_caret, _anchor);
}

double TextBox::widthTo(Typeface &type, const size_t offset) const {
    return type.width(type.at(Typeface::pick(400, _mono), Theme::fontBody),
                      std::string_view(_text).substr(0, std::min(offset, _text.size())));
}

size_t TextBox::offsetAt(Typeface &type, const double x) const {
    const double wanted = x - _box.x + _shift;
    size_t best = 0;
    double closest = 1e9;

    for (size_t at = 0; at <= _text.size();) {
        if (const double gap = std::abs(widthTo(type, at) - wanted); gap < closest) {
            closest = gap;
            best = at;
        }

        if (at == _text.size()) {
            break;
        }

        at = after(at);
    }

    return best;
}

size_t TextBox::before(const size_t at) const {
    if (at == 0) {
        return 0;
    }

    size_t back = at - 1;

    while (back > 0 && (static_cast<unsigned char>(_text[back]) & 0xc0) == 0x80) {
        --back;
    }

    return back;
}

size_t TextBox::after(const size_t at) const {
    if (at >= _text.size()) {
        return _text.size();
    }

    const auto lead = static_cast<unsigned char>(_text[at]);
    const size_t wide = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;

    return std::min(at + wide, _text.size());
}

size_t TextBox::wordLeft(size_t at) const {
    while (at > 0 && !isWordChar(_text[at - 1])) {
        at = before(at);
    }

    while (at > 0 && isWordChar(_text[at - 1])) {
        at = before(at);
    }

    return at;
}

size_t TextBox::wordRight(size_t at) const {
    while (at < _text.size() && !isWordChar(_text[at])) {
        at = after(at);
    }

    while (at < _text.size() && isWordChar(_text[at])) {
        at = after(at);
    }

    return at;
}

void TextBox::moveTo(const size_t offset, const bool selecting) {
    _caret = std::min(offset, _text.size());

    if (!selecting) {
        _anchor = _caret;
    }

    _showCaret = true;
    _blinked = now();

    if (root() != nullptr) {
        keepCaret(root()->type());
    }

    invalidate();
}

void TextBox::keepCaret(Typeface &type) {
    const double at = widthTo(type, _caret);
    const double room = std::max(0.0, _box.w - 2.0);

    if (at - _shift < 0.0) {
        _shift = at;
    } else if (at - _shift > room) {
        _shift = at - room;
    }

    const double whole = type.width(type.at(Typeface::pick(400, _mono), Theme::fontBody), _text);

    _shift = std::clamp(_shift, 0.0, std::max(0.0, whole - room));
}

void TextBox::erase(const size_t from, const size_t to) {
    if (_readOnly || from >= to) {
        return;
    }

    _text.erase(from, to - from);
    _caret = from;
    _anchor = from;

    if (_edited) {
        _edited(_text);
    }

    invalidate();
}

void TextBox::insert(const std::string &what) {
    if (_readOnly) {
        return;
    }

    size_t from = 0;
    size_t to = 0;

    span(from, to);

    if (from != to) {
        _text.erase(from, to - from);
        _caret = from;
    }

    _text.insert(_caret, what);
    _caret += what.size();
    _anchor = _caret;

    if (_edited) {
        _edited(_text);
    }

    moveTo(_caret, false);
}

void TextBox::paint(const Painter &painter) {
    const Theme::Palette &palette = Theme::of();
    const BLFont &face = painter.font(Typeface::pick(400, _mono), Theme::fontBody);

    painter.push(_box);

    if (_text.empty()) {
        if (!_placeholder.empty()) {
            painter.label(face, _box, Align::Start, _placeholder, palette.faint);
        }

        // The caret belongs to an empty field too, which is where it matters most.
        if (focused() && _showCaret) {
            painter.fill(BLRect{_box.x, _box.y + 3.0, 1.5, _box.h - 6.0}, palette.accent);
        }

        painter.pop();

        return;
    }

    size_t from = 0;
    size_t to = 0;

    span(from, to);

    const double left = _box.x - _shift;

    if (from != to && focused()) {
        const double start = painter.width(face, std::string_view(_text).substr(0, from));
        const double end = painter.width(face, std::string_view(_text).substr(0, to));

        painter.fill(BLRect{left + start, _box.y + 2.0, end - start, _box.h - 4.0},
                     palette.accent);
    }

    // Drawn in two runs so the selected part takes the accent's ink.
    if (from != to && focused()) {
        const std::string_view whole(_text);

        painter.label(face, BLRect{left, _box.y, painter.width(face, whole.substr(0, from)) + 1.0,
                                   _box.h},
                      Align::Start, whole.substr(0, from), palette.text);

        const double startX = left + painter.width(face, whole.substr(0, from));

        painter.label(face,
                      BLRect{startX, _box.y,
                             painter.width(face, whole.substr(from, to - from)) + 1.0, _box.h},
                      Align::Start, whole.substr(from, to - from), palette.accentText);

        const double endX = left + painter.width(face, whole.substr(0, to));

        painter.label(face, BLRect{endX, _box.y, _box.w + _shift, _box.h}, Align::Start,
                      whole.substr(to), palette.text);
    } else {
        painter.label(face, BLRect{left, _box.y, _box.w + _shift + 4.0, _box.h}, Align::Start,
                      _text, palette.text);
    }

    if (focused() && _showCaret && from == to) {
        const double at = left + painter.width(face, std::string_view(_text).substr(0, _caret));

        painter.fill(BLRect{at, _box.y + 3.0, 1.5, _box.h - 6.0}, palette.accent);
    }

    painter.pop();
}

bool TextBox::press(const Pointer &at) {
    if (!enabled()) {
        return false;
    }

    if (root() != nullptr) {
        root()->focus(this);

        moveTo(offsetAt(root()->type(), at.x), at.shift);
    }

    return true;
}

void TextBox::drag(const Pointer &at) {
    if (root() != nullptr) {
        moveTo(offsetAt(root()->type(), at.x), true);
    }
}

bool TextBox::key(const Key &pressed) {
    size_t from = 0;
    size_t to = 0;

    span(from, to);

    if (pressed.ctrl && (pressed.text == "a" || pressed.code == 'a')) {
        selectAll();

        return true;
    }

    if (pressed.ctrl && (pressed.code == 'c' || pressed.code == 'x')) {
        if (from != to) {
            Clipboard::write(_text.substr(from, to - from));

            if (pressed.code == 'x') {
                erase(from, to);
            }
        }

        return true;
    }

    if (pressed.ctrl && pressed.code == 'v') {
        insert(Clipboard::read());

        return true;
    }

    switch (pressed.code) {
        case Code::Left:
            moveTo(from != to && !pressed.shift ? from
                   : pressed.ctrl ? wordLeft(_caret)
                                  : before(_caret),
                   pressed.shift);

            return true;

        case Code::Right:
            moveTo(from != to && !pressed.shift ? to
                   : pressed.ctrl ? wordRight(_caret)
                                  : after(_caret),
                   pressed.shift);

            return true;

        case Code::Home:
            moveTo(0, pressed.shift);

            return true;

        case Code::End:
            moveTo(_text.size(), pressed.shift);

            return true;

        case Code::Backspace:
            if (from != to) {
                erase(from, to);
            } else if (_caret > 0) {
                erase(pressed.ctrl ? wordLeft(_caret) : before(_caret), _caret);
            }

            moveTo(_caret, false);

            return true;

        case Code::Delete:
            if (from != to) {
                erase(from, to);
            } else if (_caret < _text.size()) {
                erase(_caret, pressed.ctrl ? wordRight(_caret) : after(_caret));
            }

            moveTo(_caret, false);

            return true;

        case Code::Return:
            if (accepted) {
                accepted();
            }

            return true;

        case Code::Escape:
            if (cancelled) {
                cancelled();

                return true;
            }

            return false;

        default:
            break;
    }

    return false;
}

void TextBox::wrote(const std::string &text) {
    insert(text);
}

void TextBox::gainedFocus() {
    Widget::gainedFocus();

    // The border that says a field is live belongs to the Field around it, and a
    // damage of the run alone would never reach it.
    if (parent() != nullptr) {
        parent()->invalidate();
    }

    _showCaret = true;
    _blinked = now();

    animate();
}

void TextBox::lostFocus() {
    Widget::lostFocus();

    if (parent() != nullptr) {
        parent()->invalidate();
    }

    _shift = 0.0;
    _anchor = _caret;
}

bool TextBox::advance(const double now) {
    if (!focused()) {
        return false;
    }

    if (now - _blinked >= BLINK) {
        _blinked = now;
        _showCaret = !_showCaret;

        invalidate();
    }

    return true;
}

}
