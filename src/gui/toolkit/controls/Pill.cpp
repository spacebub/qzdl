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

#include "gui/draw/Glyphs.h"
#include "gui/draw/Typeface.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/controls/Pill.h"

namespace toolkit {

Pill::Pill(std::string text) : _text(std::move(text)) {}

void Pill::setText(std::string text) {
    if (_text == text) {
        return;
    }

    _text = std::move(text);

    invalidate();
}

Pill *Pill::kind(std::string value) {
    _kind = std::move(value);

    return this;
}

Pill *Pill::dot(const bool value) {
    _dot = value;

    return this;
}

Pill *Pill::glyph(std::string name) {
    _glyph = std::move(name);

    return this;
}

Pill *Pill::tones(const BLRgba32 tone, const BLRgba32 wash) {
    _tone = tone;
    _wash = wash;
    _toneDark = Theme::dark();
    _set = true;

    return this;
}

BLRgba32 Pill::tone() const {
    if (_set) {
        return Theme::restated(_tone, _toneDark);
    }

    const Theme::Palette &palette = Theme::of();

    if (_kind == "success") {
        return palette.success;
    }

    if (_kind == "warning") {
        return palette.warning;
    }

    if (_kind == "danger") {
        return palette.danger;
    }

    if (_kind == "muted") {
        return palette.muted;
    }

    return palette.accent;
}

BLRgba32 Pill::wash() const {
    if (_set) {
        return Theme::restated(_wash, _toneDark);
    }

    const Theme::Palette &palette = Theme::of();

    if (_kind == "success") {
        return palette.successSoft;
    }

    if (_kind == "warning") {
        return palette.warningSoft;
    }

    if (_kind == "danger") {
        return palette.dangerSoft;
    }

    if (_kind == "muted") {
        return palette.mutedSoft;
    }

    return palette.accentSoft;
}

double Pill::naturalWidth(Typeface &type) {
    if (fixedWidth >= 0.0) {
        return fixedWidth;
    }

    double content = _glyph.empty() ? type.width(type.at(600, Theme::fontSmall), _text)
                                    : 12.0 * 1.4;

    if (_dot) {
        content += 8.0 + 6.0;
    }

    return content + 22.0;
}

void Pill::paint(const Painter &painter) {
    const BLRgba32 ink = Theme::of().dark ? tone() : Theme::darker(tone(), 0.35);
    const double radius = _box.h / 2.0;

    painter.round(_box, radius, wash());
    painter.outline(_box, radius, 1.0, Theme::alpha(ink, 0.3));

    const BLFont &face = painter.font(600, Theme::fontSmall);
    const double label = _glyph.empty() ? painter.width(face, _text) : 12.0 * 1.4;
    const double content = label + (_dot ? 8.0 + 6.0 : 0.0);

    double x = _box.x + ((_box.w - content) / 2.0);

    if (_dot) {
        painter.circle(BLPoint{x + 4.0, _box.y + (_box.h / 2.0)}, 4.0, ink);

        x += 8.0 + 6.0;
    }

    if (_glyph.empty()) {
        painter.label(face, BLRect{x, _box.y, label + 2.0, _box.h}, Align::Start, _text, ink);
    } else {
        Glyphs::draw(painter.context(), _glyph.c_str(),
                     BLPoint{x, _box.y + ((_box.h - (12.0 * 1.4)) / 2.0)}, 1.4F, ink);
    }
}

}
