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
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include <blend2d/blend2d.h>
#include <gtest/gtest.h>

#include "ttk/draw/Surface.h"
#include "ttk/draw/Theme.h"
#include "ttk/draw/Typeface.h"
#include "ttk/toolkit/Root.h"

namespace tests {

// One process-wide face set. Loads the first time it is asked for, and the
// cases that need text skip themselves when no font could be found.
ttk::Typeface &fonts();

bool fontsLoaded();

// A window's pixels with no window: an offscreen surface driven through the same
// compose step Shell::draw uses, plus the root it paints.
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

    ttk::Surface &surface() { return _surface; }

    BLContext &context() { return _surface.context(); }

    const BLImage &image() const { return _surface.image(); }

    int width() const { return _width; }
    int height() const { return _height; }

    // Advances to the held clock, settles, paints what is damaged and presents.
    // Answers the pixels painted.
    std::size_t frame();

    // Moves the clock to `now`, then as frame().
    std::size_t frameAt(double now);

    // With the whole window damaged.
    std::size_t full();

    // Advances the held clock by one refresh at `hz` and answers it, for frameAt().
    double tick(double hz = 60.0);

    // `frames` ticks at `hz`, each painted. For running a tween to its end.
    std::size_t run(int frames, double hz = 60.0);

    double now() const { return _now; }

    void resize(int width, int height);

    // Lays the tree out without painting.
    void settle();

    // Back to how it started: nothing in the page or the layers, nothing hovered,
    // held or focused, no damage standing and the clock at zero.
    void bare();

    // A copy of every pixel, row by row, as 0xAARRGGBB.
    [[nodiscard]] std::vector<std::uint32_t> pixels() const;

    // One pixel, as 0xAARRGGBB. Outside the canvas it is 0.
    [[nodiscard]] std::uint32_t at(int x, int y) const;

    // Whether the pixels standing equal a fresh repaint of the whole window.
    // Repaints the window, so call it last in a case or paint again after.
    bool matchesFull();

private:
    ttk::Surface _surface;

    std::unique_ptr<ttk::Root> _root;

    int _width;
    int _height;

    double _now = 0.0;
};

// Alone in the page, placed at the size it asks for when one is not given.
template <typename Kind>
Kind *mount(Canvas &canvas, std::unique_ptr<Kind> widget, const double width = 0.0, const double height = 0.0) {
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

    canvas.frame();

    return raw;
}

// One widget through one Painter, with no tree, damage or clip above it.
std::size_t paintOnce(Canvas &canvas, ttk::Widget &widget);

// A case that paints text. Skips when no font is installed, and puts the theme back.
class Painted : public ::testing::Test {
protected:
    void SetUp() override {
        if (!fontsLoaded()) {
            GTEST_SKIP() << "no font installed";
        }
    }

    void TearDown() override { ttk::Theme::set_mode(ttk::Theme::Mode::System); }
};

}
