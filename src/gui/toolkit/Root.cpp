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

#include "gui/toolkit/Root.h"

namespace {

double area(const BLRect &region) {
    return region.w * region.h;
}

BLRect enclose(const BLRect &one, const BLRect &two) {
    const double left = std::min(one.x, two.x);
    const double top = std::min(one.y, two.y);
    const double right = std::max(one.x + one.w, two.x + two.w);
    const double bottom = std::max(one.y + one.h, two.y + two.h);

    return BLRect{left, top, right - left, bottom - top};
}

// Past this many, one rectangle round the lot is cheaper than the walk each of
// them costs.
constexpr size_t CROWDED = 32;

// A region of its own costs another walk of the tree, measured at what this many
// pixels cost to fill.
constexpr double WALK = 128.0 * 1024.0;

// Worth merging: the rectangle round the two, answered in `round`, wastes less
// than the walk that keeping them apart would cost.
bool joins(const BLRect &one, const BLRect &two, BLRect &round) {
    // A pointer reports many times between two frames and asks for the same
    // rectangle each time, so the one already inside is the case worth taking
    // before any of the arithmetic.
    if (two.x >= one.x && two.y >= one.y && two.x + two.w <= one.x + one.w
        && two.y + two.h <= one.y + one.h) {
        round = one;

        return true;
    }

    round = enclose(one, two);

    return area(round) <= area(one) + area(two) + WALK;
}

// The tree is walked once per region, so a card two of them cross is painted
// twice. A merge can bring two survivors within reach of each other, so the pass
// repeats while it still finds one.
void coalesce(std::vector<BLRect> &regions) {
    for (bool merged = true; merged && regions.size() > 1;) {
        size_t kept = 0;

        merged = false;

        for (const BLRect &region : regions) {
            size_t with = kept;
            BLRect round{};

            while (with > 0 && !joins(regions[with - 1], region, round)) {
                --with;
            }

            if (with > 0) {
                regions[with - 1] = round;
                merged = true;

                continue;
            }

            regions[kept] = region;
            ++kept;
        }

        regions.resize(kept);
    }
}

bool above(const toolkit::Widget *leaf, const toolkit::Widget *up) {
    for (; leaf != nullptr; leaf = leaf->parent()) {
        if (leaf == up) {
            return true;
        }
    }

    return false;
}

}

namespace toolkit {

Widget *Root::layer(const size_t index) {
    return &_layers[std::min(index, LAYERS - 1)];
}

void Root::resize(const double width, const double height) {
    if (width == _width && height == _height) {
        return;
    }

    _width = width;
    _height = height;
    _relayout = true;
}

bool Root::settle() {
    if (!_relayout) {
        return false;
    }

    _relayout = false;

    const BLRect whole{0.0, 0.0, _width, _height};

    _page.attach(this);
    _page.place(whole, _type);

    for (Page &over : _layers) {
        over.attach(this);
        over.place(whole, _type);
    }

    // A relayout moves anything, so nothing short of the window is safe to keep.
    damageAll();

    // What was under the pointer has moved, and nothing else asks again: a widget
    // left hovered keeps its lit look under a pointer that is elsewhere.
    if (_grabbed == nullptr && _hovered != nullptr
        && !_hovered->holds(_pointer.x, _pointer.y)) {
        hoverTo(pick(_pointer.x, _pointer.y), _pointer);
    }

    return true;
}

void Root::damage(const BLRect &region) {
    if (region.w <= 0.0 || region.h <= 0.0) {
        return;
    }

    // Too many to be worth keeping apart: the rest of the frame folds into one.
    if (_crowded) {
        _dirty.front() = enclose(_dirty.front(), region);

        return;
    }

    _dirty.push_back(region);

    if (_dirty.size() <= CROWDED) {
        return;
    }

    BLRect whole = _dirty.front();

    for (const BLRect &held : _dirty) {
        whole = enclose(whole, held);
    }

    _dirty.assign(1, whole);
    _crowded = true;
}

void Root::damageAll() {
    _dirty.clear();
    _dirty.emplace_back(0.0, 0.0, _width, _height);
}

std::vector<BLRect> Root::take() {
    std::vector<BLRect> taken;

    taken.swap(_dirty);
    _crowded = false;

    if (taken.size() > 1) {
        coalesce(taken);
    }

    return taken;
}

bool Root::shift(const Widget *who, const BLRect &region, const int dy) {
    const Widget *top = who;

    for (const Widget *up = who->parent(); up != nullptr; up = up->parent()) {
        BLRect limit{};

        if (up->clips(limit)) {
            return false;
        }

        top = up;
    }

    size_t above = 0;

    for (size_t index = 0; index < LAYERS; ++index) {
        if (top == &_layers[index]) {
            above = index + 1;
        }
    }

    for (size_t index = above; index < LAYERS; ++index) {
        for (const Widget::Ptr &child : _layers[index].children()) {
            const BLRect over = child->drawn();

            if (child->visible() && over.x < region.x + region.w && over.x + over.w > region.x
                && over.y < region.y + region.h && over.y + over.h > region.y) {
                return false;
            }
        }
    }

    _shifts.push_back(Shift{
        .region = BLRectI{static_cast<int>(std::ceil(region.x)), static_cast<int>(std::ceil(region.y)),
                          static_cast<int>(std::floor(region.x + region.w))
                              - static_cast<int>(std::ceil(region.x)),
                          static_cast<int>(std::floor(region.y + region.h))
                              - static_cast<int>(std::ceil(region.y))},
        .dy = dy,
    });

    return true;
}

std::vector<Root::Shift> Root::takeShifts() {
    std::vector<Shift> taken;

    taken.swap(_shifts);

    return taken;
}

void Root::paint(BLContext &context, const BLRectI &clip) {
    const Painter painter(context, _type, clip);

    _page.paint(painter);

    for (Page &over : _layers) {
        over.paint(painter);
    }
}

Widget *Root::pick(const double x, const double y) {
    for (size_t index = LAYERS; index > 0; --index) {
        if (Widget *found = _layers[index - 1].at(x, y); found != nullptr) {
            return found;
        }
    }

    return _page.at(x, y);
}

void Root::hoverTo(Widget *who, const Pointer &at) {
    if (_hovered == who) {
        if (who != nullptr) {
            who->hover(at);
        }

        return;
    }

    Widget *was = _hovered;

    if (was != nullptr) {
        was->leave();
    }

    _hovered = who;

    if (who != nullptr) {
        who->enter();
        who->hover(at);
    }

    // The containers hear it only where the two paths part.
    for (Widget *up = was == nullptr ? nullptr : was->parent(); up != nullptr;
         up = up->parent()) {
        if (!above(who, up)) {
            up->within(false);
        }
    }

    for (Widget *up = who == nullptr ? nullptr : who->parent(); up != nullptr;
         up = up->parent()) {
        if (!above(was, up)) {
            up->within(true);
        }
    }
}

Cursor Root::cursor() const {
    // A widget holding the pointer says what it looks like, wherever it has got to.
    if (_grabbed != nullptr) {
        return _grabbed->cursorAt(_pointer.x, _pointer.y);
    }

    return _hovered == nullptr ? Cursor::Default
                               : _hovered->cursorAt(_pointer.x, _pointer.y);
}

void Root::motion(const double x, const double y) {
    const Pointer at{.x = x, .y = y};

    _pointer = at;

    if (_grabbed != nullptr) {
        _grabbed->drag(at);

        return;
    }

    hoverTo(pick(x, y), at);
}

void Root::press(const Pointer &at) {
    _pointer = at;

    Widget *who = pick(at.x, at.y);

    _justDismissed = false;

    // A press outside an open popup closes it. The press then carries on, so the
    // dropdown next to this one opens rather than only the first closing -- unless
    // it landed on the control the popup belongs to, which would reopen it.
    if (_dismiss && (who == nullptr || _layers[POPUPS].at(at.x, at.y) == nullptr)) {
        const Widget *owner = _owner;

        dismiss();

        _justDismissed = true;

        if (who == nullptr) {
            return;
        }

        for (const Widget *up = who; up != nullptr; up = up->parent()) {
            if (up == owner) {
                return;
            }
        }
    }

    hoverTo(who, at);

    for (Widget *up = who; up != nullptr; up = up->parent()) {
        if (up->enabled() && up->press(at)) {
            up->_pressed = true;

            if (_grabbed == nullptr) {
                _grabbed = up;
            }

            return;
        }
    }

    // A press on nothing in particular takes the keyboard away from a field.
    focus(nullptr);
}

void Root::release(const Pointer &at) {
    Widget *who = _grabbed;

    _grabbed = nullptr;

    if (who == nullptr) {
        return;
    }

    who->_pressed = false;
    who->release(at);

    hoverTo(pick(at.x, at.y), at);
}

void Root::grab(Widget *who) {
    _grabbed = who;
}

void Root::wheel(const double steps, const double x, const double y) {
    const Pointer at{.x = x, .y = y};

    for (Widget *up = pick(x, y); up != nullptr; up = up->parent()) {
        if (up->wheel(steps, at)) {
            return;
        }
    }
}

void Root::leave() {
    if (_grabbed != nullptr) {
        return;
    }

    hoverTo(nullptr, Pointer{.x = -1.0, .y = -1.0});
}

bool Root::key(const Key &pressed) const {
    for (Widget *up = _focused; up != nullptr; up = up->parent()) {
        if (up->key(pressed)) {
            return true;
        }
    }

    return false;
}

void Root::wrote(const std::string &text) const {
    if (_focused != nullptr) {
        _focused->wrote(text);
    }
}

void Root::focus(Widget *who) {
    if (_focused == who) {
        return;
    }

    Widget *was = _focused;

    _focused = who;

    if (was != nullptr) {
        was->lostFocus();
    }

    if (_focused != nullptr) {
        _focused->gainedFocus();
    }

    if (composing) {
        composing(_focused != nullptr && _focused->takesFocus());
    }
}

void Root::gather(const Widget *from, std::vector<Widget *> &out) {
    for (const Widget::Ptr &child : from->children()) {
        if (!child->visible() || !child->enabled()) {
            continue;
        }

        if (child->takesFocus()) {
            out.push_back(child.get());
        }

        gather(child.get(), out);
    }
}

void Root::focusNext(const bool backwards) {
    std::vector<Widget *> order;

    // A dialog or a popup owns the keyboard while it is up.
    for (size_t index = LAYERS; index > 0 && order.empty(); --index) {
        gather(&_layers[index - 1], order);
    }

    if (order.empty()) {
        gather(&_page, order);
    }

    if (order.empty()) {
        return;
    }

    const auto found = std::ranges::find(order, _focused);

    if (found == order.end()) {
        focus(backwards ? order.back() : order.front());

        return;
    }

    const size_t at = static_cast<size_t>(found - order.begin());
    const size_t next = backwards ? (at + order.size() - 1) % order.size()
                                  : (at + 1) % order.size();

    focus(order[next]);
}

void Root::forget(const Widget *who) {
    if (who == nullptr) {
        return;
    }

    _live.erase(const_cast<Widget *>(who));

    std::erase_if(_sleeping, [who](const Sleeper &kept) { return kept.who == who; });

    if (_hovered == who) {
        _hovered = nullptr;
    }

    if (_grabbed == who) {
        _grabbed = nullptr;
    }

    if (_focused == who) {
        _focused = nullptr;
    }

    if (_owner == who) {
        _owner = nullptr;
    }

    for (const Widget::Ptr &child : who->children()) {
        forget(child.get());
    }
}

void Root::live(Widget *who) {
    _live.insert(who);

    std::erase_if(_sleeping, [who](const Sleeper &kept) { return kept.who == who; });
}

void Root::wakeAt(Widget *who, const double when) {
    for (Sleeper &kept : _sleeping) {
        if (kept.who == who) {
            kept.due = std::min(kept.due, when);

            return;
        }
    }

    _sleeping.push_back(Sleeper{.who = who, .due = when});
}

double Root::waking() const {
    double soonest = -1.0;

    for (const Sleeper &kept : _sleeping) {
        if (soonest < 0.0 || kept.due < soonest) {
            soonest = kept.due;
        }
    }

    return soonest;
}

void Root::advance(const double now) {
    for (size_t at = _sleeping.size(); at > 0; --at) {
        if (const Sleeper &kept = _sleeping[at - 1]; kept.due <= now) {
            _live.insert(kept.who);
            _sleeping.erase(_sleeping.begin() + static_cast<ptrdiff_t>(at - 1));
        }
    }

    if (_live.empty()) {
        return;
    }

    std::vector<Widget *> const running(_live.begin(), _live.end());

    for (Widget *who : running) {
        if (!who->advance(now)) {
            _live.erase(who);
        }
    }
}

void Root::dismiss() {
    if (!_dismiss) {
        return;
    }

    const std::function<void()> closing = _dismiss;

    _dismiss = nullptr;
    _owner = nullptr;

    closing();
}

}
