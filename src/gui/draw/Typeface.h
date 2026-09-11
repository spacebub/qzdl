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

#include <map>
#include <string>
#include <string_view>
#include <unordered_map>

#include <blend2d/blend2d.h>

// Type, such as Blend2D gives it: a face off the filesystem, a size, and glyphs
// filled as paths. There is no font engine to configure and no atlas -- but also
// no hinting, and nothing here goes anywhere near DirectWrite. See the README.
class Typeface {
public:
    // Weights, as the application asks for them.
    static constexpr int regular = 400;
    static constexpr int semibold = 600;
    static constexpr int bold = 700;

    // The fixed width face, for paths, command lines and run output.
    static constexpr int mono = 1;
    static constexpr int monoBold = 2;

    // The weight to ask at() for, given what a label wants.
    static int pick(int weight, bool fixed);

    // False when the platform has no face this can find, which the caller turns
    // into a clean exit rather than a crash.
    bool load();

    const BLFont &at(int weight, float size);

    // A run is shaped once and kept, and rasterised once into a mask the first time
    // it is drawn: a label is measured, elided and drawn every paint, and Blend2D
    // would otherwise shape it and fill every glyph outline again each time.

    // The advance width, which is what a layout needs; the ink may be narrower.
    float width(const BLFont &font, std::string_view run);

    // `run` shortened until it fits, with an ellipsis where anything was dropped.
    // Measured with the tracking it will be drawn with, when there is any.
    std::string elide(const BLFont &font, std::string_view run, float room,
                      float tracking = 0.0F);

    // `top` is the top of the line box; the baseline is worked out from the face.
    void draw(BLContext &context, const BLFont &font, BLPoint top, std::string_view run,
              BLRgba32 tone);

    // Letter-spaced, which fill_utf8_text has no notion of: the run is drawn one
    // character at a time with the tracking added to each advance.
    float widthTracked(const BLFont &font, std::string_view run, float tracking);

    void drawTracked(BLContext &context, const BLFont &font, BLPoint top, std::string_view run,
                     BLRgba32 tone, float tracking);

    // Centred vertically inside a box `height` tall starting at `top`.
    void drawCentred(BLContext &context, const BLFont &font, BLPoint top, float height,
                     std::string_view run, BLRgba32 tone);

    [[nodiscard]] float lineHeight(const BLFont &font) const;

private:
    struct Shaped {
        BLGlyphBuffer buffer;
        float width = 0.0F;

        // The ink, relative to the origin on the baseline.
        BLBox ink{};

        // Coverage only; the tone is applied when it is laid down.
        BLImage mask;
        BLPointI maskAt{};
        bool masked = false;
    };

    Shaped &shaped(const BLFont &font, std::string_view run);

    // Lays a run down at `origin` on the baseline, from its mask.
    void lay(BLContext &context, const BLFont &font, Shaped &made, BLPoint origin,
             BLRgba32 tone);

    // Faces are held by weight; a size makes a BLFont out of one.
    std::map<int, BLFontFace> _faces;
    std::map<long long, BLFont> _fonts;

    // Keyed by the font's address, which the map above keeps still, and the run.
    std::unordered_map<std::string, Shaped> _shaped;
    size_t _maskBytes = 0;
};
