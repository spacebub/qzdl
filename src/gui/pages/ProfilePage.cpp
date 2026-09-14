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

#include "gui/draw/Glyphs.h"
#include "gui/draw/Theme.h"
#include "gui/pages/ProfilePage.h"
#include "gui/services/Filters.h"
#include "gui/state/State.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/layout/Scroll.h"
#include "gui/toolkit/layout/Spacer.h"

namespace pages {

using namespace toolkit;

ProfilePage::ProfilePage(Reach *reach) : _reach(reach) {
    Box *column = append(Box::column());

    column->spacing(16.0);

    _head = column->append(std::make_unique<components::ProfileHead>(reach));

    Scroll *scroll = column->append(std::make_unique<Scroll>());

    scroll->stretch = 1.0;

    Box *body = static_cast<Box *>(scroll->hold(Box::column()));

    body->spacing(16.0)->pad(Theme::bleed, 0.0, Theme::bleed, 8.0);

    // Add-ons floor, gap, and the run panel: under this the page is cut, not
    // squeezed.
    body->minWidth = components::AddonsPanel::LEAST + 16.0 + components::RunPanel::WIDTH
        + (Theme::bleed * 2.0);

    Box *top = body->append(Box::row());

    top->spacing(16.0);

    // A floor, not a ceiling: the row grows with whatever The run holds.
    top->minHeight = 340.0;

    _addons = top->append(std::make_unique<components::AddonsPanel>(reach));
    _addons->stretch = 1.0;
    _addons->minWidth = components::AddonsPanel::LEAST;

    _run = top->append(std::make_unique<components::RunPanel>(reach));
    _run->fixedWidth = components::RunPanel::WIDTH;

    _replay = body->append(std::make_unique<components::ReplayPanel>(reach));
    _saves = body->append(std::make_unique<components::SavesPanel>(reach));
    _net = body->append(std::make_unique<components::NetPanel>(reach));
    _command = body->append(std::make_unique<components::CommandPanel>(reach));

    // --- no profiles ---

    _none = append(Box::column());
    _none->spacing(18.0)->align(Box::Place::Centre)->cross(Box::Place::Centre);
    _none->setVisible(false);

    Label *title = _none->append(std::make_unique<Label>("No profiles"));

    title->font(600, Theme::fontMedium)->tone(Theme::of().muted);
    title->fixedWidth = 420.0;

    Label *said = _none->append(std::make_unique<Label>(
        "A profile holds a game, the port it runs on and whatever is loaded over them, under a "
        "name. This config has none."));

    said->font(400, Theme::fontSmall)->tone(Theme::of().faint)->wrap();
    said->fixedWidth = 420.0;

    Box *buttons = _none->append(Box::row());

    buttons->spacing(8.0);
    buttons->fixedWidth = 420.0;
    buttons->fixedHeight = Theme::control;

    buttons->append(std::make_unique<Button>("New profile", [this] {
        _reach->prompt("New profile", "Name", "New profile", "Create",
                     [this](const std::string &named) {
                         _reach->config.profile().addProfile(named);
                     });
    }))->kind(Button::Kind::Primary)->glyph(Glyphs::Glyph::Plus);

    buttons->append(std::make_unique<Button>("Import a .zdl", [this] {
        _reach->picker.open(FilePicker::Action::LoadZdl, "Load a .zdl launch config", Filters::zdl(), false,
                            false, false, FilePicker::Slot::Zdl);
    }))->kind(Button::Kind::Ghost)->glyph(Glyphs::Glyph::Download)
        ->tooltip("Read a .zdl launch config in as a profile of its own");

    buttons->append(std::make_unique<Spacer>());
}

void ProfilePage::sync() const {
    const bool empty = State::get().cfg.profileIndex < 0;

    _none->setVisible(empty);
    children().front()->setVisible(!empty);

    if (empty) {
        return;
    }

    _head->sync();
    _addons->sync();
    _run->sync();
    _replay->sync();
    _saves->sync();
    _net->sync();
    _command->sync();
}

}
