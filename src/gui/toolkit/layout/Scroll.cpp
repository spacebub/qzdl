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
#include <cmath>

#include "gui/draw/Theme.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/layout/Scroll.h"

namespace {

constexpr double STEP = 52.0;
constexpr double SHORTEST = 28.0;
constexpr double GLIDE = 0.045;

}

namespace toolkit {

Scroll::Scroll() {
    // Empty room inside a scroller still answers the wheel; without this a page
    // of panels would only scroll when the pointer happened to be over a control.
    _takesPointer = true;
}

Widget *Scroll::hold(Ptr child) {
    clear();

    _content = add(std::move(child));

    return _content;
}

bool Scroll::scrollable() const {
    return _reach > _box.h + 1.0;
}

BLRect Scroll::lane() const {
    return BLRect{_box.x + _box.w - Theme::lane + 1.0, _box.y, 8.0, _box.h};
}

BLRect Scroll::thumb() const {
    const BLRect track = lane();
    const double tall = std::max(SHORTEST, track.h * (_box.h / std::max(1.0, _reach)));
    const double travel = std::max(0.0, track.h - tall);
    const double over = std::max(1.0, _reach - _box.h);

    return BLRect{track.x + ((track.w - 5.0) / 2.0),
                  track.y + (travel * std::clamp(_offset / over, 0.0, 1.0)), 5.0, tall};
}

void Scroll::scrollTo(const double offset) {
    _gliding = false;

    settle(offset);
}

void Scroll::settle(const double offset) {
    // Whole pixels: the content lands crisp, and a scroll is a move of what is on
    // screen rather than a repaint of it.
    const double wanted = std::round(std::clamp(offset, 0.0,
                                                std::floor(std::max(0.0, _reach - _box.h))));

    if (wanted == _offset) {
        return;
    }

    const int dy = static_cast<int>(_offset - wanted);

    _offset = wanted;

    if (root() != nullptr) {
        arrange(root()->type());
    }

    if (root() == nullptr || !root()->shift(this, _box, dy)) {
        invalidate();

        return;
    }

    // What came into view, a row at each edge for a box off the pixel grid, and
    // the bar, which did not move with the content.
    const double bare = std::abs(static_cast<double>(dy));

    invalidate(BLRect{_box.x, _box.y, _box.w, dy > 0 ? bare : 1.0});
    invalidate(BLRect{_box.x, _box.y + _box.h - (dy < 0 ? bare : 1.0), _box.w,
                      dy < 0 ? bare : 1.0});
    invalidate(lane());
}

void Scroll::setReach(const double reach) {
    _reach = reach;
    _offset = std::clamp(_offset, 0.0, std::floor(std::max(0.0, _reach - _box.h)));
}

void Scroll::reveal(const BLRect &wanted) {
    if (wanted.y < _box.y) {
        scrollTo(_offset - (_box.y - wanted.y));
    } else if (wanted.y + wanted.h > _box.y + _box.h) {
        scrollTo(_offset + (wanted.y + wanted.h - _box.y - _box.h));
    }
}

void Scroll::arrange(Typeface &type) {
    if (_content == nullptr) {
        return;
    }

    // The bar is drawn over the content, not beside it: taking width would relay
    // the page out the moment the bar appeared, and lay it out again the moment it
    // went. Every content here keeps a margin wider than the bar.
    const double room = std::max({_content->minWidth, 0.0, _box.w});

    _reach = _content->wantedHeight(type, room);

    // A content shorter than the view after a change would leave the offset past
    // the end.
    _offset = std::clamp(_offset, 0.0, std::floor(std::max(0.0, _reach - _box.h)));

    _content->place(BLRect{_box.x, _box.y - _offset, room, std::max(_reach, _box.h)}, type);
}

bool Scroll::clips(BLRect &region) const {
    region = _box;

    return true;
}

void Scroll::paint(const Painter &painter) {
    Widget::paint(painter);

    if (!scrollable()) {
        return;
    }

    const BLRect bar = thumb();

    painter.round(bar, bar.w / 2.0,
                  Theme::alpha(Theme::of().borderStrong, _dragging ? 0.9 : 0.45));
}

bool Scroll::wheel(const double steps, const Pointer & /*at*/) {
    if (!scrollable()) {
        return false;
    }

    if (!_gliding) {
        _gliding = true;
        _goal = _offset;
        _via = _offset;
        _along = _offset;
        _glided = now();
    }

    _goal = std::clamp(_goal - (steps * STEP), 0.0,
                       std::floor(std::max(0.0, _reach - _box.h)));
    animate();

    return true;
}

bool Scroll::advance(const double now) {
    if (!_gliding) {
        return false;
    }

    // Two stages, so the speed builds from nothing and dies away rather than
    // jumping at every turn of the wheel.
    const double step = 1.0 - std::exp(-std::clamp(now - _glided, 0.0, 0.05) / GLIDE);

    _glided = now;
    _via += (_goal - _via) * step;
    _along += (_via - _along) * step;

    if (std::abs(_goal - _along) < 0.5 && std::abs(_goal - _via) < 0.5) {
        _via = _goal;
        _along = _goal;
        _gliding = false;
    }

    settle(_along);

    return _gliding;
}

Widget *Scroll::at(const double x, const double y) {
    if (!visible() || !holds(x, y)) {
        return nullptr;
    }

    // The lane is answered before anything under it.
    if (scrollable() && x >= lane().x) {
        return this;
    }

    return Widget::at(x, y);
}

bool Scroll::press(const Pointer &at) {
    if (!scrollable() || at.x < lane().x) {
        return false;
    }

    const BLRect bar = thumb();

    if (at.y < bar.y || at.y >= bar.y + bar.h) {
        // A press on the track jumps a page.
        scrollTo(_offset + (at.y < bar.y ? -_box.h : _box.h));

        return true;
    }

    _dragging = true;
    _grabbed = bar.y;
    _from = at.y;

    invalidate();

    return true;
}

void Scroll::release(const Pointer & /*at*/) {
    if (_dragging) {
        _dragging = false;

        invalidate();
    }
}

void Scroll::drag(const Pointer &at) {
    if (!_dragging) {
        return;
    }

    const BLRect track = lane();
    const double tall = thumb().h;
    const double travel = std::max(1.0, track.h - tall);
    const double over = std::max(0.0, _reach - _box.h);

    scrollTo(std::clamp((_grabbed + at.y - _from - track.y) / travel, 0.0, 1.0) * over);
}

}
