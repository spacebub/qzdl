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
#include <vector>

#include "gui/toolkit/layout/Box.h"

namespace toolkit {

Box *Box::spacing(const double value) {
    _spacing = value;

    return this;
}

Box *Box::pad(const double all) {
    return pad(all, all, all, all);
}

Box *Box::pad(const double sides, const double ends) {
    return pad(sides, ends, sides, ends);
}

Box *Box::pad(const double left, const double top, const double right, const double bottom) {
    _left = left;
    _top = top;
    _right = right;
    _bottom = bottom;

    return this;
}

Box *Box::align(const Place where) {
    _align = where;

    return this;
}

Box *Box::cross(const Place where) {
    _cross = where;

    return this;
}

Box *Box::grow(const double weight) {
    stretch = weight;

    return this;
}

double Box::gapTotal() const {
    size_t shown = 0;

    for (const Ptr &child : children()) {
        if (child->visible()) {
            ++shown;
        }
    }

    return shown > 1 ? static_cast<double>(shown - 1) * _spacing : 0.0;
}

double Box::naturalWidth(Typeface &type) {
    if (fixedWidth >= 0.0) {
        return fixedWidth;
    }

    double total = 0.0;

    for (const Ptr &child : children()) {
        if (!child->visible()) {
            continue;
        }

        const double wanted = child->wantedWidth(type);

        total = _flow == Flow::Row ? total + wanted : std::max(total, wanted);
    }

    if (_flow == Flow::Row) {
        total += gapTotal();
    }

    return total + _left + _right;
}

double Box::naturalHeight(Typeface &type, const double width) {
    if (fixedHeight >= 0.0) {
        return fixedHeight;
    }

    const double inner = std::max(0.0, width - _left - _right);
    double total = 0.0;

    if (_flow == Flow::Column) {
        for (const Ptr &child : children()) {
            if (!child->visible()) {
                continue;
            }

            const double across = child->fixedWidth >= 0.0 ? child->fixedWidth : inner;

            total += child->wantedHeight(type, across);
        }

        total += gapTotal();
    } else {
        // Every child is measured at the width it will actually be given, spare
        // shared out and all: a wrapped label measured at its own unwrapped width
        // reports one line and the row comes out a line tall.
        const std::vector<double> widths = share(type, inner, 0.0);
        size_t at = 0;

        for (const Ptr &child : children()) {
            if (!child->visible()) {
                continue;
            }

            total = std::max(total, child->wantedHeight(type, widths[at++]));
        }
    }

    return std::max(minHeight, total + _top + _bottom);
}

// What each visible child gets along the main axis, in order. `across` is the
// room the other axis has, which is what a height is measured against.
std::vector<double> Box::share(Typeface &type, const double room, const double across) const {
    std::vector<double> mains;
    std::vector<double> least;
    double taken = gapTotal();
    double weight = 0.0;

    for (const Ptr &child : children()) {
        if (!child->visible()) {
            continue;
        }

        const double main = _flow == Flow::Row
            ? child->wantedWidth(type)
            : child->wantedHeight(type, child->fixedWidth >= 0.0 ? child->fixedWidth : across);

        mains.push_back(main);
        least.push_back(_flow == Flow::Row ? child->minWidth : child->minHeight);

        taken += main;
        weight += child->stretch;
    }

    const double spare = room - taken;

    if (spare == 0.0 || mains.empty()) {
        return mains;
    }

    size_t at = 0;

    for (const Ptr &child : children()) {
        if (!child->visible()) {
            continue;
        }

        if (spare > 0.0 && weight > 0.0) {
            mains[at] += spare * (child->stretch / weight);
        } else if (spare < 0.0 && weight > 0.0) {
            mains[at] = std::max(least[at], mains[at] + (spare * (child->stretch / weight)));
        } else if (spare < 0.0 && taken > 0.0) {
            mains[at] = std::max(least[at], mains[at] * (std::max(0.0, room) / taken));
        }

        ++at;
    }

    return mains;
}

void Box::arrange(Typeface &type) {
    const double innerX = _box.x + _left;
    const double innerY = _box.y + _top;
    const double innerW = std::max(0.0, _box.w - _left - _right);
    const double innerH = std::max(0.0, _box.h - _top - _bottom);

    const bool horizontal = _flow == Flow::Row;
    const double room = horizontal ? innerW : innerH;

    // The same division naturalHeight measured against, so a child is never laid
    // out at a width it was not measured at.
    const std::vector<double> mains = share(type, room, horizontal ? innerH : innerW);

    std::vector<Slot> slots;
    double weight = 0.0;
    size_t which = 0;

    for (const Ptr &child : children()) {
        if (!child->visible()) {
            continue;
        }

        slots.push_back(Slot{.who = child.get(), .main = mains[which++]});

        weight += child->stretch;
    }

    double total = gapTotal();

    for (const Slot &slot : slots) {
        total += slot.main;
    }

    const double spare = room - total;

    double at = horizontal ? innerX : innerY;

    if (spare > 0.0 && weight <= 0.0) {
        if (_align == Place::Centre) {
            at += (room - total) / 2.0;
        } else if (_align == Place::End) {
            at += room - total;
        }
    }

    for (Slot  const&slot : slots) {
        Widget *who = slot.who;
        const double acrossRoom = horizontal ? innerH : innerW;

        double across = acrossRoom;

        if (horizontal) {
            if (who->fixedHeight >= 0.0) {
                across = who->fixedHeight;
            } else if (_cross != Place::Fill) {
                across = std::min(acrossRoom, who->naturalHeight(type, slot.main));
            }
        } else {
            if (who->fixedWidth >= 0.0) {
                across = who->fixedWidth;
            } else if (_cross != Place::Fill) {
                across = std::min(acrossRoom, who->naturalWidth(type));
            }
        }

        double offset = 0.0;

        if (_cross == Place::Centre) {
            offset = (acrossRoom - across) / 2.0;
        } else if (_cross == Place::End) {
            offset = acrossRoom - across;
        }

        if (horizontal) {
            who->place(BLRect{at, innerY + offset, slot.main, across}, type);
        } else {
            who->place(BLRect{innerX + offset, at, across, slot.main}, type);
        }

        at += slot.main + _spacing;
    }
}

}
