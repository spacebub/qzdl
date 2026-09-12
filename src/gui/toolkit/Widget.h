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

#include <memory>
#include <string>
#include <vector>

#include <blend2d/blend2d.h>

#include "gui/draw/Typeface.h"
#include "gui/toolkit/Event.h"
#include "gui/toolkit/Painter.h"

namespace toolkit {

class Root;

// One thing on screen.
//
// Boxes are in window coordinates, so damage and hit testing are both a rectangle
// test. Layout runs top down: a parent asks each child what it wants, hands it a
// box, and the child places its own children inside it.
class Widget {
public:
    using Ptr = std::unique_ptr<Widget>;

    Widget() = default;
    virtual ~Widget() = default;

    Widget(const Widget &) = delete;
    Widget &operator=(const Widget &) = delete;
    Widget(Widget &&) = delete;
    Widget &operator=(Widget &&) = delete;

    // --- tree ---

    Widget *add(Ptr child);

    template <typename Kind>
    Kind *append(std::unique_ptr<Kind> child) {
        Kind *raw = child.get();

        add(std::move(child));

        return raw;
    }

    void clear();

    // Drops `child` and everything under it.
    void erase(const Widget *child);

    [[nodiscard]] const std::vector<Ptr> &children() const { return _children; }

    [[nodiscard]] Widget *parent() const { return _parent; }

    [[nodiscard]] Root *root() const { return _root; }

    // --- geometry ---

    [[nodiscard]] const BLRect &box() const { return _box; }

    void place(const BLRect &box, Typeface &type);

    // What it would like across, and how tall it is once that is settled.
    virtual double naturalWidth(Typeface &type);
    virtual double naturalHeight(Typeface &type, double width);

    // Places children inside the box already set.
    virtual void arrange(Typeface &type);

    // --- state ---

    [[nodiscard]] bool visible() const { return _visible; }
    void setVisible(bool value);

    [[nodiscard]] bool enabled() const { return _enabled; }
    void setEnabled(bool value);

    [[nodiscard]] bool hovered() const { return _hovered; }

    // True while the pointer is on it or on anything in it.
    [[nodiscard]] bool holdsPointer() const;
    [[nodiscard]] bool pressed() const { return _pressed; }
    [[nodiscard]] bool focused() const;

    // How a row shares its spare width; zero never takes any.
    double stretch = 0.0;

    // Honoured by the layouts in Box.h; negative is "ask the widget".
    double fixedWidth = -1.0;
    double fixedHeight = -1.0;

    // A hard floor: a row short of room overflows
    // and is clipped rather than squeezing what is in it past legibility.
    double minWidth = 0.0;
    double minHeight = 0.0;

    // What the widget wants across and down, with the floors applied.
    double wantedWidth(Typeface &type);
    double wantedHeight(Typeface &type, double width);

    // Shown while the pointer rests on it.
    std::string hint;

    // What the pointer turns into over it.
    Cursor cursor = Cursor::Default;

    // For a widget whose parts want different ones: the grip of a row, say.
    [[nodiscard]] virtual Cursor cursorAt(double x, double y) const;

    // --- painting ---

    virtual void paint(const Painter &painter);

    // Everything the widget puts on screen, which is its box unless it casts a
    // shadow or grows on hover. Damage and the cull test both go by this.
    [[nodiscard]] virtual BLRect drawn() const { return _box; }

    // What a child may draw into; empty for none. A scroller answers its viewport.
    virtual bool clips(BLRect &region) const;

    void invalidate() const;
    void invalidate(const BLRect &region) const;

    // --- events ---

    // True when the press was taken; the widget then receives drag and release.
    virtual bool press(const Pointer &at);
    virtual void drag(const Pointer &at);
    virtual void release(const Pointer &at);

    virtual void enter();
    virtual void leave();
    virtual void hover(const Pointer &at);

    // The pointer has come onto something in it, or left the last such thing.
    virtual void within(bool inside);

    virtual bool wheel(double steps, const Pointer &at);

    virtual bool key(const Key &pressed);
    virtual void wrote(const std::string &text);

    [[nodiscard]] virtual bool takesFocus() const { return false; }
    virtual void gainedFocus();
    virtual void lostFocus();

    // The deepest widget under the point that wants the pointer.
    virtual Widget *at(double x, double y);

    [[nodiscard]] bool holds(double x, double y) const;

    // --- animation ---

    // Answers false once nothing is left in flight, which drops it from the
    // frame loop and lets the window go back to sleep.
    virtual bool advance(double now);

    // Puts the widget on the root's live list.
    void animate() const;

    // The frame clock, for starting a tween from an event handler.
    [[nodiscard]] double now() const;

protected:
    // Called after place() has set the box, before arrange().
    virtual void moved() {}

    // A leaf that answers the pointer says so once, in its constructor; a plain
    // container lets what is under it through.
    bool _takesPointer = false;

    void attach(Root *root);

    BLRect _box{};

private:
    std::vector<Ptr> _children;
    Widget *_parent = nullptr;
    Root *_root = nullptr;

    bool _visible = true;
    bool _enabled = true;

    bool _hovered = false;
    bool _pressed = false;

    friend class Root;
};

}
