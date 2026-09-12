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

#include <utility>

#include "gui/draw/Typeface.h"
#include "gui/toolkit/controls/Chip.h"

namespace toolkit {

namespace {

constexpr double PAD = 7.0;
constexpr double LIFT = 3.0;

}

Chip::Chip(std::string text, std::string about) : _text(std::move(text)) {
    _takesPointer = true;
    hint = std::move(about);
}

void Chip::setText(std::string text) {
    if (_text == text) {
        return;
    }

    _text = std::move(text);

    invalidate();
}

void Chip::setTight(const bool value) {
    if (_tight == value) {
        return;
    }

    _tight = value;

    invalidate();
}

Chip *Chip::plain() {
    _mono = false;

    return this;
}

const BLFont &Chip::face(Typeface &type) const {
    return type.at(_mono ? Typeface::mono : Typeface::regular, Theme::fontSmall);
}

double Chip::naturalWidth(Typeface &type) {
    if (fixedWidth >= 0.0) {
        return fixedWidth;
    }

    return type.width(face(type), _text) + (PAD * 2.0);
}

double Chip::naturalHeight(Typeface &type, const double /*width*/) {
    if (fixedHeight >= 0.0) {
        return fixedHeight;
    }

    return type.lineHeight(face(type)) + (LIFT * 2.0);
}

void Chip::paint(const Painter &painter) {
    if (_text.empty()) {
        return;
    }

    const Theme::Palette &palette = Theme::of();
    const BLRgba32 ink = _tight    ? palette.warning
                       : hovered() ? palette.text
                                   : palette.faint;

    if (_tight || hovered()) {
        painter.round(_box, Theme::radiusSmall, _tight ? palette.warningSoft : palette.raised);
        painter.outline(_box, Theme::radiusSmall, 1.0, Theme::alpha(ink, 0.3));
    }

    painter.label(painter.font(_mono ? Typeface::mono : Typeface::regular, Theme::fontSmall),
                  BLRect{_box.x + PAD, _box.y, _box.w - (PAD * 2.0), _box.h}, Align::Start,
                  _text, ink);
}

}
