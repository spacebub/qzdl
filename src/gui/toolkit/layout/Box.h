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

#include <cstdint>
#include <memory>
#include <vector>

#include "gui/toolkit/Widget.h"

namespace toolkit {

// A row or a column, which between them are every layout in the interface.
//
// A child takes its natural size along the main axis unless it carries a stretch,
// in which case it shares what is left over. Across the axis it fills, unless the
// layout says otherwise or the child asks for a width of its own.
class Box : public Widget {
public:
    enum class Flow : std::uint8_t {
        Row,
        Column,
    };

    enum class Place : std::uint8_t {
        Fill,
        Start,
        Centre,
        End,
    };

    explicit Box(const Flow flow) : _flow(flow) {}

    static std::unique_ptr<Box> row() { return std::make_unique<Box>(Flow::Row); }

    static std::unique_ptr<Box> column() { return std::make_unique<Box>(Flow::Column); }

    Box *spacing(double value);
    Box *pad(double all);
    Box *pad(double sides, double ends);
    Box *pad(double left, double top, double right, double bottom);

    // Where the children sit when they do not fill the main axis.
    Box *align(Place where);

    // Where a child sits across the axis.
    Box *cross(Place where);

    Box *grow(double weight);

    double naturalWidth(Typeface &type) override;
    double naturalHeight(Typeface &type, double width) override;

    void arrange(Typeface &type) override;

private:
    struct Slot {
        Widget *who = nullptr;
        double main = 0.0;
    };

protected:
    [[nodiscard]] double gapTotal() const;

    // What each visible child gets along the main axis, in order.
    std::vector<double> share(Typeface &type, double room, double across) const;

private:

protected:
    void setFlow(const Flow flow) { _flow = flow; }

private:
    Flow _flow;

    double _spacing = 0.0;

    double _left = 0.0;
    double _top = 0.0;
    double _right = 0.0;
    double _bottom = 0.0;

    Place _align = Place::Fill;
    Place _cross = Place::Fill;
};

}
