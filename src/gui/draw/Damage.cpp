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

#include "gui/draw/Damage.h"

namespace {

BLRectI enclose(const std::vector<BLRectI> &regions) {
    int left = regions.front().x;
    int top = regions.front().y;
    int right = left + regions.front().w;
    int bottom = top + regions.front().h;

    for (const BLRectI &region : regions) {
        left = std::min(left, region.x);
        top = std::min(top, region.y);
        right = std::max(right, region.x + region.w);
        bottom = std::max(bottom, region.y + region.h);
    }

    return {left, top, right - left, bottom - top};
}

}

void Damage::resize(const int width, const int height) {
    _width = width;
    _height = height;
    _regions.clear();
}

BLRectI Damage::clampTo(const BLRect &region) const {
    const int left = std::max(0, static_cast<int>(std::floor(region.x)));
    const int top = std::max(0, static_cast<int>(std::floor(region.y)));
    const int right = std::min(_width, static_cast<int>(std::ceil(region.x + region.w)));
    const int bottom = std::min(_height, static_cast<int>(std::ceil(region.y + region.h)));

    return right <= left || bottom <= top ? BLRectI{} : BLRectI{left, top, right - left,
                                                                bottom - top};
}

void Damage::add(const BLRect &region) {
    const BLRectI inside = clampTo(region);

    if (inside.w <= 0 || inside.h <= 0) {
        return;
    }

    // A pointer can report a hundred moves between two frames, and each of them
    // asks for the same rectangle; without this the widget under it is painted
    // once per report rather than once per frame.
    for (const BLRectI &held : _regions) {
        if (inside.x >= held.x && inside.y >= held.y && inside.x + inside.w <= held.x + held.w
            && inside.y + inside.h <= held.y + held.h) {
            return;
        }
    }

    _regions.push_back(inside);

    if (_regions.size() > CROWDED) {
        _regions = {enclose(_regions)};
    }
}

void Damage::all() {
    _regions = {{0, 0, _width, _height}};
}
