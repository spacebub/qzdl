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

#include "gui/dialogs/AboutDialog.h"
#include "gui/draw/Mark.h"
#include "gui/state/State.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/layout/Box.h"
#include "gui/toolkit/layout/Spacer.h"
#include "gui/toolkit/overlays/Dialog.h"
#include "gui/util/Desktop.h"
#include "gui/util/Format.h"

namespace dialogs {

using namespace toolkit;

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

    name->font(Theme::of().headingWeight, Theme::fontDisplay)->tone(Theme::of().text);

    _version = said->append(std::make_unique<Label>());
    _version->font(400, Theme::fontSmall)->tone(Theme::of().faint);

    Label *blurb = said->append(
        std::make_unique<Label>("A launcher for Doom engine source ports."));

    blurb->font(400, Theme::fontSmall)->tone(Theme::of().muted);

    GlyphButton *shut = top->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Cross, [this] {
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
            ->tone(Theme::of().muted);
    }

    Box *thanks = column->append(Box::column());

    thanks->spacing(3.0);

    thanks->append(std::make_unique<Label>("THANKS"))->section();

    thanks->append(std::make_unique<Label>(
               "BioHazard, for the original version. NeuralStunner, without whose help none "
               "of this would be possible. Blzut3, Risen, Enjay, DRDTeam.org and "
               "ZDoom.org."))
        ->font(400, Theme::fontSmall)
        ->tone(Theme::of().muted)
        ->wrap();

    Box *where = column->append(Box::column());

    where->spacing(4.0);

    where->append(std::make_unique<Label>("CONFIGURATION FILE"))->section();

    _path = where->append(std::make_unique<Label>());
    _path->font(400, Theme::fontSmall)->tone(Theme::of().faint)->mono();
    _path->hint = "Open the directory it is in";
    _path->onClick([] { Desktop::open(Format::directoryOf(State::get().cfg.path)); });

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
    _version->setText("Version " + State::get().sys.version + " · "
                      + State::get().sys.runtime);
    _path->setText(Format::prettyPath(State::get().cfg.path));
}

void AboutDialog::paintOver(const Painter &painter) {
    const BLImage &mark = Mark::of(128);

    if (!mark.is_empty()) {
        const BLRect box = card()->box();

        painter.context().blit_image(BLRect{box.x + 22.0, box.y + 23.0, 56.0, 56.0}, mark);
    }
}

}
