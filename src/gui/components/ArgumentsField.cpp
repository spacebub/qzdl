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
#include <utility>

#include "ttk/draw/Theme.h"
#include "ttk/draw/Typeface.h"
#include "ttk/toolkit/Painter.h"
#include "ttk/toolkit/controls/Label.h"

#include "gui/components/ArgumentsField.h"

using namespace ttk;

namespace components {

namespace {

constexpr double INSET = 12.0;

// Room past the longest common prefix, so it does not sit against the divider.
constexpr double SLACK = 24.0;

}

ArgumentsField::ArgumentsField(std::string caption, Edited prefix, Edited extra) : Box(Flow::Column) {
    spacing(6.0);

    minWidth = 320.0;

    append(std::make_unique<Label>(std::move(caption)))->section();

    _row = append(Box::row());
    _row->fixedHeight = Theme::control;

    _prefix = _row->append(std::make_unique<TextBox>(std::move(prefix)));
    _prefix->placeholder("Prefix")->mono();

    _extra = _row->append(std::make_unique<TextBox>(std::move(extra)));
    _extra->placeholder("Arguments")->mono();
}

void ArgumentsField::set(const std::string &prefix, const std::string &extra) const {
    if (_prefix->text() != prefix) {
        _prefix->set_text(prefix);
    }

    if (_extra->text() != extra) {
        _extra->set_text(extra);
    }
}

void ArgumentsField::arrange(Typeface &type) {
    Box::arrange(type);

    const BLRect row = _row->box();
    const double inner = Theme::control - 12.0;
    const double top = row.y + ((row.h - inner) / 2.0);
    const double wide = type.width(type.at(Typeface::mono, Theme::fontBody), "gamemoderun") + SLACK;

    _frame = row;

    // On a whole pixel, or the rule smears across two at partial strength.
    _divider = std::round(_frame.x + INSET + wide + 9.0);

    const double left = _divider + 9.0;

    _prefix->place(BLRect{_frame.x + INSET, top, wide, inner}, type);
    _extra->place(BLRect{left, top, std::max(0.0, _frame.x + _frame.w - INSET - left), inner}, type);
}

void ArgumentsField::paint(const Painter &painter) {
    const Theme::Palette &palette = Theme::palette();
    const bool focused = _prefix->focused() || _extra->focused();

    painter.round(_frame, Theme::radiusSmall, palette.field);
    painter.outline(_frame, Theme::radiusSmall, 1.0, focused ? palette.accent : palette.borderStrong);
    painter.fill(BLRect{_divider, _frame.y + 6.0, 1.0, _frame.h - 12.0}, palette.borderStrong);

    Box::paint(painter);
}

}
