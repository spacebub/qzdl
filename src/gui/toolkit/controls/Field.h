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

#include <functional>
#include <string>

#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/controls/Pill.h"
#include "gui/toolkit/controls/TextBox.h"
#include "gui/toolkit/layout/Box.h"

namespace toolkit {

// A caption, a box, and whatever hangs off it.
class Field : public Box {
public:
    Field(std::string label, std::function<void(const std::string &)> edited);

    Field *placeholder(std::string text);
    Field *value(const std::string &text);
    Field *leadingGlyph(Glyphs::Glyph glyph);
    Field *prefix(std::string text);
    Field *mono(bool value = true);
    Field *readOnly(bool value = true);
    Field *note(std::string text);
    Field *badge(std::string text, Pill::Kind kind);

    // A glyph button inside the box, or a full button beside it.
    Field *icon(Glyphs::Glyph glyph, std::string hint, std::function<void()> pressed);
    Field *action(std::string label, std::function<void()> pressed);

    void setText(const std::string &text);

    [[nodiscard]] const std::string &text() const { return _input->text(); }

    [[nodiscard]] TextBox *input() const { return _input; }

    void takeFocus();

    void paint(const Painter &painter) override;

    void arrange(Typeface &type) override;

    std::function<void()> accepted;

private:
    // The box the input sits in, drawn by this widget rather than a child.
    BLRect _frame{};

    Box *_caption_row = nullptr;
    Label *_caption = nullptr;
    Pill *_badge = nullptr;
    TextBox *_input = nullptr;
    GlyphButton *_icon = nullptr;
    Button *_action = nullptr;
    Label *_note = nullptr;

    Box *_row = nullptr;

    Glyphs::Glyph _leadingGlyph{};
    std::string _prefix;
};

}
