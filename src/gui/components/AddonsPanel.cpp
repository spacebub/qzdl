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

#include <string>
#include <utility>

#include "gui/components/AddonsPanel.h"
#include "gui/components/Parts.h"
#include "gui/draw/Glyphs.h"
#include "gui/draw/Theme.h"
#include "gui/services/Filters.h"
#include "gui/state/State.h"
#include "gui/toolkit/layout/Box.h"
#include "gui/toolkit/layout/Spacer.h"

namespace components {

using namespace toolkit;

AddonsPanel::AddonsPanel(Reach *reach) : _reach(reach) {
    Box *inside = append(Box::column());

    inside->pad(16.0)->spacing(12.0);

    Box *head = inside->append(Box::row());

    head->spacing(8.0)->cross(Box::Place::Centre);
    head->fixedHeight = Theme::controlSmall;

    panelTitle(head, "Add-ons");

    head->append(std::make_unique<Spacer>());

    _loaded = head->append(std::make_unique<Pill>());
    _loaded->kind(Pill::Kind::Muted)->dot(false);

    GlyphButton *add = head->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Plus, [this] {
        _reach->picker.open(FilePicker::Action::AddFiles, "Add files", Filters::wad(), false, true, true,
                            FilePicker::Slot::Wad);
    }));

    add->tooltip("Add files");
    add->fixedWidth = Theme::controlSmall;

    _clearFiles = head->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Trash, [this] {
        _reach->ask("Clear the file list?",
                  "Every file in this profile's list is removed. The files themselves are left "
                  "alone, and the rest of the profile is untouched.",
                  "Clear", true, [this] { _reach->config.lists().clearFiles(); });
    }));

    _clearFiles->tone(Theme::of().muted, Theme::of().danger)
        ->tooltip("Remove every file from this profile");
    _clearFiles->fixedWidth = Theme::controlSmall;

    Panel *trough = inside->append(std::make_unique<Panel>());

    trough->inset = true;
    trough->stretch = 1.0;

    Box *lane = trough->append(Box::column());

    lane->pad(6.0);

    _files = lane->append(std::make_unique<AddonList>(reach));
    _files->stretch = 1.0;
}

void AddonsPanel::sync() const {
    const State::Cfg &cfg = State::get().cfg;

    _loaded->setVisible(!cfg.files.empty());
    _loaded->setText(std::cmp_equal(cfg.enabledCount, cfg.files.size())
                         ? std::to_string(cfg.files.size()) + " loaded"
                         : std::to_string(cfg.enabledCount) + " of "
                               + std::to_string(cfg.files.size()) + " loaded");

    _clearFiles->setEnabled(!cfg.files.empty());
}

}
