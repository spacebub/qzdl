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

#include "gui/components/sheets/CommandSheet.h"
#include "gui/draw/Typeface.h"
#include "gui/model/State.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/layout/Scroll.h"
#include "gui/toolkit/overlays/Sheet.h"
#include "gui/util/Clipboard.h"

namespace components {

using namespace toolkit;

CommandSheet::CommandSheet(std::function<void()> copied)
    : _copied(std::move(copied)) {
    wanted = 760.0;
    tall = 420.0;

    _shut = card()->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Cross, [this] {
        if (dismissed) {
            dismissed();
        }
    }));

    _shut->size(26.0);

    _well = card()->append(std::make_unique<Panel>());

    _well->inset = true;
    _well->rounding = Theme::radius;

    _scroll = card()->append(std::make_unique<Scroll>());

    _view = static_cast<TextView *>(_scroll->hold(std::make_unique<TextView>()));

    _view->face(Typeface::mono, Theme::fontSmall)->ink([](size_t /*row*/) {
        const Theme::Palette &palette = Theme::of();

        return State::get().cfg.commandLine.empty() ? palette.faint : palette.text;
    });

    _copy = card()->append(std::make_unique<Button>("Copy", [this] {
        Clipboard::write(State::get().cfg.commandLine);
        if (_copied) {
            _copied();
        }
    }));

    _copy->glyph(Glyphs::Glyph::Edit);

    _close = card()->append(std::make_unique<Button>("Close", [this] {
        if (dismissed) {
            dismissed();
        }
    }));

    _close->kind(Button::Kind::Primary);
}

void CommandSheet::sync() {
    const State::Cfg &cfg = State::get().cfg;

    _shown = !cfg.commandLine.empty() ? cfg.commandLine
           : cfg.dosPort && cfg.dosbox.empty() && cfg.systemDosbox.empty()
               ? "Nothing to launch yet: this source port is a DOS one, and there is no "
                 "DOSBox on this machine to run it in."
               : "Nothing to launch yet: no source port is selected.";

    _view->setRun(_shown);

    _copy->setEnabled(!cfg.commandLine.empty());
}

void CommandSheet::arrange(Typeface &type) {
    Sheet::arrange(type);

    const BLRect box = card()->box();

    _shut->place(BLRect{box.x + box.w - 22.0 - 26.0, box.y + 22.0, 26.0, 26.0}, type);

    const BLRect panel{box.x + 22.0, box.y + 62.0, box.w - 44.0,
                       box.h - 62.0 - 14.0 - Theme::control - 22.0};

    _well->place(panel, type);
    _scroll->place(BLRect{panel.x + 14.0, panel.y + 14.0, panel.w - 28.0, panel.h - 28.0},
                   type);

    const double bottom = box.y + box.h - 22.0 - Theme::control;

    _close->place(BLRect{box.x + box.w - 22.0 - Theme::buttonWidth, bottom,
                         Theme::buttonWidth, Theme::control},
                  type);
    _copy->place(BLRect{box.x + box.w - 22.0 - (Theme::buttonWidth * 2.0) - 8.0, bottom,
                        Theme::buttonWidth, Theme::control},
                 type);
}

void CommandSheet::paintOver(const Painter &painter) {
    const Theme::Palette &palette = Theme::of();
    const BLRect box = card()->box();

    painter.label(painter.font(palette.headingWeight, Theme::fontLarge),
                  BLRect{box.x + 22.0, box.y + 22.0, box.w - 44.0 - 30.0, 26.0}, Align::Start,
                  "Generated command", palette.text);
}

}
