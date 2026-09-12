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
#include <utility>

#include "gui/toolkit/Root.h"
#include "gui/toolkit/controls/TextView.h"
#include "gui/util/Clipboard.h"

namespace toolkit {

TextView::TextView() {
    _takesPointer = true;
    cursor = Cursor::Text;
}

void TextView::setRows(std::vector<std::string> rows) {
    if (_rows == rows && !_wrapped) {
        return;
    }

    _rows = std::move(rows);
    _spans.clear();
    _run.clear();
    _wrapped = false;
    _folded = -1.0;

    clamp();
    invalidate();
}

void TextView::setRun(std::string run) {
    if (_wrapped && _run == run) {
        return;
    }

    _run = std::move(run);
    _wrapped = true;
    _folded = -1.0;

    clearSelection();
    invalidate();
}

TextView *TextView::face(const int weight, const float size) {
    _weight = weight;
    _size = size;
    _folded = -1.0;

    return this;
}

TextView *TextView::ink(std::function<BLRgba32(size_t)> pick) {
    _ink = std::move(pick);

    return this;
}

const BLFont &TextView::font(Typeface &type) const {
    return type.at(_weight, _size);
}

void TextView::refold(Typeface &type, const double width) {
    if (!_wrapped || (_folded == width && !_rows.empty())) {
        return;
    }

    _rows.clear();
    _spans.clear();

    for (const Fold &line : foldSpans(type, font(type), _run, width)) {
        _rows.push_back(line.text);
        _spans.emplace_back(line.from, line.to);
    }

    _folded = width;
}

void TextView::selectAll() {
    _anchor = Spot{};
    _caret = _rows.empty()
        ? Spot{}
        : Spot{.row = _rows.size() - 1, .at = _rows.back().size()};

    invalidate();
}

void TextView::clearSelection() {
    if (_anchor == _caret) {
        return;
    }

    _anchor = Spot{};
    _caret = Spot{};

    invalidate();
}

void TextView::clamp() {
    if (_rows.empty()) {
        _anchor = Spot{};
        _caret = Spot{};

        return;
    }

    const auto hold = [this](Spot &spot) {
        spot.row = std::min(spot.row, _rows.size() - 1);
        spot.at = std::min(spot.at, _rows[spot.row].size());
    };

    hold(_anchor);
    hold(_caret);
}

void TextView::span(Spot &from, Spot &to) const {
    from = std::min(_anchor, _caret);
    to = std::max(_anchor, _caret);
}

std::string TextView::selection() const {
    Spot from;
    Spot to;

    span(from, to);

    if (from == to || _rows.empty()) {
        return {};
    }

    // A folded run reads back off the run itself, so what comes out is what went in
    // rather than the lines it was broken into.
    if (_wrapped && _spans.size() == _rows.size()) {
        const auto offsetOf = [this](const Spot &spot) {
            const auto &[begins, ends] = _spans[std::min(spot.row, _spans.size() - 1)];

            return std::min(begins + spot.at, ends);
        };

        const size_t begins = offsetOf(from);
        const size_t ends = offsetOf(to);

        return ends > begins ? _run.substr(begins, ends - begins) : std::string();
    }

    std::string out;

    for (size_t row = from.row; row <= to.row && row < _rows.size(); ++row) {
        const std::string &text = _rows[row];
        const size_t begins = row == from.row ? std::min(from.at, text.size()) : 0;
        const size_t ends = row == to.row ? std::min(to.at, text.size()) : text.size();

        if (row != from.row) {
            out += '\n';
        }

        if (ends > begins) {
            out += text.substr(begins, ends - begins);
        }
    }

    return out;
}

double TextView::naturalHeight(Typeface &type, const double width) {
    refold(type, width);

    return static_cast<double>(_rows.size()) * type.lineHeight(font(type));
}

void TextView::arrange(Typeface &type) {
    refold(type, _box.w);
}

TextView::Spot TextView::spotAt(Typeface &type, const double x, const double y) const {
    if (_rows.empty()) {
        return {};
    }

    const double step = type.lineHeight(font(type));
    const auto which = static_cast<long long>((y - _box.y) / std::max(1.0, step));
    const size_t row = static_cast<size_t>(
        std::clamp(which, 0LL, static_cast<long long>(_rows.size()) - 1));

    const std::string &text = _rows[row];
    const double wanted = x - _box.x;

    size_t best = 0;
    double closest = 1e9;

    for (size_t at = 0; at <= text.size();) {
        const double gap = std::abs(type.width(font(type), std::string_view(text).substr(0, at))
                                    - wanted);

        if (gap < closest) {
            closest = gap;
            best = at;
        }

        if (at == text.size()) {
            break;
        }

        // Stepped by character, so a spot never lands inside one.
        const auto lead = static_cast<unsigned char>(text[at]);

        at += lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
        at = std::min(at, text.size());
    }

    return Spot{.row = row, .at = best};
}

void TextView::moveTo(const Spot &where, const bool selecting) {
    if (_caret == where && (selecting || _anchor == where)) {
        return;
    }

    _caret = where;

    if (!selecting) {
        _anchor = where;
    }

    invalidate();
}

bool TextView::press(const Pointer &at) {
    if (root() == nullptr) {
        return false;
    }

    root()->focus(this);

    moveTo(spotAt(root()->type(), at.x, at.y), at.shift);

    return true;
}

void TextView::drag(const Pointer &at) {
    if (root() != nullptr) {
        moveTo(spotAt(root()->type(), at.x, at.y), true);
    }
}

bool TextView::key(const Key &pressed) {
    if (pressed.ctrl && (pressed.text == "a" || pressed.code == 'a')) {
        selectAll();

        return true;
    }

    if (pressed.ctrl && (pressed.text == "c" || pressed.code == 'c')) {
        if (const std::string taken = selection(); !taken.empty()) {
            Clipboard::write(taken);
        }

        return true;
    }

    return false;
}

void TextView::lostFocus() {
    Widget::lostFocus();

    clearSelection();
}

void TextView::paint(const Painter &painter) {
    if (_rows.empty()) {
        return;
    }

    const BLFont &face = painter.font(_weight, _size);
    const double step = painter.lineHeight(face);
    const BLRgba32 wash = Theme::alpha(Theme::of().accent, 0.3);

    Spot from;
    Spot to;

    span(from, to);

    const bool marked = from != to;

    for (size_t row = 0; row < _rows.size(); ++row) {
        const BLRect where{_box.x, _box.y + (static_cast<double>(row) * step), _box.w, step};

        if (!painter.needed(where)) {
            continue;
        }

        const std::string &text = _rows[row];

        if (marked && row >= from.row && row <= to.row) {
            const size_t begins = row == from.row ? std::min(from.at, text.size()) : 0;
            const size_t ends = row == to.row ? std::min(to.at, text.size()) : text.size();
            const double left = painter.width(face, std::string_view(text).substr(0, begins));

            // A row selected through to its end is washed a little past its last
            // character, so a run of rows reads as one block.
            const double right = ends >= text.size() && row < to.row
                ? _box.w
                : painter.width(face, std::string_view(text).substr(0, ends));

            if (right > left) {
                painter.fill(BLRect{where.x + left, where.y, right - left, step}, wash);
            }
        }

        painter.label(face, where, Align::Start, text,
                      _ink ? _ink(row) : Theme::of().text);
    }
}

}
