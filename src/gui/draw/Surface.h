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

#include <blend2d/blend2d.h>

struct SDL_Window;
struct SDL_Surface;

// The window's own pixels, drawn into directly.
//
// SDL_GetWindowSurface hands back a CPU framebuffer; Blend2D writes into it in
// place, and SDL_UpdateWindowSurfaceRects pushes the changed parts to the desktop.
// On Windows that is a GDI BitBlt and on X11 a shared-memory image, so nothing here
// opens a graphics device, loads a driver or allocates a texture. Where no video
// driver offers a framebuffer of its own -- Wayland and macOS -- SDL falls back to
// uploading the same pixels through its 2D render API, which still works and costs
// what a GPU context costs.
//
// Damage is tracked because it can be: only the rectangles that actually changed
// are repainted, and only those are presented.
class Surface {
public:
    // Takes, or retakes, the window's surface. Called again after every resize,
    // which is when SDL throws the old one away.
    bool attach(SDL_Window *window);

    // True where the video driver has a framebuffer of its own, which is the only
    // case in which the window's pixels are drawn into in place.
    [[nodiscard]] bool direct() const { return _direct; }

    // Re-takes the surface if SDL has replaced it since the last look.
    //
    // SDL throws a window surface away whenever the window changes size, and the
    // replacement is a different allocation. Waiting for a resize event to notice
    // means an expose that arrives first draws into memory SDL has already freed,
    // which is torn fragments and a black window until the next resize repairs it.
    // Asking every frame is cheap: SDL hands back the cached surface untouched
    // when nothing has changed.
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
    void shift(const BLRectI &region, int dy);

    [[nodiscard]] bool dirty() const { return !_damage.empty() || !_moved.empty(); }

    [[nodiscard]] const std::vector<BLRectI> &regions() const { return _damage; }

    // Pushes the damaged rectangles and forgets them.
    void present(SDL_Window *window);

    // The window's pixels to a PNG, which Blend2D encodes itself.
    bool save(const char *path);

private:
    // Copies the damaged rectangles of our own buffer into SDL's.
    bool take(SDL_Window *window);

    BLImage _image;
    BLContext _context;

    // Where no video driver has a framebuffer of its own, SDL keeps the window's
    // pixels in a block it frees and mallocs again whenever the window is
    // reconfigured -- and a fresh block can land on the address the old one had,
    // so nothing about the surface says it happened. Drawing into a buffer of our
    // own and copying it over at present time is proof against that; the direct
    // path is kept where it is real, which is what the whole approach rests on.
    bool _direct = true;

    // What was wrapped, so a replacement can be spotted.
    SDL_Surface *_surface = nullptr;
    void *_pixels = nullptr;

    std::vector<BLRectI> _damage;

    // Presented but not repainted.
    std::vector<BLRectI> _moved;

    int _width = 0;
    int _height = 0;
    bool _ready = false;
};
