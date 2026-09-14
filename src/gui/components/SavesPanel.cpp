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

#include "core/config/Profile.h"
#include "gui/components/SavesPanel.h"
#include "gui/draw/Glyphs.h"
#include "gui/draw/Theme.h"
#include "gui/state/State.h"
#include "gui/toolkit/layout/Rule.h"
#include "gui/toolkit/layout/Spacer.h"
#include "gui/util/Desktop.h"
#include "gui/util/Format.h"

namespace components {

using namespace toolkit;

SavesPanel::SavesPanel(Reach *reach)
    : CollapsiblePanel("Saves",
                       [reach](const bool open) { reach->config.panels().setSaveOpen(open); }),
      _reach(reach) {
    _on = tools()->append(std::make_unique<MultistateSwitch>([this](const int value) {
        _reach->config.panels().setSaveEnabled(value != 0);
        _reach->config.panels().setSaveOpen(value != 0);
    }));

    _on->setOptions({{.value = 0, .label = "Off"}, {.value = 1, .label = "On"}});

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
    _note->font(400, Theme::fontSmall)->tone(Theme::of().faint)->wrap();

    _path = body->append(std::make_unique<Label>());
    _path->font(400, Theme::fontTiny)->tone(Theme::of().faint)->path();
    _path->onClick([] { Desktop::open(State::get().cfg.saveFolder); });
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

    return "from " + Format::fitPath(cfg.savePath, 30);
}

void SavesPanel::sync() {
    const State::Cfg &cfg = State::get().cfg;

    setVisible(cfg.saveLoads);

    if (!cfg.saveLoads) {
        return;
    }

    const bool broken = cfg.saveEnabled && cfg.saveFile.empty();
    const bool homeless = cfg.saveFolder.empty();

    setOpen(cfg.saveOpen);
    setSaid(summary(), broken || !cfg.saveTrouble.empty());

    _on->setCurrent(cfg.saveEnabled ? 1 : 0);

    _file->setOptions(cfg.saveFiles);
    _file->setBadges(cfg.saveSlotLabels);
    _file->setCurrent(cfg.saveIndex);
    _file->setEnabled(!cfg.saveFiles.empty());
    _file->placeholder(cfg.saveFiles.empty() ? "Nothing saved yet" : "Nothing picked");
    _file->tooltip(cfg.saveSlots
                       ? "The saves in this profile's folder, newest first. The port is handed "
                         "the slot it sits in"
                       : "The saves in this profile's folder, newest first");

    if (!cfg.saveTrouble.empty()) {
        _note->setVisible(true);
        _note->setText(cfg.saveTrouble);
        _note->tone(Theme::of().danger);
    } else if (homeless) {
        _note->setVisible(true);
        _note->setText("This profile launches on the settings the source port keeps for "
                       "itself, so its saves are the port's own and there is nothing here "
                       "to list. Give it settings of its own to keep them apart.");
        _note->tone(Theme::of().faint);
    } else if (cfg.saveFiles.empty()) {
        _note->setVisible(true);
        _note->setText("Nothing has been saved in this profile yet. A game saved while it "
                       "is playing lands in its saves folder and shows up here.");
        _note->tone(Theme::of().faint);
    } else if (broken) {
        _note->setVisible(true);
        _note->setText("Without a save picked there is nothing to load, and the profile "
                       "launches a new game.");
        _note->tone(Theme::of().warning);
    } else if (!cfg.saveFile.empty() && cfg.replayMode != ReplayMode::Off) {
        _note->setVisible(true);
        _note->setText(cfg.replayMode == ReplayMode::Record
                           ? "A demo is being recorded, which starts where a new game "
                             "starts, so the save is left out of the launch."
                           : "A demo is being played back, so the save is left out of the "
                             "launch.");
        _note->tone(Theme::of().warning);
    } else {
        _note->setVisible(false);
    }

    _path->setVisible(!cfg.saveFile.empty());
    _path->setText(cfg.savePath);
}

}
