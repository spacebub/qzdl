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

#include <cstring>

#include "ttk/draw/Damage.h"
#include "ttk/toolkit/Compose.h"
#include "ttk/toolkit/Painter.h"

#include "support/Canvas.h"

using namespace ttk;

namespace {

bool &loaded() {
    static bool value = false;

    return value;
}

}

namespace tests {

Typeface &fonts() {
    static Typeface type = [] {
        Typeface made;

        loaded() = made.load();

        return made;
    }();

    return type;
}

bool fontsLoaded() {
    fonts();

    return loaded();
}

Canvas::Canvas(const int width, const int height) : _width(width), _height(height) {
    _surface.attach(width, height);

    _root = std::make_unique<Root>(fonts());
    _root->resize(width, height);
    _root->set_now(_now);
}

Canvas::~Canvas() {
    _surface.detach();
}

void Canvas::resize(const int width, const int height) {
    _width = width;
    _height = height;

    _surface.attach(width, height);

    _root->resize(width, height);
    _root->damage_all();
}

double Canvas::tick(const double hz) {
    _now += 1.0 / hz;

    return _now;
}

std::size_t Canvas::run(const int frames, const double hz) {
    std::size_t painted = 0;

    for (int at = 0; at < frames; ++at) {
        painted += frameAt(tick(hz));
    }

    return painted;
}

void Canvas::settle() {
    _root->settle();
}

void Canvas::bare() {
    _root->leave();

    for (size_t at = 0; at < Root::LAYERS; ++at) {
        _root->layer(at)->clear();
    }

    _root->content()->clear();
    _root->relayout();
    _root->settle();

    // settle() damages the window. Nothing is owed to a caller starting over.
    _root->take();
    _root->take_shifts();

    _now = 0.0;
    _root->set_now(_now);

    // Runs every tween down, so nothing is left on the live list.
    for (int at = 0; at < 4; ++at) {
        _root->advance(_now);
    }

    _surface.damage_all();
    _surface.present();
}

std::size_t Canvas::frame() {
    return frameAt(_now);
}

std::size_t Canvas::frameAt(const double now) {
    _now = now;

    _root->set_now(now);
    _root->advance(now);

    const std::size_t painted = compose(*_root, _surface);

    _surface.present();

    return painted;
}

std::size_t Canvas::full() {
    _root->damage_all();

    return frame();
}

std::vector<std::uint32_t> Canvas::pixels() const {
    BLImageData data{};
    std::vector<std::uint32_t> copy(static_cast<std::size_t>(_width) * static_cast<std::size_t>(_height));

    if (_surface.image().get_data(&data) != BL_SUCCESS) {
        return copy;
    }

    for (int y = 0; y < _height; ++y) {
        std::memcpy(copy.data() + (static_cast<std::ptrdiff_t>(y) * _width),
                    static_cast<const std::uint8_t *>(data.pixel_data) + (static_cast<std::ptrdiff_t>(y) * data.stride),
                    static_cast<std::size_t>(_width) * 4);
    }

    return copy;
}

std::uint32_t Canvas::at(const int x, const int y) const {
    BLImageData data{};

    if (x < 0 || y < 0 || x >= _width || y >= _height || _surface.image().get_data(&data) != BL_SUCCESS) {
        return 0;
    }

    std::uint32_t pixel = 0;

    std::memcpy(&pixel,
                static_cast<const std::uint8_t *>(data.pixel_data) + (static_cast<std::ptrdiff_t>(y) * data.stride)
                        + (static_cast<std::ptrdiff_t>(x) * 4),
                4);

    return pixel;
}

bool Canvas::matchesFull() {
    const std::vector<std::uint32_t> partial = pixels();

    full();

    return partial == pixels();
}

std::size_t paintOnce(Canvas &canvas, Widget &widget) {
    const BLRect drawn = widget.drawn();
    Damage frame;

    frame.resize(canvas.width(), canvas.height());

    const BLRectI clip = frame.clamp_to(drawn);

    const Painter painter(canvas.context(), canvas.type(), clip);

    canvas.context().save();
    canvas.context().clip_to_rect(clip);

    widget.paint(painter);

    canvas.context().restore();
    canvas.context().flush(BL_CONTEXT_FLUSH_SYNC);

    return static_cast<std::size_t>(clip.w) * static_cast<std::size_t>(clip.h);
}

}
