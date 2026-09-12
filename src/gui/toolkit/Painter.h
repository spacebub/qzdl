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
#include <string>
#include <string_view>
#include <vector>

#include <blend2d/blend2d.h>

#include "gui/draw/Typeface.h"

namespace toolkit {

// One folded line, and where it came from in the run it was folded out of, so a
// selection over the lines can be read back off the run itself.
struct Fold {
    std::string text;
    size_t from = 0;
    size_t to = 0;
};

// Word wrapping, as much of it as the interface asks for: break on spaces and on
// newlines, and a word wider than the line is broken wherever it lands.
std::vector<Fold> foldSpans(Typeface &type, const BLFont &font, std::string_view run,
                            double room);

std::vector<std::string> fold(Typeface &type, const BLFont &font, std::string_view run,
                              double room);

// How tall `run` wraps to at `room` wide.
double wrapHeight(Typeface &type, const BLFont &font, std::string_view run, double room);

enum class Align : std::uint8_t {
    Start,
    Centre,
    End,
};

// The drawing vocabulary of the interface: a rectangle, a run of type, a path and
// an icon, since a rasteriser offers none of them and a widget needs all four.
class Painter {
public:
    Painter(BLContext &context, Typeface &type, const BLRectI &clip)
        : _context(context), _type(type), _clip(clip) {}

    BLContext &context() const { return _context; }

    Typeface &type() const { return _type; }

    const BLRectI &clip() const { return _clip; }

    // False when nothing of `box` is in the damaged rectangle being painted.
    bool needed(const BLRect &box) const;

    void fill(const BLRect &box, BLRgba32 tone) const;
    void round(const BLRect &box, double radius, BLRgba32 tone) const;

    // Drawn inside the box rather than straddling its edge.
    void outline(const BLRect &box, double radius, double width, BLRgba32 tone) const;

    void circle(BLPoint centre, double radius, BLRgba32 tone) const;

    void path(const BLPath &shape, BLRgba32 tone) const;
    void stroke(const BLPath &shape, double width, BLRgba32 tone) const;

    const BLFont &font(int weight, float size) const;

    double width(const BLFont &font, std::string_view run) const;
    double lineHeight(const BLFont &font) const;

    std::string elide(const BLFont &font, std::string_view run, double room) const;

    // Baseline worked out from the face; `top` is the top of the line box.
    void text(const BLFont &font, BLPoint top, std::string_view run, BLRgba32 tone) const;

    // Vertically centred in `box`, and placed across it by `align`. Elided to fit.
    void label(const BLFont &font, const BLRect &box, Align align, std::string_view run,
               BLRgba32 tone) const;

    void tracked(const BLFont &font, BLPoint top, std::string_view run, BLRgba32 tone,
                 double spacing) const;

    // Wrapped at `box.w`, from the top; answers the height it took.
    double paragraph(const BLFont &font, const BLRect &box, std::string_view run,
                     BLRgba32 tone) const;

    // How tall `run` wraps to at `room` wide, drawing nothing.
    double wrapHeight(const BLFont &font, std::string_view run, double room) const;

    // Narrows both the rasteriser's clip and the rectangle `needed` answers for,
    // so a row scrolled out of view is skipped rather than drawn and thrown away.
    void push(const BLRect &box) const;
    void pop() const;

private:
    BLContext &_context;
    Typeface &_type;

    mutable BLRectI _clip;
    mutable std::vector<BLRectI> _held;
};

}
