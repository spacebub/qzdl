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

#include "gui/toolkit/Widget.h"

namespace toolkit {

// A view onto something taller than itself, with a bar down the right. The content
// is a single child, placed at a negative offset and clipped.
class Scroll : public Widget {
public:
    Scroll();

    [[nodiscard]] Widget *content() const { return _content; }

    // Takes ownership and becomes the one thing scrolled.
    Widget *hold(Ptr child);

    [[nodiscard]] double offset() const { return _offset; }

    // Lands there at once, and stops a glide on its way somewhere else.
    void scrollTo(double offset);

    // Brings a box in the content into view, moving as little as possible.
    void reveal(const BLRect &wanted);

    // How tall the content turned out, which is what the bar is measured against.
    [[nodiscard]] double reach() const { return _reach; }

    // For a list drawn by hand rather than held as a child.
    void setReach(double reach);

    [[nodiscard]] bool scrollable() const;

    void arrange(Typeface &type) override;

    bool clips(BLRect &region) const override;

    void paint(const Painter &painter) override;

    bool wheel(double steps, const Pointer &at) override;

    bool press(const Pointer &at) override;
    void drag(const Pointer &at) override;
    void release(const Pointer &at) override;

    Widget *at(double x, double y) override;

    bool advance(double now) override;

private:
    void settle(double offset);

    [[nodiscard]] BLRect lane() const;
    [[nodiscard]] BLRect thumb() const;

    Widget *_content = nullptr;

    double _offset = 0.0;
    double _reach = 0.0;

    // The wheel moves a goal; the offset closes in on it a fixed share per unit
    // time, so turns run together rather than each starting a curve of its own.
    double _goal = 0.0;
    double _via = 0.0;
    double _along = 0.0;
    double _glided = 0.0;
    bool _gliding = false;

    // Where the thumb was when it was grabbed.
    double _grabbed = 0.0;
    double _from = 0.0;
    bool _dragging = false;
};

}
