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

#include <vector>

#include <SDL3/SDL.h>
#include <blend2d/blend2d.h>

#include "gui/draw/Damage.h"

// The window's own pixels, drawn into directly where the video driver has a
// framebuffer of its own and through a buffer of ours where it has not. Only the
// rectangles that changed are repainted, and only those are presented.
class Surface {
public:
    // Takes, or retakes, the window's surface. Called again after every resize,
    // which is when SDL throws the old one away.
    bool attach(SDL_Window *window);

    // True where the video driver has a framebuffer of its own, which is the only
    // case in which the window's pixels are drawn into in place.
    [[nodiscard]] bool direct() const { return _direct; }

    // Re-takes the surface if SDL has replaced it since the last look. Asked every
    // frame: an expose arriving before the resize event would otherwise draw into
    // memory SDL has already freed.
    bool sync(SDL_Window *window);

    // Lets go of the surface before SDL frees it.
    void detach();

    [[nodiscard]] bool ready() const { return _ready; }

    [[nodiscard]] int width() const { return _width; }
    [[nodiscard]] int height() const { return _height; }

    BLContext &context() { return _context; }

    // Marks a region for repaint. Rectangles outside the surface are dropped and
    // ones that overlap are left alone -- painting a pixel twice is cheaper than
    // working out that it would be.
    void damage(const BLRect &region);
    void damageAll();

    // Moves the pixels of `region` down by `dy` (up when negative) instead of
    // repainting them, carrying any pending damage inside it along; the region is
    // still presented whole.
    void shift(const BLRectI &wanted, int dy);

    [[nodiscard]] bool dirty() const { return !_damage.empty() || !_moved.empty(); }

    [[nodiscard]] const std::vector<BLRectI> &regions() const { return _damage.regions(); }

    // Pushes the damaged rectangles and forgets them.
    void present(SDL_Window *window);

    // The window's pixels to a PNG, which Blend2D encodes itself.
    bool save(const char *path);

private:
    // Copies the damaged rectangles of our own buffer into SDL's.
    bool take(SDL_Window *window);

    BLImage _image;
    BLContext _context;

    // False where SDL keeps the window's pixels in a block it reallocates on every
    // reconfigure: a fresh block can land on the old address, so nothing about the
    // surface says it moved. Those draw into a buffer of ours and are copied over.
    bool _direct = true;

    // What was wrapped, so a replacement can be spotted.
    SDL_Surface *_surface = nullptr;
    void *_pixels = nullptr;

    Damage _damage;

    // Presented but not repainted.
    std::vector<BLRectI> _moved;

    int _width = 0;
    int _height = 0;
    bool _ready = false;
};
