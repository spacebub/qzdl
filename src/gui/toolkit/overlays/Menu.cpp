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

#include "gui/draw/Glyphs.h"
#include "gui/draw/Theme.h"
#include "gui/toolkit/overlays/Menu.h"

namespace toolkit {

// --- Menu ----------------------------------------------------------------------

Menu::Menu(std::vector<Menu::Row> rows, std::function<void(const std::string &)> triggered)
    : _rows(std::move(rows)), _triggered(std::move(triggered)) {
    _takesPointer = true;
    cursor = Cursor::Pointer;
}

double Menu::heightOf(const std::vector<Menu::Row> &rows) {
    double tall = 10.0;

    for (const Menu::Row &row : rows) {
        tall += row.separator ? RULE : ROW;
    }

    return tall;
}

int Menu::rowAt(const double y) const {
    double top = _box.y + 5.0;

    for (size_t row = 0; row < _rows.size(); ++row) {
        const double tall = _rows[row].separator ? RULE : ROW;

        if (y >= top && y < top + tall) {
            return static_cast<int>(row);
        }

        top += tall;
    }

    return -1;
}

void Menu::paint(const Painter &painter) {
    const Theme::Palette &palette = Theme::of();

    painter.round(_box, Theme::radiusSmall, palette.raised);
    painter.outline(_box, Theme::radiusSmall, 1.0, palette.borderStrong);

    double top = _box.y + 5.0;

    for (size_t index = 0; index < _rows.size(); ++index) {
        const Menu::Row &row = _rows[index];
        const double tall = row.separator ? RULE : ROW;
        const BLRect line{_box.x + 5.0, top, _box.w - 10.0, tall};

        top += tall;

        if (row.separator) {
            painter.fill(BLRect{line.x + 4.0, line.y + ((tall - 1.0) / 2.0), line.w - 8.0, 1.0},
                         palette.border);

            continue;
        }

        const bool usable = !row.disabled;

        if (usable && std::cmp_equal(index, _over)) {
            painter.round(line, Theme::radiusSmall, palette.hover);
        }

        double x = line.x + 10.0;

        if (!row.glyph.empty()) {
            constexpr float weight = 1.0F;
            const double side = Glyphs::span(weight);

            Glyphs::draw(painter.context(), row.glyph.c_str(),
                         BLPoint{x, line.y + ((line.h - side) / 2.0)}, weight,
                         row.danger ? palette.danger : palette.faint);

            x += side + 9.0;
        }

        painter.label(painter.font(400, Theme::fontBody),
                      BLRect{x, line.y, line.x + line.w - 10.0 - x, line.h}, Align::Start,
                      row.label,
                      Theme::alpha(row.danger ? palette.danger : palette.text,
                                   usable ? 1.0 : 0.4));
    }
}

bool Menu::press(const Pointer &at) {
    return holds(at.x, at.y);
}

void Menu::release(const Pointer &at) {
    const int row = rowAt(at.y);

    if (row < 0) {
        return;
    }

    const Menu::Row &picked = _rows[static_cast<size_t>(row)];

    if (picked.separator || picked.disabled || !_triggered) {
        return;
    }

    // Everything is copied out first: the handler closes this menu, which frees the
    // callable being run and the row it came from.
    const std::string action = picked.action;
    const std::function<void(const std::string &)> fire = _triggered;

    fire(action);
}

void Menu::hover(const Pointer &at) {
    const int over = rowAt(at.y);

    if (over != _over) {
        _over = over;

        invalidate();
    }
}

void Menu::leave() {
    Widget::leave();

    _over = -1;
}

}
