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

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include <blend2d/blend2d.h>

#include "ttk/draw/Damage.h"
#include "ttk/draw/Typeface.h"
#include "ttk/toolkit/Root.h"

namespace bench {

// One process-wide face set. Loading one costs several milliseconds of file
// reading that no benchmark wants to measure.
ttk::Typeface &fonts();

bool fontsLoaded();

// A window's pixels with no window: the same XRGB32 target, rasteriser and
// damage loop Shell drives, minus SDL.
class Canvas {
public:
    explicit Canvas(int width = 1280, int height = 800);
    ~Canvas();

    Canvas(const Canvas &) = delete;
    Canvas &operator=(const Canvas &) = delete;
    Canvas(Canvas &&) = delete;
    Canvas &operator=(Canvas &&) = delete;

    ttk::Root &ui() { return *_root; }

    ttk::Typeface &type() const { return fonts(); }

    BLContext &context() { return _context; }

    const BLImage &image() const { return _image; }

    int width() const { return _width; }
    int height() const { return _height; }

    // Mirrors Shell::draw. Answers the pixels painted.
    std::size_t frame();

    std::size_t frameAt(double now);

    // Paints what is owed without moving the clock, as the shell's expose path
    // does. A tween started before it will not have advanced by the time it draws.
    std::size_t pending();

    // With the whole window damaged.
    std::size_t full();

    void resize(int width, int height);

    // Advances the frame clock by one refresh at `hz`.
    double tick(double hz = 280.0);

    double now() const { return _now; }

    // Lays the tree out without painting, so a layout can be measured alone.
    void settle();

    // Back to how it started: nothing in the page or the layers, nothing hovered,
    // held or focused, no damage standing and the clock at zero. A benchmark that
    // starts from here reads the same alone as it does in the whole run.
    void bare();

private:
    void shift(const BLRectI &region, int dy);

    BLImage _image;
    BLContext _context;

    std::unique_ptr<ttk::Root> _root;

    int _width;
    int _height;

    // The same bookkeeping Surface does on the way to the window, so a frame is
    // measured against the list the shell would paint.
    ttk::Damage _damage;

    double _now = 0.0;
};

// Alone in the page, placed at the size it asks for when one is not given.
template <typename Kind>
Kind *mount(Canvas &canvas, std::unique_ptr<Kind> widget, const double width = 0.0,
            const double height = 0.0) {
    canvas.bare();

    Kind *raw = canvas.ui().content()->append(std::move(widget));

    // Root attaches the tree in settle(). Without it the widget has no root and
    // popups, damage and animation all go nowhere.
    canvas.ui().relayout();
    canvas.ui().settle();

    ttk::Typeface &type = canvas.type();
    const double across = width > 0.0 ? width : raw->wanted_width(type);
    const double down = height > 0.0 ? height : raw->wanted_height(type, across);

    // Label and Select declare a place() of their own, which hides Widget's.
    static_cast<ttk::Widget *>(raw)->place(BLRect{0.0, 0.0, across, down}, type);

    return raw;
}

// One widget through one Painter, with no tree, damage or clip above it.
std::size_t paintOnce(Canvas &canvas, ttk::Widget &widget);

}
