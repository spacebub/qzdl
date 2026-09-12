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

#include "gui/components/sheets/CopyConfigSheet.h"
#include "gui/draw/Typeface.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/overlays/Sheet.h"

namespace components {

using namespace toolkit;

CopyConfigSheet::CopyConfigSheet(std::function<void(const std::string &)> picked)
    : _picked(std::move(picked)) {
    wanted = 560.0;
    tall = 480.0;

    _rows = card()->append(std::make_unique<Rows>(this));

    _cancel = card()->append(std::make_unique<Button>("Cancel", [this] {
        if (dismissed) {
            dismissed();
        }
    }));

    _cancel->compact();

    _accept = card()->append(std::make_unique<Button>("Copy", [this] {
        const std::function<void(const std::string &)> fire = _picked;
            const std::string id = _chosen;

            if (dismissed) {
                dismissed();
            }

            if (fire) {
                fire(id);
            }
    }));

    _accept->kind(Button::Kind::Primary)->compact();
}

void CopyConfigSheet::sync() {
    _accept->setEnabled(!_chosen.empty());
}

void CopyConfigSheet::arrange(Typeface &type) {
    Sheet::arrange(type);

    const BLRect box = card()->box();

    _panel = BLRect{box.x + 22.0, box.y + 96.0, box.w - 44.0,
                    box.h - 96.0 - 14.0 - Theme::controlSmall - 22.0};

    _rows->place(BLRect{_panel.x + 6.0, _panel.y + 6.0, _panel.w - 12.0, _panel.h - 12.0},
                 type);

    const double bottom = box.y + box.h - 22.0 - Theme::controlSmall;
    const double wide = 110.0;

    _accept->place(BLRect{box.x + box.w - 22.0 - wide, bottom, wide, Theme::controlSmall},
                   type);
    _cancel->place(BLRect{box.x + box.w - 22.0 - (wide * 2.0) - 10.0, bottom, wide,
                          Theme::controlSmall},
                   type);
}

void CopyConfigSheet::paintOver(const Painter &painter) {
    const Theme::Palette &palette = Theme::of();
    const BLRect box = card()->box();
    const State::Cfg &cfg = State::get().cfg;

    painter.label(painter.font(palette.headingWeight, Theme::fontLarge),
                  BLRect{box.x + 22.0, box.y + 22.0, box.w - 44.0, 24.0}, Align::Start,
                  "Copy port config", palette.text);

    painter.paragraph(painter.font(400, Theme::fontSmall),
                      BLRect{box.x + 22.0, box.y + 52.0, box.w - 44.0, 0.0},
                      cfg.configDonors.empty()
                          ? "Nothing else on " + cfg.port + " has settings to take."
                          : "Take the settings another profile on " + cfg.port
                                + " has built up. What this profile keeps now is written "
                                  "over.",
                      palette.muted);

    painter.round(_panel, Theme::radius, palette.sunken);
    painter.outline(_panel, Theme::radius, 1.0, palette.border);

    _rows->paint(painter);
}

}
