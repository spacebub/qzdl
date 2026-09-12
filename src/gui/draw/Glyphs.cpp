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

#include <array>
#include <cmath>
#include <map>
#include <numbers>
#include <string>
#include <vector>

#include "gui/draw/Glyphs.h"
#include "gui/draw/Svg.h"

namespace {

// The outline path, the filled path, the viewbox each is drawn in, and the pen. A
// glyph may have both -- System is a stroked circle with a filled half.
struct Shape {
    const char *outline;
    const char *solid;
    float box;
    float solidBox;
    float pen;
};

// The default pen, overridden per glyph below.
constexpr float PEN = 1.5F;

// Indexed by the enum: rodata, with nothing to build at start-up and no lookup
// beyond the subscript.
constexpr std::array<Shape, static_cast<size_t>(Glyphs::Glyph::Count)> SHAPES = [] {
    std::array<Shape, static_cast<size_t>(Glyphs::Glyph::Count)> table{};

    const auto set = [&table](const Glyphs::Glyph glyph, const Shape &shape) {
        table[static_cast<size_t>(glyph)] = shape;
    };

    set(Glyphs::Glyph::Close,
        {.outline = "M 1.9 1.9 L 10.1 10.1 M 10.1 1.9 L 1.9 10.1",
         .solid = nullptr, .box = 12.0F, .solidBox = 0.0F, .pen = PEN});

    set(Glyphs::Glyph::Cross,
        {.outline = "M 1.9 1.9 L 10.1 10.1 M 10.1 1.9 L 1.9 10.1",
         .solid = nullptr, .box = 12.0F, .solidBox = 0.0F, .pen = PEN});

    set(Glyphs::Glyph::Plus,
        {.outline = "M 0.8 6 L 11.2 6 M 6 0.8 L 6 11.2",
         .solid = nullptr, .box = 12.0F, .solidBox = 0.0F, .pen = 1.6F});

    set(Glyphs::Glyph::Search,
        {.outline = "M 8.25 4.75 A 3.5 3.5 0 1 1 1.25 4.75 A 3.5 3.5 0 1 1 8.25 4.75"
                    " M 7.6 9.15 L 10.71 12.26",
         .solid = nullptr, .box = 12.0F, .solidBox = 0.0F, .pen = PEN});

    set(Glyphs::Glyph::Download,
        {.outline = "M 7 1 L 7 9.5 M 3.2 6 L 7 9.8 L 10.8 6 M 2 12.6 L 12 12.6",
         .solid = nullptr, .box = 14.0F, .solidBox = 0.0F, .pen = 1.6F});

    set(Glyphs::Glyph::Save,
        {.outline = "M 1.8 2.2 L 9.2 2.2 L 12.2 5.2 L 12.2 11.8 L 1.8 11.8 Z"
                    " M 4.4 2.2 L 4.4 5.6 L 8.8 5.6 L 8.8 2.2 M 4 11.8 L 4 8.2 L 10 8.2 L 10 11.8",
         .solid = nullptr, .box = 14.0F, .solidBox = 0.0F, .pen = PEN});

    set(Glyphs::Glyph::Extract,
        {.outline = "M 6.5 1.5 L 1.5 1.5 L 1.5 12.5 L 6.5 12.5 M 5 7 L 12.5 7"
                    " M 9.2 3.8 L 12.6 7 L 9.2 10.2",
         .solid = nullptr, .box = 14.0F, .solidBox = 0.0F, .pen = PEN});

    set(Glyphs::Glyph::Trash,
        {.outline = "M 1.5 3.5 L 12.5 3.5 M 5.2 3.3 L 5.2 1.6 L 8.8 1.6 L 8.8 3.3"
                    " M 3 3.8 L 3.7 12.6 L 10.3 12.6 L 11 3.8 M 5.8 6 L 6 10.4 M 8.2 6 L 8 10.4",
         .solid = nullptr, .box = 14.0F, .solidBox = 0.0F, .pen = PEN});

    set(Glyphs::Glyph::Edit,
        {.outline = "M 2 12 L 2 9.4 L 9.4 2 L 12 4.6 L 4.6 12 Z",
         .solid = nullptr, .box = 14.0F, .solidBox = 0.0F, .pen = PEN});

    set(Glyphs::Glyph::Up,
        {.outline = "M 6.5 12 L 6.5 1.8 M 2 6.2 L 6.5 1.8 L 11 6.2",
         .solid = nullptr, .box = 13.0F, .solidBox = 0.0F, .pen = 1.6F});

    set(Glyphs::Glyph::Down,
        {.outline = "M 6.5 1.8 L 6.5 12 M 2 7.6 L 6.5 12 L 11 7.6",
         .solid = nullptr, .box = 13.0F, .solidBox = 0.0F, .pen = 1.6F});

    set(Glyphs::Glyph::System,
        {.outline = "M 11.9 7 A 4.9 4.9 0 1 1 2.1 7 A 4.9 4.9 0 1 1 11.9 7",
         .solid = "M 7 11.9 A 4.9 4.9 0 0 1 7 2.1 Z", .box = 14.0F, .solidBox = 13.0F, .pen = PEN});

    set(Glyphs::Glyph::Light,
        {.outline = "M 10 7 A 3 3 0 1 1 4 7 A 3 3 0 1 1 10 7 M 11.8 7 L 13.3 7"
                    " M 10.394 10.394 L 11.455 11.455 M 7 11.8 L 7 13.3"
                    " M 3.606 10.394 L 2.545 11.455 M 2.2 7 L 0.7 7"
                    " M 3.606 3.606 L 2.545 2.545 M 7 2.2 L 7 0.7"
                    " M 10.394 3.606 L 11.455 2.545",
         .solid = nullptr, .box = 14.0F, .solidBox = 0.0F, .pen = PEN});

    set(Glyphs::Glyph::Dark,
        {.outline = "M 8.98 2.41 A 5 5 0 1 1 2.41 8.98 A 5 5 0 0 0 8.98 2.41 Z",
         .solid = nullptr, .box = 14.0F, .solidBox = 0.0F, .pen = PEN});

    set(Glyphs::Glyph::Check,
        {.outline = "M 2 7.4 L 5.6 11 L 12 3.2",
         .solid = nullptr, .box = 14.0F, .solidBox = 0.0F, .pen = 1.8F});

    set(Glyphs::Glyph::Refresh,
        {.outline = "M 8.77 10.955 A 5 5 0 1 1 10.955 4.23",
         .solid = "M 9.8 1.6 L 10.6 5.4 L 6.9 4.4 Z", .box = 13.0F, .solidBox = 13.0F, .pen = 1.6F});

    set(Glyphs::Glyph::Cog,
        {.outline = "M 10.9 7 A 3.9 3.9 0 1 1 3.1 7 A 3.9 3.9 0 1 1 10.9 7 M 10.9 7 L 13.2 7"
                    " M 9.758 9.758 L 11.384 11.384 M 7 10.9 L 7 13.2"
                    " M 4.242 9.758 L 2.616 11.384 M 3.1 7 L 0.8 7"
                    " M 4.242 4.242 L 2.616 2.616 M 7 3.1 L 7 0.8"
                    " M 9.758 4.242 L 11.384 2.616",
         .solid = nullptr, .box = 14.0F, .solidBox = 0.0F, .pen = PEN});

    set(Glyphs::Glyph::Terminal,
        {.outline = "M 3 2 L 11 2 A 2 2 0 0 1 13 4 L 13 10 A 2 2 0 0 1 11 12 L 3 12"
                    " A 2 2 0 0 1 1 10 L 1 4 A 2 2 0 0 1 3 2 Z"
                    " M 3.6 5.6 L 5.8 7.4 L 3.6 9.2 M 7.4 9.4 L 10.6 9.4",
         .solid = nullptr, .box = 14.0F, .solidBox = 0.0F, .pen = 1.4F});

    set(Glyphs::Glyph::Copy,
        {.outline = "M 5.2 5.2 L 12.6 5.2 L 12.6 12.6 L 5.2 12.6 Z"
                    " M 8.8 4.6 L 8.8 1.4 L 1.4 1.4 L 1.4 8.8 L 4.6 8.8",
         .solid = nullptr, .box = 14.0F, .solidBox = 0.0F, .pen = PEN});

    set(Glyphs::Glyph::Folder,
        {.outline = nullptr,
         .solid = "M 1 3.5 L 5.5 3.5 L 7 5.4 L 13 5.4 L 13 12 L 1 12 Z", .box = 0.0F, .solidBox = 14.0F, .pen = PEN});

    set(Glyphs::Glyph::Play,
        {.outline = nullptr,
         .solid = "M 3.4 1.6 L 11.6 6.5 L 3.4 11.4 Z", .box = 0.0F, .solidBox = 13.0F, .pen = PEN});

    // These draw in a 15 unit box of their own.
    set(Glyphs::Glyph::File,
        {.outline = "M 3 1.5 L 9.5 1.5 L 12.5 4.8 L 12.5 13.5 L 3 13.5 Z M 9.3 1.7 L 9.3 5 L 12.3 5",
         .solid = nullptr, .box = 15.0F, .solidBox = 0.0F, .pen = 1.2F});

    set(Glyphs::Glyph::FileFolder,
        {.outline = nullptr,
         .solid = "M 1 4 L 5.5 4 L 7 5.8 L 14 5.8 L 14 13 L 1 13 Z", .box = 0.0F, .solidBox = 15.0F, .pen = PEN});

    return table;
}();

// Parsing is cheap but not free, and the title bar redraws on every hover frame.
// The paths are kept in viewbox units and scaled by the context.
const BLPath &cached(const char *commands) {
    static std::map<const char *, BLPath> paths;

    const auto found = paths.find(commands);

    if (found != paths.end()) {
        return found->second;
    }

    BLPath path;

    Svg::parse(commands, path);

    return paths.emplace(commands, std::move(path)).first->second;
}

// The rounded bar, which is a shape rather than a path.
void bar(BLContext &context, const BLRect &rect, const double radius, const BLRgba32 tone) {
    context.fill_round_rect(rect, radius, radius, tone);
}

// An axis-aligned edge belongs on the pixel grid. Left where it falls, the minimize
// bar lands on a half pixel at each end and measures a pixel narrower than the
// square beside it -- which is exactly the sort of mismatch that reads as the
// glyphs being out of proportion with one another.
double snap(const double at) {
    return std::floor(at + 0.5);
}

}

float Glyphs::span(const float weight) {
    return element * weight;
}

void Glyphs::draw(BLContext &context, const Glyph glyph, const BLPoint origin,
                  const float weight, const BLRgba32 tone, const float turn) {
    const double side = element * weight;

    if (turn != 0.0F) {
        context.save();
        context.rotate(turn * std::numbers::pi / 180.0,
                       origin.x + (side * 0.5), origin.y + (side * 0.5));

        draw(context, glyph, origin, weight, tone);

        context.restore();

        return;
    }

    // Three dots in a row, and the two by three the drag handle is.
    if (glyph == Glyph::Dots || glyph == Glyph::Grip) {
        const bool grip = glyph == Glyph::Grip;
        const double dot = (grip ? 2.2 : 2.6) * weight;
        const double step = dot + ((grip ? 2.6 : 2.0) * weight);

        const int columns = grip ? 2 : 3;
        const int rows = grip ? 3 : 1;

        const double left = origin.x + ((side - ((columns * step) - (step - dot))) * 0.5);
        const double top = origin.y + ((side - ((rows * step) - (step - dot))) * 0.5);

        for (int column = 0; column < columns; ++column) {
            for (int row = 0; row < rows; ++row) {
                context.fill_circle(left + (column * step) + (dot * 0.5),
                                    top + (row * step) + (dot * 0.5), dot * 0.5, tone);
            }
        }

        return;
    }

    // The three window controls, built out of rectangles.
    if (glyph == Glyph::Minimize || glyph == Glyph::Minus) {
        const double wide = snap(11.0 * weight);
        const double tall = 1.5 * weight;

        // A flat 1px radius, not a half-height one: rounding
        // the ends any harder eats into the bar's drawn length.
        bar(context,
            {snap(origin.x + ((side - wide) * 0.5)), origin.y + ((side - tall) * 0.5), wide, tall},
            1.0, tone);

        return;
    }

    if (glyph == Glyph::Maximize) {
        const double box = 10.0 * weight;
        const double pen = 1.5 * weight;

        context.set_stroke_width(pen);

        // A stroke straddles the path, so the rectangle is
        // pulled in by half the pen to land in the same place.
        context.stroke_round_rect(
            BLRect{origin.x + ((side - box) * 0.5) + (pen * 0.5),
                   origin.y + ((side - box) * 0.5) + (pen * 0.5), box - pen, box - pen},
            2.0, 2.0, tone);

        return;
    }

    if (glyph == Glyph::Restore) {
        const double box = 9.0 * weight;
        const double pen = 1.5 * weight;
        const double step = 3.0 * weight;

        const double edge = box - pen;
        constexpr double round = 1.5;

        // The back one stops a pen short of the front one, which sits over it.
        const double gap = pen;

        const BLRect front{origin.x + (pen * 0.5), origin.y + step + (pen * 0.5), edge, edge};
        const BLRect back{origin.x + step + (pen * 0.5), origin.y + (pen * 0.5), edge, edge};

        BLPath behind;

        behind.move_to(back.x, front.y - gap);
        behind.line_to(back.x, back.y + round);
        behind.arc_quadrant_to(back.x, back.y, back.x + round, back.y);
        behind.line_to(back.x + back.w - round, back.y);
        behind.arc_quadrant_to(back.x + back.w, back.y, back.x + back.w, back.y + round);
        behind.line_to(back.x + back.w, back.y + back.h - round);
        behind.arc_quadrant_to(back.x + back.w, back.y + back.h, back.x + back.w - round,
                               back.y + back.h);
        behind.line_to(front.x + front.w + gap, back.y + back.h);

        context.save();
        context.set_stroke_width(pen);
        context.set_stroke_caps(BL_STROKE_CAP_BUTT);
        context.set_stroke_join(BL_STROKE_JOIN_ROUND);
        context.stroke_path(behind, tone);
        context.stroke_round_rect(front, 2.0, 2.0, tone);
        context.restore();

        return;
    }

    const auto at = static_cast<size_t>(glyph);

    if (at >= SHAPES.size()) {
        return;
    }

    const Shape &shape = SHAPES[at];

    if (shape.outline == nullptr && shape.solid == nullptr) {
        return;
    }

    if (shape.solid != nullptr) {
        const double drawn = shape.solidBox * weight;

        context.save();
        context.translate(origin.x + ((side - drawn) * 0.5), origin.y + ((side - drawn) * 0.5));
        context.scale(static_cast<double>(weight));
        context.fill_path(cached(shape.solid), tone);
        context.restore();
    }

    if (shape.outline == nullptr) {
        return;
    }

    const double drawn = shape.box * weight;

    context.save();
    context.translate(origin.x + ((side - drawn) * 0.5), origin.y + ((side - drawn) * 0.5));
    context.scale(static_cast<double>(weight));

    // Every outline is round-capped and round-joined.
    context.set_stroke_width(shape.pen);
    context.set_stroke_caps(BL_STROKE_CAP_ROUND);
    context.set_stroke_join(BL_STROKE_JOIN_ROUND);
    context.stroke_path(cached(shape.outline), tone);

    context.restore();
}

bool Glyphs::sheet(const char *path) {
    // Every glyph, table-driven and rectangle-built alike; Empty is skipped.
    std::vector<Glyph> names;

    for (size_t at = 1; at < static_cast<size_t>(Glyph::Count); ++at) {
        names.push_back(static_cast<Glyph>(at));
    }

    constexpr int CELL = 64;
    constexpr int COLUMNS = 8;

    const int rows = (static_cast<int>(names.size()) + COLUMNS - 1) / COLUMNS;

    BLImage out;

    if (out.create(CELL * COLUMNS, CELL * rows, BL_FORMAT_PRGB32) != BL_SUCCESS) {
        return false;
    }

    {
        BLContext context(out);

        context.fill_all(BLRgba32(0xff0a0d13));

        for (size_t at = 0; at < names.size(); ++at) {
            const int column = static_cast<int>(at) % COLUMNS;
            const int row = static_cast<int>(at) / COLUMNS;

            // Drawn large, where a rasteriser has nowhere to hide.
            constexpr float weight = 2.6F;
            const double side = span(weight);

            draw(context, names[at],
                 BLPoint{(column * CELL) + ((CELL - side) * 0.5), (row * CELL) + ((CELL - side) * 0.5)},
                 weight, BLRgba32(0xffe4eaf5));
        }
    }

    return out.write_to_file(path) == BL_SUCCESS;
}
