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

#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/layout/Box.h"
#include "ttk/toolkit/layout/Spacer.h"
#include "ttk/toolkit/overlays/Dialog.h"
#include "ttk/util/Desktop.h"
#include "ttk/util/Format.h"

#include "gui/dialogs/AboutDialog.h"
#include "gui/draw/Mark.h"
#include "gui/state/State.h"

using namespace ttk;

namespace dialogs {

AboutDialog::AboutDialog() {
    wanted = 520.0;

    Box *column = card()->append(Box::column());

    column->pad(22.0)->spacing(16.0);

    Box *top = column->append(Box::row());

    top->spacing(14.0)->cross(Box::Place::Centre);
    top->fixedHeight = 58.0;

    top->append(std::make_unique<Spacer>(0.0))->fixedWidth = 56.0;

    Box *said = top->append(Box::column());

    said->spacing(2.0)->align(Box::Place::Centre);
    said->stretch = 1.0;

    Label *name = said->append(std::make_unique<Label>("ZDL4"));

    name->font(Theme::palette().headingWeight, Theme::fontDisplay)->tone(&Theme::Palette::text);

    _version = said->append(std::make_unique<Label>());
    _version->font(400, Theme::fontSmall)->tone(&Theme::Palette::faint);

    Label *blurb = said->append(
        std::make_unique<Label>("A launcher for Doom engine source ports."));

    blurb->font(400, Theme::fontSmall)->tone(&Theme::Palette::muted);

    GlyphButton *shut = top->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Close, [this] {
        if (dismissed) {
            dismissed();
        }
    }));

    shut->size(26.0);
    shut->fixedWidth = 26.0;

    column->append(std::make_unique<Spacer>(0.0))->fixedHeight = 1.0;

    Box *rights = column->append(Box::column());

    rights->spacing(3.0);

    rights->append(std::make_unique<Label>("COPYRIGHT"))->section();

    for (const char *line : {"© 2023-2026 spacebub", "© 2018-2019 Lcferrum",
                             "© 2004-2012 ZDL Software Foundation"}) {
        rights->append(std::make_unique<Label>(line))
            ->font(400, Theme::fontSmall)
            ->tone(&Theme::Palette::muted);
    }

    Box *thanks = column->append(Box::column());

    thanks->spacing(3.0);

    thanks->append(std::make_unique<Label>("THANKS"))->section();

    thanks->append(std::make_unique<Label>(
               "BioHazard, for the original version. NeuralStunner, without whose help none "
               "of this would be possible. Blzut3, Risen, Enjay, DRDTeam.org and "
               "ZDoom.org."))
        ->font(400, Theme::fontSmall)
        ->tone(&Theme::Palette::muted)
        ->wrap();

    Box *where = column->append(Box::column());

    where->spacing(4.0);

    where->append(std::make_unique<Label>("CONFIGURATION FILE"))->section();

    _path = where->append(std::make_unique<Label>());
    _path->font(400, Theme::fontSmall)->tone(&Theme::Palette::faint)->mono();
    _path->hint = "Show in file explorer";
    _path->on_click([] { Desktop::open(Format::directory_of(State::get().cfg.path)); });

    Box *row = column->append(Box::row());

    row->spacing(8.0);
    row->fixedHeight = Theme::controlSmall;

    row->append(std::make_unique<Button>("Project page", [] {
        Desktop::open("https://github.com/spacebub/qzdl");
    }))->kind(Button::Kind::Ghost)->compact();

    row->append(std::make_unique<Spacer>());

    row->append(std::make_unique<Button>("Close", [this] {
        if (dismissed) {
            dismissed();
        }
    }))->kind(Button::Kind::Primary)->compact();
}

void AboutDialog::sync() {
    _version->set_text("Version " + State::get().sys.version + " · "
                      + State::get().sys.runtime);
    _path->set_text(Format::pretty_path(State::get().cfg.path));
}

void AboutDialog::paint_over(const Painter &painter) {
    const BLImage &mark = Mark::of(128);

    if (!mark.is_empty()) {
        const BLRect box = card()->box();

        painter.context().blit_image(BLRect{box.x + 22.0, box.y + 23.0, 56.0, 56.0}, mark);
    }
}

}
