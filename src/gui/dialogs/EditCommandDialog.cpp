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
#include <functional>
#include <string>
#include <utility>

#include "ttk/draw/Glyphs.h"
#include "ttk/draw/Theme.h"
#include "ttk/draw/Typeface.h"
#include "ttk/toolkit/Event.h"
#include "ttk/toolkit/Root.h"

#include "core/launch/Command.h"
#include "gui/components/TokenChips.h"
#include "gui/dialogs/EditCommandDialog.h"

using namespace ttk;

namespace dialogs {

namespace {

// The command is one line, so Return saves it and Escape cancels as in any field, and no line break is typed.
// The editor keeps every key from the window, so both are answered here.
class OneLine : public TextEdit {
public:
    OneLine(std::function<void(const std::string &)> edited,
            std::function<void()> saved,
            std::function<void()> cancelled)
        : TextEdit(std::move(edited)), _saved(std::move(saved)), _cancelled(std::move(cancelled)) {}

    bool key(const Key &pressed) override {
        // An open popup takes the Escape first.
        const bool popup = root() != nullptr && root()->has_dismiss();

        if (pressed.code == Code::Return || (pressed.code == Code::Escape && !popup)) {
            // Copied out: either takes the dialog down, and this editor with it.
            const std::function<void()> answer = pressed.code == Code::Return ? _saved : _cancelled;

            if (answer) {
                answer();
            }

            return true;
        }

        return TextEdit::key(pressed);
    }

private:
    std::function<void()> _saved;
    std::function<void()> _cancelled;
};

}

EditCommandDialog::EditCommandDialog(std::string command, std::function<void(const std::string &)> accepted)
    : _command(std::move(command)), _accepted(std::move(accepted)) {
    wanted = 760.0;
    tall = 420.0;

    card()->fills = false;

    _shut = card()->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Close, [this] {
        if (dismissed) {
            dismissed();
        }
    }));

    _shut->size(26.0);

    _well = card()->append(std::make_unique<Panel>());

    _well->inset = true;
    _well->rounding = Theme::radius;

    // Copied out: dismissing takes the dialog down, and the function with it.
    const auto cancel = [this] {
        const std::function<void()> shut = dismissed;

        if (shut) {
            shut();
        }
    };

    _editor = card()->append(std::make_unique<OneLine>(
            [this](const std::string &text) { _command = text; }, [this] { commit(); }, cancel));
    _editor->bare()->wrapped()->placeholder(Command::example());
    _editor->set_text(_command);

    _tokens = card()->append(std::make_unique<components::TokenChips>());

    _cancel = card()->append(std::make_unique<Button>("Cancel", [this] {
        if (dismissed) {
            dismissed();
        }
    }));

    _save = card()->append(std::make_unique<Button>("Save", [this] { commit(); }));
    _save->kind(Button::Kind::Primary);
}

void EditCommandDialog::opened() {
    if (root() != nullptr) {
        root()->focus(_editor);
    }
}

void EditCommandDialog::commit() const {
    // Copied out: accepting takes the dialog down, and both with it.
    const std::string command = _command;
    const auto fire = _accepted;
    const std::function<void()> shut = dismissed;

    if (shut) {
        shut();
    }

    if (fire) {
        fire(command);
    }
}

void EditCommandDialog::arrange(Typeface &type) {
    Dialog::arrange(type);

    const BLRect box = card()->box();

    _shut->place(BLRect{box.x + box.w - 22.0 - 26.0, box.y + 22.0, 26.0, 26.0}, type);

    const double bottom = box.y + box.h - 22.0 - Theme::control;
    const double wide = box.w - 44.0;
    const double chips = _tokens->natural_height(type, wide);
    const double under = bottom - 14.0 - chips;
    const BLRect panel{box.x + 22.0, box.y + 62.0, wide, std::max(0.0, under - 12.0 - (box.y + 62.0))};

    _tokens->place(BLRect{box.x + 22.0, under, wide, chips}, type);
    _well->place(panel, type);
    _editor->place(BLRect{panel.x + 6.0, panel.y + 6.0, panel.w - 12.0, panel.h - 12.0}, type);

    _save->place(BLRect{box.x + box.w - 22.0 - Theme::buttonWidth, bottom, Theme::buttonWidth, Theme::control}, type);
    _cancel->place(
            BLRect{box.x + box.w - 22.0 - (Theme::buttonWidth * 2.0) - 8.0, bottom, Theme::buttonWidth, Theme::control},
            type);
}

void EditCommandDialog::paint_over(const Painter &painter) {
    const Theme::Palette &palette = Theme::palette();
    const BLRect box = card()->box();

    painter.label(painter.font(palette.headingWeight, Theme::fontLarge),
                  BLRect{box.x + 22.0, box.y + 22.0, box.w - 44.0 - 30.0, 26.0},
                  Align::Start,
                  "Edit command",
                  palette.text);
}

}
