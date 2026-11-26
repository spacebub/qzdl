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
#include <string>
#include <utility>

#include "ttk/draw/Glyphs.h"
#include "ttk/draw/Theme.h"
#include "ttk/toolkit/layout/Box.h"
#include "ttk/toolkit/layout/Spacer.h"

#include "gui/components/AddonsPanel.h"
#include "gui/components/Parts.h"
#include "gui/model/GroupsBridge.h"
#include "gui/services/Filters.h"
#include "gui/state/State.h"

using namespace ttk;

namespace components {

AddonsPanel::AddonsPanel(Reach *reach) : _reach(reach) {
    Box *inside = append(Box::column());

    inside->pad(16.0)->spacing(12.0);

    Box *head = inside->append(Box::row());

    _head = head;
    head->spacing(8.0)->cross(Box::Place::Centre);
    head->fixedHeight = Theme::controlSmall;

    _title = panelTitle(head, "Add-ons");

    _view = head->append(std::make_unique<MultistateSwitch>([this](const int value) {
        _reach->config.lists().setAddonView(static_cast<State::AddonView>(value));
        _reach->touch();
    }));
    _view->compact()->set_options({
            {.value = static_cast<int>(State::AddonView::Groups), .label = "Groups", .badge = false},
            {.value = static_cast<int>(State::AddonView::LoadOrder), .label = "Load order", .badge = false},
    });

    _spacer = head->append(std::make_unique<Spacer>());

    _newGroup = head->append(std::make_unique<Label>("New group"));
    _newGroup->tone(&Theme::Palette::muted);
    _newGroup->hint = "Add a group to sort the add-ons into";
    _newGroup->on_click([this] {
        _reach->prompt("New group", "Name", "New group", "Create", [this](const std::string &named) {
            _reach->config.groups().add(GroupsBridge::List::Files, named);
        });
    });

    _loaded = head->append(std::make_unique<Pill>());
    _loaded->kind(Pill::Kind::Muted)->dot(false);

    GlyphButton *add = head->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Plus, [this] {
        _reach->files.open("Add files",
                           Filters::addon(),
                           false,
                           true,
                           true,
                           LastDir::WAD,
                           [this](const std::vector<std::string> &paths, bool) {
                               _reach->config.lists().addFiles(paths);
                               _files->sync();
                               _files->scroll_to(_files->reach());
                               _reach->touch();
                           });
    }));

    add->tooltip("Add files");
    add->fixedWidth = Theme::controlSmall;

    _clearFiles = head->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Trash, [this] {
        _reach->ask("Clear the file list?",
                    "Every file in this profile's list is removed. The files themselves are left "
                    "alone, and the rest of the profile is untouched.",
                    "Clear",
                    true,
                    [this] { _reach->config.lists().clearFiles(); });
    }));

    _clearFiles->tone(&Theme::Palette::muted, &Theme::Palette::danger)->tooltip("Remove every file from this profile");
    _clearFiles->fixedWidth = Theme::controlSmall;

    Panel *trough = inside->append(std::make_unique<Panel>());

    trough->inset = true;
    trough->stretch = 1.0;

    Box *lane = trough->append(Box::column());

    lane->pad(6.0);

    _files = lane->append(std::make_unique<AddonList>(reach));
    _files->stretch = 1.0;

    _note = inside->append(std::make_unique<Label>());
    _note->font(400, Theme::fontSmall)->tone(&Theme::Palette::warning)->wrap();
    _note->set_visible(false);
}

void AddonsPanel::sync() const {
    const State::Cfg &cfg = State::get().cfg;

    _files->sync();

    // The view sits beside the title while it shows, and the title takes the spare room while it does not.
    _title->stretch = cfg.addonsGrouped ? 0.0 : 1.0;
    _spacer->set_visible(cfg.addonsGrouped);
    _view->set_visible(cfg.addonsGrouped);
    _view->set_current(static_cast<int>(State::get().nav.addons));
    _newGroup->set_visible(cfg.groupAddons);

    // A ticked patch the launch leaves out does not load.
    const auto loads = cfg.enabledCount - std::ranges::count_if(cfg.files, &State::FileRow::leftOut);

    _loaded->set_visible(!cfg.files.empty());
    _loaded->set_text(std::cmp_equal(loads, cfg.files.size())
                              ? std::to_string(cfg.files.size()) + " loaded"
                              : std::to_string(loads) + " of " + std::to_string(cfg.files.size()) + " loaded");

    _clearFiles->set_enabled(!cfg.files.empty());

    _note->set_visible(!cfg.patchWarning.empty());
    _note->set_text(cfg.patchWarning);
}

double AddonsPanel::least(Typeface &type) const {
    return _newGroup->visible() ? std::max(LEAST, _head->natural_width(type) + 32.0) : LEAST;
}

}
