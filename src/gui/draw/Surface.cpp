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

#include <SDL3/SDL.h>

#include "gui/draw/Surface.h"

namespace {

}

bool Surface::sync(SDL_Window *window) {
    SDL_Surface *surface = SDL_GetWindowSurface(window);

    if (surface == nullptr) {
        // Minimized, or the platform has nothing to give right now.
        detach();

        return false;
    }

    if (_ready && surface->w == _width && surface->h == _height) {
        if (surface == _surface && surface->pixels == _pixels) {
            return true;
        }

        // Off the direct path our own buffer still holds the frame, so there is
        // nothing to re-wrap -- but SDL's is new, and a frame with no damage would
        // present nothing into it and leave whatever was in that memory on screen.
        if (!_direct) {
            _surface = surface;
            _pixels = surface->pixels;

            damageAll();

            return true;
        }
    }

    return attach(window);
}

bool Surface::attach(SDL_Window *window) {
    detach();

    SDL_Surface *surface = SDL_GetWindowSurface(window);

    if (surface == nullptr) {
        return false;
    }

    // A window surface carries no meaningful alpha; both of these are drawn into
    // as opaque, which is also the cheaper pipeline.
    if (surface->format != SDL_PIXELFORMAT_XRGB8888
        && surface->format != SDL_PIXELFORMAT_ARGB8888) {
        return false;
    }

    const char *driver = SDL_GetCurrentVideoDriver();

    _direct = driver != nullptr
        && (SDL_strcmp(driver, "windows") == 0 || SDL_strcmp(driver, "x11") == 0);

    const BLResult made = _direct
        ? _image.create_from_data(surface->w, surface->h, BL_FORMAT_XRGB32, surface->pixels,
                                  surface->pitch)
        : _image.create(surface->w, surface->h, BL_FORMAT_XRGB32);

    if (made != BL_SUCCESS) {
        return false;
    }

    if (_context.begin(_image) != BL_SUCCESS) {
        _image.reset();

        return false;
    }

    _surface = surface;
    _pixels = surface->pixels;
    _width = surface->w;
    _height = surface->h;
    _ready = true;

    _damage.resize(_width, _height);
    damageAll();

    return true;
}

void Surface::detach() {
    if (_ready) {
        _context.end();
        _image.reset();
    }

    _damage.resize(0, 0);
    _moved.clear();
    _surface = nullptr;
    _pixels = nullptr;
    _ready = false;
    _width = 0;
    _height = 0;
}

void Surface::damage(const BLRect &region) {
    if (_ready) {
        _damage.add(region);
    }
}

void Surface::damageAll() {
    if (!_ready) {
        return;
    }

    _damage.all();
    _moved.clear();
}

void Surface::shift(const BLRectI &wanted, const int dy) {
    if (!_ready || dy == 0) {
        return;
    }

    const int left = std::max(0, wanted.x);
    const int top = std::max(0, wanted.y);
    const int right = std::min(_width, wanted.x + wanted.w);
    const int bottom = std::min(_height, wanted.y + wanted.h);

    if (right <= left || bottom <= top) {
        return;
    }

    const BLRectI region{left, top, right - left, bottom - top};

    if (std::abs(dy) >= region.h) {
        damage(BLRect{static_cast<double>(region.x), static_cast<double>(region.y),
                      static_cast<double>(region.w), static_cast<double>(region.h)});

        return;
    }

    _context.flush(BL_CONTEXT_FLUSH_SYNC);

    BLImageData data{};

    if (_image.get_data(&data) != BL_SUCCESS) {
        return;
    }

    // Ours to write: the image is either our own or wraps SDL's surface.
    auto *pixels = static_cast<uint8_t *>(data.pixel_data);
    const size_t wide = static_cast<size_t>(region.w) * 4;
    const size_t at = static_cast<size_t>(region.x) * 4;

    const auto row = [&](const int y) {
        return pixels + (static_cast<size_t>(y) * data.stride) + at;
    };

    if (dy < 0) {
        for (int y = region.y; y < region.y + region.h + dy; ++y) {
            SDL_memcpy(row(y), row(y - dy), wide);
        }
    } else {
        for (int y = region.y + region.h - 1; y >= region.y + dy; --y) {
            SDL_memcpy(row(y), row(y - dy), wide);
        }
    }

    // Whatever was due a repaint inside has gone with the pixels.
    const std::vector<BLRectI> pending = _damage.regions();

    for (const BLRectI &held : pending) {
        if (held.x < region.x + region.w && held.x + held.w > region.x
            && held.y < region.y + region.h && held.y + held.h > region.y) {
            damage(BLRect{static_cast<double>(held.x), static_cast<double>(held.y + dy),
                          static_cast<double>(held.w), static_cast<double>(held.h)});
        }
    }

    _moved.push_back(region);
}

void Surface::present(SDL_Window *window) {
    if (!_ready || _damage.empty()) {
        return;
    }

    // Everything queued has to have landed in the pixels before the desktop reads
    // them; the context is synchronous, so this is the one place it has to be said.
    _context.flush(BL_CONTEXT_FLUSH_SYNC);

    // Off the direct path our own pixels have to be handed over, the damaged
    // rectangles alone unless the surface is one take() has not seen.
    if (!_direct && !take(window)) {
        return;
    }

    std::vector<SDL_Rect> rects;

    rects.reserve(_damage.regions().size() + _moved.size());

    for (const BLRectI &region : _damage.regions()) {
        rects.push_back({.x = region.x, .y = region.y, .w = region.w, .h = region.h});
    }

    for (const BLRectI &region : _moved) {
        rects.push_back({.x = region.x, .y = region.y, .w = region.w, .h = region.h});
    }

    SDL_UpdateWindowSurfaceRects(window, rects.data(), static_cast<int>(rects.size()));

    _damage.clear();
    _moved.clear();
}

bool Surface::take(SDL_Window *window) {
    SDL_Surface *surface = SDL_GetWindowSurface(window);

    if (surface == nullptr || surface->w != _width || surface->h != _height) {
        return false;
    }

    BLImageData data{};

    if (_image.get_data(&data) != BL_SUCCESS) {
        return false;
    }

    // SDL rotates between surfaces of its own off the direct path. One not filled
    // before holds whatever was last in that memory, and only this frame's damage
    // would go over it; the image behind is whole, so all of it does.
    if (surface != _surface || surface->pixels != _pixels) {
        damageAll();
    }

    const auto *from = static_cast<const uint8_t *>(data.pixel_data);
    auto *to = static_cast<uint8_t *>(surface->pixels);

    const auto copy = [&](const BLRectI &region) {
        const size_t wide = static_cast<size_t>(region.w) * 4;
        const size_t at = static_cast<size_t>(region.x) * 4;

        for (int row = region.y; row < region.y + region.h; ++row) {
            SDL_memcpy(to + (static_cast<size_t>(row) * surface->pitch) + at,
                       from + (static_cast<size_t>(row) * data.stride) + at, wide);
        }
    };

    for (const BLRectI &region : _damage.regions()) {
        copy(region);
    }

    for (const BLRectI &region : _moved) {
        copy(region);
    }

    _surface = surface;
    _pixels = surface->pixels;

    return true;
}

bool Surface::save(const char *path) {
    if (!_ready) {
        return false;
    }

    _context.flush(BL_CONTEXT_FLUSH_SYNC);

    // The image wraps SDL's pixels, so this writes exactly what is on screen.
    return _image.write_to_file(path) == BL_SUCCESS;
}
