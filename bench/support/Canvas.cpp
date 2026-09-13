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
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "gui/draw/Theme.h"
#include "support/Canvas.h"

namespace {

bool &loaded() {
    static bool value = false;

    return value;
}

}

namespace bench {

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
    _image.create(width, height, BL_FORMAT_XRGB32);
    _context.begin(_image);

    _root = std::make_unique<toolkit::Root>(fonts());
    _root->resize(width, height);
    _root->setNow(_now);
}

Canvas::~Canvas() {
    _context.end();
}

void Canvas::resize(const int width, const int height) {
    _context.end();

    _width = width;
    _height = height;

    _image.create(width, height, BL_FORMAT_XRGB32);
    _context.begin(_image);

    _root->resize(width, height);
    _root->damageAll();
}

double Canvas::tick(const double hz) {
    _now += 1.0 / hz;

    return _now;
}

void Canvas::settle() {
    _root->settle();
}

void Canvas::bare() {
    _root->leave();

    for (size_t at = 0; at < toolkit::Root::LAYERS; ++at) {
        _root->layer(at)->clear();
    }

    _root->content()->clear();
    _root->relayout();
    _root->settle();

    // settle() damages the window; nothing is owed to a caller starting over.
    _root->take();
    _root->takeShifts();

    _now = 0.0;
    _root->setNow(_now);

    // Runs every tween down, so nothing is left on the live list.
    for (int at = 0; at < 4; ++at) {
        _root->advance(_now);
    }

    _damage.clear();
}

std::size_t Canvas::frame() {
    return frameAt(_now);
}

std::size_t Canvas::full() {
    _root->damageAll();

    return frameAt(_now);
}

void Canvas::shift(const BLRectI &wanted, const int dy) {
    if (dy == 0) {
        return;
    }

    const int left = std::max(0, wanted.x);
    const int top = std::max(0, wanted.y);
    const int right = std::min(_width, wanted.x + wanted.w);
    const int bottom = std::min(_height, wanted.y + wanted.h);

    if (right <= left || bottom <= top || std::abs(dy) >= bottom - top) {
        return;
    }

    _context.flush(BL_CONTEXT_FLUSH_SYNC);

    BLImageData data{};

    if (_image.get_data(&data) != BL_SUCCESS) {
        return;
    }

    auto *pixels = static_cast<std::uint8_t *>(const_cast<void *>(data.pixel_data));
    const size_t wide = static_cast<size_t>(right - left) * 4;
    const size_t at = static_cast<size_t>(left) * 4;

    const auto row = [&](const int y) {
        return pixels + (static_cast<size_t>(y) * data.stride) + at;
    };

    if (dy < 0) {
        for (int y = top; y < bottom + dy; ++y) {
            std::memcpy(row(y), row(y - dy), wide);
        }
    } else {
        for (int y = bottom - 1; y >= top + dy; --y) {
            std::memcpy(row(y), row(y - dy), wide);
        }
    }
}

std::size_t paintOnce(Canvas &canvas, toolkit::Widget &widget) {
    const BLRect drawn = widget.drawn();
    Damage frame;

    frame.resize(canvas.width(), canvas.height());

    const BLRectI clip = frame.clampTo(drawn);

    const toolkit::Painter painter(canvas.context(), canvas.type(), clip);

    canvas.context().save();
    canvas.context().clip_to_rect(clip);

    widget.paint(painter);

    canvas.context().restore();
    canvas.context().flush(BL_CONTEXT_FLUSH_SYNC);

    return static_cast<std::size_t>(clip.w) * static_cast<std::size_t>(clip.h);
}

std::size_t Canvas::frameAt(const double now) {
    _root->setNow(now);
    _root->advance(now);

    return pending();
}

std::size_t Canvas::pending() {
    _root->settle();

    const std::vector<BLRect> damage = _root->take();

    for (const toolkit::Root::Shift &moved : _root->takeShifts()) {
        shift(moved.region, moved.dy);
    }

    std::size_t painted = 0;

    _damage.resize(_width, _height);

    for (const BLRect &region : damage) {
        _damage.add(region);
    }

    for (const BLRectI &clip : _damage.regions()) {

        _context.save();
        _context.clip_to_rect(clip);
        _context.fill_rect(BLRect{static_cast<double>(clip.x), static_cast<double>(clip.y),
                                  static_cast<double>(clip.w), static_cast<double>(clip.h)},
                           Theme::of().background);

        _root->paint(_context, clip);

        _context.restore();

        painted += static_cast<std::size_t>(clip.w) * static_cast<std::size_t>(clip.h);
    }

    _context.flush(BL_CONTEXT_FLUSH_SYNC);

    return painted;
}

}
