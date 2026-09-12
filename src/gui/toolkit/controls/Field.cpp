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

#include "gui/draw/Glyphs.h"
#include "gui/draw/Typeface.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/Field.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/controls/Pill.h"
#include "gui/toolkit/controls/TextBox.h"
#include "gui/toolkit/layout/Box.h"

namespace toolkit {

// --- Field ---------------------------------------------------------------------

Field::Field(std::string label, std::function<void(const std::string &)> edited)
    : Box(Flow::Column) {
    spacing(6.0);

    minWidth = 240.0;

    _caption_row = append(Box::row());

    Box *caption = _caption_row;

    caption->fixedHeight = 22.0;
    caption->cross(Place::Centre);

    _caption = caption->append(std::make_unique<Label>(std::move(label)));
    _caption->section();
    _caption->stretch = 1.0;

    _badge = caption->append(std::make_unique<Pill>());
    _badge->setVisible(false);
    _badge->fixedHeight = 22.0;

    caption->setVisible(!_caption->text().empty());

    _row = append(Box::row());
    _row->spacing(8.0);
    _row->fixedHeight = Theme::control;
    _row->cross(Place::Centre);

    _input = _row->append(std::make_unique<TextBox>(std::move(edited)));
    _input->stretch = 1.0;
    _input->fixedHeight = Theme::control - 12.0;

    _input->accepted = [this] {
        if (accepted) {
            accepted();
        }
    };

    _note = append(std::make_unique<Label>());
    _note->font(400, Theme::fontSmall)->tone(Theme::of().faint)->wrap();
    _note->setVisible(false);
}

Field *Field::placeholder(std::string text) {
    _input->placeholder(std::move(text));

    return this;
}

Field *Field::value(const std::string &text) {
    _input->setText(text);

    return this;
}

Field *Field::leadingGlyph(Glyphs::Glyph glyph) {
    _leadingGlyph = glyph;

    return this;
}

Field *Field::prefix(std::string text) {
    _prefix = std::move(text);

    return this;
}

Field *Field::mono(const bool value) {
    _input->mono(value);

    return this;
}

Field *Field::readOnly(const bool value) {
    _input->readOnly(value);

    return this;
}

Field *Field::note(std::string text) {
    _note->setText(std::move(text));
    _note->setVisible(!_note->text().empty());

    return this;
}

Field *Field::badge(std::string text, Pill::Kind kind) {
    const bool shown = !text.empty();

    _badge->setText(std::move(text));
    _badge->kind(kind);
    _badge->setVisible(shown);

    _caption_row->setVisible(!_caption->text().empty() || shown);

    return this;
}

Field *Field::icon(Glyphs::Glyph glyph, std::string hint, std::function<void()> pressed) {
    _icon = _row->append(std::make_unique<GlyphButton>(glyph, std::move(pressed)));
    _icon->size(30.0)->tooltip(std::move(hint));
    _icon->fixedWidth = 30.0;
    _icon->fixedHeight = 30.0;

    return this;
}

Field *Field::action(std::string label, std::function<void()> pressed) {
    _action = _row->append(std::make_unique<Button>(std::move(label), std::move(pressed)));
    _action->compact();

    return this;
}

void Field::setText(const std::string &text) {
    _input->setText(text);
}

void Field::takeFocus() {
    if (root() != nullptr) {
        root()->focus(_input);
        _input->selectAll();
    }
}

void Field::arrange(Typeface &type) {
    Box::arrange(type);

    // The box wraps the input, the leading glyph and the icon, but not the action
    // button beside it.
    const BLRect row = _row->box();
    double right = row.x + row.w;

    if (_action != nullptr && _action->visible()) {
        right = _action->box().x - 8.0;
    }

    _frame = BLRect{row.x, row.y, std::max(0.0, right - row.x), Theme::control};

    double left = _frame.x + 12.0;

    if (_leadingGlyph != Glyphs::Glyph::Empty) {
        left += (12.0 * 1.1) + 9.0;
    }

    if (!_prefix.empty()) {
        left += type.width(type.at(Typeface::mono, Theme::fontBody), _prefix) + 19.0;
    }

    const double inset = _icon != nullptr && _icon->visible() ? 42.0 : 12.0;

    _input->place(BLRect{left, _frame.y + 6.0, std::max(0.0, _frame.x + _frame.w - inset - left),
                         _frame.h - 12.0},
                  type);

    if (_icon != nullptr && _icon->visible()) {
        _icon->place(BLRect{_frame.x + _frame.w - 34.0, _frame.y + ((_frame.h - 30.0) / 2.0), 30.0,
                            30.0},
                     type);
    }
}

void Field::paint(const Painter &painter) {
    const Theme::Palette &palette = Theme::of();

    painter.round(_frame, Theme::radiusSmall, palette.field);
    painter.outline(_frame, Theme::radiusSmall, 1.0,
                    _input->focused() ? palette.accent : palette.borderStrong);

    if (_leadingGlyph != Glyphs::Glyph::Empty) {
        constexpr float weight = 1.1F;
        const double side = Glyphs::span(weight);

        Glyphs::draw(painter.context(), _leadingGlyph,
                     BLPoint{_frame.x + 12.0, _frame.y + ((_frame.h - side) / 2.0)}, weight,
                     _input->focused() ? palette.accent : palette.faint);
    }

    if (!_prefix.empty()) {
        const BLFont &face = painter.font(Typeface::mono, Theme::fontBody);
        const double taken = painter.width(face, _prefix);
        const double at = _frame.x + 12.0 + (_leadingGlyph == Glyphs::Glyph::Empty ? 0.0 : (12.0 * 1.1) + 9.0);

        painter.label(face, BLRect{at, _frame.y, taken + 2.0, _frame.h}, Align::Start, _prefix,
                      palette.faint);
        painter.fill(BLRect{at + taken + 9.0, _frame.y + 6.0, 1.0, _frame.h - 12.0},
                     palette.borderStrong);
    }

    Box::paint(painter);
}

}
