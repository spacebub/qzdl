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
#include <ranges>

#include "gui/toolkit/Root.h"
#include "gui/toolkit/Widget.h"

namespace toolkit {

Widget *Widget::add(Ptr child) {
    Widget *raw = child.get();

    raw->_parent = this;
    raw->attach(_root);

    _children.push_back(std::move(child));

    return raw;
}

void Widget::clear() {
    Root *root = _root;

    if (root != nullptr) {
        for (const Ptr &child : _children) {
            root->forget(child.get());
        }
    }

    _children.clear();
}

void Widget::erase(const Widget *child) {
    const auto found = std::ranges::find_if(_children,
                                            [child](const Ptr &held) {
                                                return held.get() == child;
                                            });

    if (found == _children.end()) {
        return;
    }

    if (_root != nullptr) {
        _root->forget(found->get());
    }

    _children.erase(found);
}

void Widget::attach(Root *root) {
    _root = root;

    for (const Ptr &child : _children) {
        child->attach(root);
    }
}

void Widget::place(const BLRect &box, Typeface &type) {
    _box = box;

    moved();
    arrange(type);
}

double Widget::naturalWidth(Typeface &type) {
    if (fixedWidth >= 0.0) {
        return fixedWidth;
    }

    double widest = 0.0;

    for (const Ptr &child : _children) {
        if (child->visible()) {
            widest = std::max(widest, child->naturalWidth(type));
        }
    }

    return widest;
}

double Widget::naturalHeight(Typeface &type, const double width) {
    if (fixedHeight >= 0.0) {
        return fixedHeight;
    }

    double tallest = 0.0;

    for (const Ptr &child : _children) {
        if (child->visible()) {
            tallest = std::max(tallest, child->naturalHeight(type, width));
        }
    }

    return tallest;
}

// A plain widget stacks its children on itself; only a layout gives them places of
// their own.
void Widget::arrange(Typeface &type) {
    for (const Ptr &child : _children) {
        child->place(_box, type);
    }
}

void Widget::setVisible(const bool value) {
    if (_visible == value) {
        return;
    }

    invalidate();

    _visible = value;

    if (_root != nullptr) {
        _root->relayout();
    }
}

void Widget::setEnabled(const bool value) {
    if (_enabled == value) {
        return;
    }

    _enabled = value;

    invalidate();
}

bool Widget::focused() const {
    return _root != nullptr && _root->focused() == this;
}

void Widget::paint(const Painter &painter) {
    for (const Ptr &child : _children) {
        if (!child->visible() || !painter.needed(child->drawn())) {
            continue;
        }

        BLRect region{};

        if (child->clips(region)) {
            painter.push(region);
            child->paint(painter);
            painter.pop();
        } else {
            child->paint(painter);
        }
    }
}

bool Widget::clips(BLRect & /*unused*/) const {
    return false;
}

void Widget::invalidate() const {
    invalidate(drawn());
}

void Widget::invalidate(const BLRect &region) const {
    if (_root == nullptr) {
        return;
    }

    BLRect wanted = region;

    // Clipped by every scroller on the way up: a row scrolled out of view has a
    // box, and repainting it would scribble over whatever is there now.
    for (const Widget *above = _parent; above != nullptr; above = above->_parent) {
        BLRect limit{};

        if (!above->clips(limit)) {
            continue;
        }

        const double left = std::max(wanted.x, limit.x);
        const double top = std::max(wanted.y, limit.y);
        const double right = std::min(wanted.x + wanted.w, limit.x + limit.w);
        const double bottom = std::min(wanted.y + wanted.h, limit.y + limit.h);

        if (right <= left || bottom <= top) {
            return;
        }

        wanted = BLRect{left, top, right - left, bottom - top};
    }

    _root->damage(wanted);
}

bool Widget::press(const Pointer & /*unused*/) {
    return false;
}

void Widget::drag(const Pointer & /*unused*/) {}

void Widget::release(const Pointer & /*unused*/) {}

void Widget::enter() {
    _hovered = true;

    invalidate();
}

void Widget::leave() {
    _hovered = false;
    _pressed = false;

    invalidate();
}

void Widget::hover(const Pointer & /*unused*/) {}

void Widget::within(bool /*unused*/) {}

bool Widget::holdsPointer() const {
    for (const Widget *up = _root == nullptr ? nullptr : _root->hovered(); up != nullptr;
         up = up->_parent) {
        if (up == this) {
            return true;
        }
    }

    return false;
}

bool Widget::wheel(double /*unused*/, const Pointer & /*unused*/) {
    return false;
}

bool Widget::key(const Key & /*unused*/) {
    return false;
}

void Widget::wrote(const std::string & /*unused*/) {}

void Widget::gainedFocus() {
    invalidate();
}

void Widget::lostFocus() {
    invalidate();
}

Widget *Widget::at(const double x, const double y) {
    if (!_visible || !holds(x, y)) {
        return nullptr;
    }

    BLRect region{};

    if (clips(region)
        && (x < region.x || x >= region.x + region.w || y < region.y
             || y >= region.y + region.h)) {
        return nullptr;
    }

    for (auto & child : std::views::reverse(_children)) {
        if (Widget *found = child->at(x, y); found != nullptr) {
            return found;
        }
    }

    return _takesPointer && _enabled ? this : nullptr;
}

// A size a parent has set outright wins over the floor: the floor is what the
// widget asks for when it is left to size itself, and a row splitting itself in
// half has already decided.
double Widget::wantedWidth(Typeface &type) {
    return fixedWidth >= 0.0 ? fixedWidth : std::max(minWidth, naturalWidth(type));
}

double Widget::wantedHeight(Typeface &type, const double width) {
    return fixedHeight >= 0.0 ? fixedHeight : std::max(minHeight, naturalHeight(type, width));
}

Cursor Widget::cursorAt(double /*unused*/, double /*unused*/) const {
    return cursor;
}

bool Widget::holds(const double x, const double y) const {
    return x >= _box.x && x < _box.x + _box.w && y >= _box.y && y < _box.y + _box.h;
}

bool Widget::advance(double /*unused*/) {
    return false;
}

void Widget::animate() const {
    if (_root != nullptr) {
        _root->live(const_cast<Widget *>(this));
    }
}

double Widget::now() const {
    return _root != nullptr ? _root->now() : 0.0;
}

}
