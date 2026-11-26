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
#include <string>
#include <utility>

#include "ttk/draw/Glyphs.h"
#include "ttk/draw/Theme.h"
#include "ttk/draw/Typeface.h"
#include "ttk/system/Text.h"

#include "gui/draw/Band.h"

using namespace ttk;

namespace Band {

void paint(const Painter &painter, const BLRect &box, const Header &header) {
    const Theme::Palette &palette = Theme::palette();
    const BLRect button = menuBox(box);

    if (header.named) {
        const double side = Glyphs::span(1.0F);

        Glyphs::draw(painter.context(),
                     Glyphs::Glyph::Chevron,
                     BLPoint{box.x + ((LANE - side) / 2.0), box.y + ((box.h - side) / 2.0)},
                     1.0F,
                     header.over ? palette.text : palette.faint,
                     header.open * 180.0F);
    }

    const BLFont &caption = painter.font(600, Theme::fontTiny);
    const BLFont &small = painter.font(400, Theme::fontTiny);
    const std::string name = Text::upper(header.name);
    const double left = box.x + header.indent;
    const double right = header.menu ? button.x : box.x + box.w - 12.0;
    const double base = painter.baseline(caption, box);
    const double after = left + painter.type().width_tracked(caption, name, 0.9F) + 8.0;
    const double rule = after + painter.width(small, header.count) + 10.0;

    painter.tracked(caption,
                    BLPoint{left, base - caption.metrics().ascent},
                    name,
                    header.lit    ? palette.accent
                    : header.over ? palette.muted
                                  : palette.faint,
                    0.9);
    painter.row_on(small, BLPoint{after, base}, right - after, header.count, palette.faint);
    const double middle = std::floor(base - (caption.metrics().cap_height / 2.0));

    painter.fill(BLRect{rule, middle, std::max(0.0, right - rule), 1.0},
                 header.lit    ? palette.accent
                 : header.over ? palette.borderStrong
                               : palette.border);

    if (header.menu) {
        const double span = Glyphs::span(1.0F);

        Glyphs::draw(painter.context(),
                     Glyphs::Glyph::Dots,
                     BLPoint{button.x + ((BUTTON - span) / 2.0), box.y + ((box.h - span) / 2.0)},
                     1.0F,
                     header.overMenu ? palette.text : palette.muted);
    }
}

BLRect menuBox(const BLRect &box) {
    return BLRect{box.x + box.w - 4.0 - BUTTON, box.y, 4.0 + BUTTON, box.h};
}

bool onMenu(const BLRect &box, const double x, const double y) {
    const BLRect button = menuBox(box);

    return x >= button.x && x < button.x + button.w && y >= button.y && y < button.y + button.h;
}

void Folds::turn(const int group, const bool collapsed, const double now, std::function<void(bool)> fold) {
    auto slide = std::ranges::find(_slides, group, &Slide::group);

    if (slide == _slides.end()) {
        slide = _slides.insert(_slides.end(), Slide{.group = group, .open = !collapsed, .shown = {}, .fold = {}});
        slide->shown.set(collapsed ? 0.0F : 1.0F);
    }

    slide->open = !slide->open;
    slide->fold = std::move(fold);

    // The timing of every fold in the toolkit.
    slide->shown.run(slide->open ? 1.0F : 0.0F, now, 0.22, Anim::Curve::CubicOut);

    if (slide->open && collapsed) {
        slide->fold(false);
    }
}

float Folds::open(const int group, const bool collapsed) const {
    const auto slide = std::ranges::find(_slides, group, &Slide::group);

    if (slide != _slides.end()) {
        return slide->shown.value();
    }

    return collapsed ? 0.0F : 1.0F;
}

bool Folds::advance(const double now) {
    bool live = false;

    for (Slide &slide : _slides) {
        if (!slide.shown.live()) {
            continue;
        }

        slide.shown.advance(now);
        live = live || slide.shown.live();

        if (!slide.shown.live() && !slide.open) {
            slide.fold(true);
        }
    }

    std::erase_if(_slides, [](const Slide &slide) { return slide.open && !slide.shown.live(); });

    return live;
}

void Folds::settled() {
    std::erase_if(_slides, [](const Slide &slide) { return !slide.shown.live(); });
}

}
