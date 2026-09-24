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

#include "ttk/draw/Glyphs.h"
#include "ttk/draw/Theme.h"
#include "ttk/toolkit/layout/Rule.h"
#include "ttk/toolkit/layout/Spacer.h"
#include "ttk/util/Desktop.h"
#include "ttk/util/Format.h"

#include "core/config/Profile.h"
#include "gui/components/SavesPanel.h"
#include "gui/state/State.h"

using namespace ttk;

namespace components {

SavesPanel::SavesPanel(Reach *reach)
    : CollapsiblePanel("Saves",
                       [reach](const bool open) { reach->config.panels().setSaveOpen(open); }),
      _reach(reach) {
    _on = tools()->append(std::make_unique<MultistateSwitch>([this](const int value) {
        _reach->config.panels().setSaveEnabled(value != 0);
        _reach->config.panels().setSaveOpen(value != 0);
    }));

    _on->set_options({{.value = 0, .label = "Off"}, {.value = 1, .label = "On"}});

    Box *body = CollapsiblePanel::body();

    body->append(std::make_unique<Rule>());

    Box *pick = body->append(Box::row());

    pick->spacing(8.0)->cross(Box::Place::End);

    _file = pick->append(std::make_unique<Select>("Save", [this](const int index) {
        _reach->config.panels().setSaveIndex(index);
    }));

    _file->clearable();
    _file->fixedWidth = 340.0;

    _refresh = pick->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Refresh, [this] {
        _reach->config.panels().refreshSaves();
    }));

    _refresh->size(Theme::control)->outlined()->tooltip("Refresh folder");
    _refresh->fixedWidth = Theme::control;

    pick->append(std::make_unique<Spacer>());

    _note = body->append(std::make_unique<Label>());
    _note->font(400, Theme::fontSmall)->tone(&Theme::Palette::faint)->wrap();

    _path = body->append(std::make_unique<Label>());
    _path->font(400, Theme::fontTiny)->tone(&Theme::Palette::faint)->path();
    _path->on_click([] { Desktop::open(State::get().cfg.saveFolder); });
    _path->hint = "Show in file explorer";
}

std::string SavesPanel::summary() {
    const State::Cfg &cfg = State::get().cfg;

    if (!cfg.saveEnabled) {
        return "Off · every launch starts a new game";
    }

    if (cfg.saveFile.empty()) {
        return "nothing picked yet";
    }

    return "from " + Format::fit_path(cfg.savePath, 30);
}

void SavesPanel::sync() {
    const State::Cfg &cfg = State::get().cfg;

    set_visible(cfg.saveLoads);

    if (!cfg.saveLoads) {
        return;
    }

    const bool broken = cfg.saveEnabled && cfg.saveFile.empty();
    const bool homeless = cfg.saveFolder.empty();

    set_open(cfg.saveOpen);
    set_said(summary(), broken || !cfg.saveTrouble.empty());

    _on->set_current(cfg.saveEnabled ? 1 : 0);

    _file->set_options(cfg.saveFiles);
    _file->set_badges(cfg.saveSlotLabels);
    _file->set_current(cfg.saveIndex);
    _file->set_enabled(!cfg.saveFiles.empty());
    _file->placeholder(cfg.saveFiles.empty() ? "Nothing saved yet" : "Nothing picked");
    _file->tooltip(cfg.saveSlots
                       ? "The saves in this profile's folder, newest first. The port is handed "
                         "the slot it sits in"
                       : "The saves in this profile's folder, newest first");

    if (!cfg.saveTrouble.empty()) {
        _note->set_visible(true);
        _note->set_text(cfg.saveTrouble);
        _note->tone(&Theme::Palette::danger);
    } else if (homeless) {
        _note->set_visible(true);
        _note->set_text("This profile launches on the settings the source port keeps for "
                       "itself, so its saves are the port's own and there is nothing here "
                       "to list. Give it settings of its own to keep them apart.");
        _note->tone(&Theme::Palette::faint);
    } else if (cfg.saveFiles.empty()) {
        _note->set_visible(true);
        _note->set_text("Nothing has been saved in this profile yet. A game saved while it "
                       "is playing lands in its saves folder and shows up here.");
        _note->tone(&Theme::Palette::faint);
    } else if (broken) {
        _note->set_visible(true);
        _note->set_text("Without a save picked there is nothing to load, and the profile "
                       "launches a new game.");
        _note->tone(&Theme::Palette::warning);
    } else if (!cfg.saveFile.empty() && cfg.replayMode != ReplayMode::Off) {
        _note->set_visible(true);
        _note->set_text(cfg.replayMode == ReplayMode::Record
                           ? "A demo is being recorded, which starts where a new game "
                             "starts, so the save is left out of the launch."
                           : "A demo is being played back, so the save is left out of the "
                             "launch.");
        _note->tone(&Theme::Palette::warning);
    } else {
        _note->set_visible(false);
    }

    _path->set_visible(!cfg.saveFile.empty());
    _path->set_text(cfg.savePath);
}

}
