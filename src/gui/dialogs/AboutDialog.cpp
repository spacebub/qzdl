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
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/TextView.h"
#include "ttk/toolkit/layout/Box.h"
#include "ttk/toolkit/layout/Scroll.h"
#include "ttk/toolkit/layout/Spacer.h"
#include "ttk/toolkit/overlays/Dialog.h"
#include "ttk/util/Desktop.h"

#include "gui/dialogs/AboutDialog.h"
#include "gui/draw/Mark.h"
#include "gui/state/State.h"

namespace Embedded {

extern const unsigned char authors[];
extern const std::size_t authorsSize;

}

using namespace ttk;

namespace {

struct Credits {
    std::vector<std::string> rows;
    std::vector<bool> headings;
};

Credits credits() {
    const std::string_view text{reinterpret_cast<const char *>(Embedded::authors), Embedded::authorsSize};
    Credits out;

    for (size_t at = 0; at < text.size();) {
        const size_t end = std::min(text.find('\n', at), text.size());
        std::string_view line = text.substr(at, end - at);

        at = end + 1;

        if (line.ends_with('\r')) {
            line.remove_suffix(1);
        }

        if (line.starts_with("# ") || (line.empty() && out.rows.empty())) {
            continue;
        }

        const bool heading = line.starts_with("## ");
        const size_t indent = std::min(line.find_first_not_of(' '), line.size());

        if (heading) {
            out.rows.emplace_back(line.substr(3));
        } else if (line.substr(indent).starts_with("- ")) {
            out.rows.push_back(std::string(indent + 2, ' ').append(line.substr(indent + 2)));
        } else {
            out.rows.emplace_back(line);
        }

        out.headings.push_back(heading);
    }

    return out;
}

}

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

    Label *blurb = said->append(std::make_unique<Label>("A launcher for Doom engine source ports."));

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

    for (const char *line : {"© 2023-2026 spacebub", "© 2018-2019 Lcferrum", "© 2004-2012 ZDL Software Foundation"}) {
        rights->append(std::make_unique<Label>(line))->font(400, Theme::fontSmall)->tone(&Theme::Palette::muted);
    }

    Box *thanks = column->append(Box::column());

    thanks->spacing(3.0);

    thanks->append(std::make_unique<Label>("THANKS"))->section();

    thanks->append(
                  std::make_unique<Label>("BioHazard, for the original version. NeuralStunner, without whose help none "
                                          "of this would be possible. Blzut3, Risen, Enjay, DRDTeam.org and "
                                          "ZDoom.org."))
            ->font(400, Theme::fontSmall)
            ->tone(&Theme::Palette::muted)
            ->wrap();

    Box *people = column->append(Box::column());

    people->spacing(4.0);

    people->append(std::make_unique<Label>("AUTHORS"))->section();

    Scroll *scroll = people->append(std::make_unique<Scroll>());

    scroll->fixedHeight = 150.0;

    TextView *list = static_cast<TextView *>(scroll->hold(std::make_unique<TextView>()));
    Credits names = credits();

    list->face(400, Theme::fontSmall)->ink([headings = std::move(names.headings)](const size_t row) {
        const Theme::Palette &palette = Theme::palette();

        return row < headings.size() && headings[row] ? palette.text : palette.muted;
    });
    list->set_rows(std::move(names.rows));

    Box *row = column->append(Box::row());

    row->spacing(8.0);
    row->fixedHeight = Theme::controlSmall;

    GlyphButton *site = row->append(std::make_unique<GlyphButton>(
            Glyphs::Glyph::Globe, [] { Desktop::open("https://github.com/spacebub/qzdl"); }));

    site->outlined()->tooltip("Project page");
    site->fixedWidth = Theme::controlSmall;

    row->append(std::make_unique<Spacer>());

    row->append(std::make_unique<Button>("Close",
                                         [this] {
                                             if (dismissed) {
                                                 dismissed();
                                             }
                                         }))
            ->kind(Button::Kind::Primary)
            ->compact();
}

void AboutDialog::sync() {
    _version->set_text("Version " + State::get().sys.version + " · " + State::get().sys.runtime);
}

void AboutDialog::paint_over(const Painter &painter) {
    const BLImage &mark = Mark::of(128);

    if (!mark.is_empty()) {
        const BLRect box = card()->box();

        painter.context().blit_image(BLRect{box.x + 22.0, box.y + 23.0, 56.0, 56.0}, mark);
    }
}

}
