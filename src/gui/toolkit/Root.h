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
#include <unordered_set>
#include <vector>

#include <blend2d/blend2d.h>

#include "gui/draw/Typeface.h"
#include "gui/toolkit/Widget.h"

namespace toolkit {

// The top of the tree: what the window shows, where the pointer is, what has the
// keyboard, and which rectangles have changed since the last frame.
class Root {
public:
    explicit Root(Typeface &type) : _type(type) {}

    Widget *content() { return &_page; }

    // Layers over the page, painted in order: dialogs, then popups, then tips and
    // toasts. Each covers the window and lets what is under it through.
    Widget *layer(size_t index);

    static constexpr size_t LAYERS = 4;

    static constexpr size_t DIALOGS = 0;
    static constexpr size_t POPUPS = 1;
    static constexpr size_t NOTICES = 2;
    static constexpr size_t TIPS = 3;

    void resize(double width, double height);

    double width() const { return _width; }
    double height() const { return _height; }

    // Lays the whole tree out again at the next frame.
    void relayout() { _relayout = true; }

    // Runs a pending relayout; true when one happened.
    bool settle();

    void damage(const BLRect &region);
    void damageAll();

    std::vector<BLRect> take();

    bool dirty() const { return !_dirty.empty(); }

    struct Shift {
        BLRectI region;
        int dy;
    };

    // Asks for the pixels of `region` to be moved down by `dy` rather than
    // repainted. Refused when something drawn over the region would not move with
    // it, or when `who` is clipped by anything above it.
    bool shift(const Widget *who, const BLRect &region, int dy);

    std::vector<Shift> takeShifts();

    void paint(BLContext &context, const BLRectI &clip);

    // --- pointer ---

    void motion(double x, double y);
    void press(const Pointer &at);
    void release(const Pointer &at);
    void wheel(double steps, double x, double y);
    void leave();

    Widget *hovered() const { return _hovered; }
    Widget *grabbed() const { return _grabbed; }

    // Where the pointer last was, which is what a tooltip hangs off.
    double pointerX() const { return _pointer.x; }
    double pointerY() const { return _pointer.y; }

    // What the pointer should look like now; the shell asks once per frame.
    Cursor cursor() const;

    // Holds the pointer until release, whatever it passes over.
    void grab(Widget *who);

    // --- keyboard ---

    bool key(const Key &pressed);
    void wrote(const std::string &text);

    void focus(Widget *who);
    Widget *focused() const { return _focused; }

    void focusNext(bool backwards);

    // Called when a field wants the platform's text input on or off.
    std::function<void(bool)> composing;

    // --- animation ---

    void live(Widget *who) { _live.insert(who); }

    void forget(const Widget *who);

    void advance(double now);

    bool busy() const { return !_live.empty(); }

    // The clock every animation is started against, set once per frame.
    void setNow(const double now) { _now = now; }

    double now() const { return _now; }

    Typeface &type() const { return _type; }

    // A popup owns the pointer: a press anywhere else dismisses it.
    // `owner` is the control the popup hangs off: a press on it closes the popup
    // and goes no further, so the control does not reopen what it just shut.
    void setDismiss(std::function<void()> dismiss, const Widget *owner = nullptr) {
        _dismiss = std::move(dismiss);
        _owner = owner;
    }

    bool hasDismiss() const { return static_cast<bool>(_dismiss); }

    // True for the press that closed a popup, so a dialog under one does not read
    // that press as a click on its scrim.
    bool justDismissed() const { return _justDismissed; }

    void dismiss();

private:
    Widget *pick(double x, double y);

    void hoverTo(Widget *who, const Pointer &at);

    void gather(Widget *from, std::vector<Widget *> &out) const;

    class Page : public Widget {};

    Typeface &_type;

    Page _page;
    Page _layers[LAYERS];

    std::vector<BLRect> _dirty;
    std::vector<Shift> _shifts;

    std::unordered_set<Widget *> _live;

    Pointer _pointer{};

    Widget *_hovered = nullptr;
    Widget *_grabbed = nullptr;
    Widget *_focused = nullptr;

    std::function<void()> _dismiss;
    const Widget *_owner = nullptr;
    bool _justDismissed = false;

    double _width = 0.0;
    double _height = 0.0;
    double _now = 0.0;

    bool _relayout = true;
};

}
