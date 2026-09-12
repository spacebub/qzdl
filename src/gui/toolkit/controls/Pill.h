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

#include <string>

#include "gui/draw/Theme.h"
#include "gui/toolkit/Widget.h"

namespace toolkit {

class Pill : public Widget {

public:
    enum class Kind : std::uint8_t {
        None, // defaults to Theme::Palette.accent
        Muted,
        Success,
        Warning,
        Danger,
    };

    explicit Pill(std::string text = {});

    void setText(std::string text);

    // muted | warning | danger; anything else is the accent.
    Pill *kind(Kind value);
    Pill *dot(bool value);
    Pill *glyph(Glyphs::Glyph glyph);
    Pill *tones(BLRgba32 tone, BLRgba32 wash);

    double naturalWidth(Typeface &type) override;
    double naturalHeight(Typeface & /*type*/, double /*width*/) override { return 26.0; }

    void paint(const Painter &painter) override;

private:
    [[nodiscard]] BLRgba32 tone() const;
    [[nodiscard]] BLRgba32 wash() const;

    std::string _text;
    Kind _kind{};
    Glyphs::Glyph _glyph{};

    BLRgba32 _tone{};
    BLRgba32 _wash{};
    bool _set = false;
    bool _toneDark = Theme::dark();

    bool _dot = true;
};

}
