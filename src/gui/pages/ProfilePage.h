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
#pragma once

#include <string>

#include "gui/components/AddonList.h"
#include "gui/components/NetPanel.h"
#include "gui/components/ProfileChooser.h"
#include "gui/components/Reach.h"
#include "gui/components/ReplayPanel.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/Chip.h"
#include "gui/toolkit/controls/Fact.h"
#include "gui/toolkit/controls/Field.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/layout/Collapsible.h"
#include "gui/toolkit/controls/Pill.h"
#include "gui/toolkit/controls/MultistateSwitch.h"
#include "gui/toolkit/controls/Select.h"
#include "gui/toolkit/controls/Toggle.h"
#include "gui/toolkit/layout/Box.h"
#include "gui/toolkit/layout/Scroll.h"
#include "gui/toolkit/layout/Wrap.h"

namespace pages {

// What one profile launches, and everything that hangs off it.
class ProfilePage : public toolkit::Widget {
public:
    explicit ProfilePage(Reach *reach);

    void sync() const;

private:
    void buildRun(toolkit::Box *into);
    void buildSaves(toolkit::Box *into);
    void buildCommand(toolkit::Box *into);

    void syncRun() const;
    void syncSaves() const;
    void syncCommand() const;

    void showMenu() const;

    // What the heading says under the name.
    [[nodiscard]] static std::string summary();
    [[nodiscard]] static std::string saveSummary();

    static bool ready();

    Reach *_reach;

    // The head.
    components::ProfileChooser *_chooser = nullptr;
    toolkit::GlyphButton *_terminal = nullptr;
    toolkit::GlyphButton *_cog = nullptr;
    toolkit::Button *_launch = nullptr;

    toolkit::Scroll *_scroll = nullptr;

    // Add-ons.
    toolkit::Pill *_loaded = nullptr;
    toolkit::GlyphButton *_addFiles = nullptr;
    toolkit::GlyphButton *_clearFiles = nullptr;
    components::AddonList *_files = nullptr;

    // The run.
    toolkit::Button *_addPort = nullptr;
    toolkit::Select *_port = nullptr;
    toolkit::Button *_addGame = nullptr;
    toolkit::Select *_iwad = nullptr;
    toolkit::Select *_map = nullptr;
    toolkit::Select *_skill = nullptr;
    toolkit::Select *_monsters = nullptr;
    toolkit::Toggle *_capture = nullptr;
    toolkit::Toggle *_fullscreen = nullptr;
    toolkit::Toggle *_levelstat = nullptr;
    toolkit::Toggle *_sharedConfig = nullptr;
    toolkit::Fact *_directory = nullptr;

    components::ReplayPanel *_replay = nullptr;

    // Saves.
    toolkit::CollapsiblePanel *_saves = nullptr;
    toolkit::MultistateSwitch *_saveOn = nullptr;
    toolkit::Select *_saveFile = nullptr;
    toolkit::GlyphButton *_saveRefresh = nullptr;
    toolkit::Label *_saveNote = nullptr;
    toolkit::Label *_savePath = nullptr;

    components::NetPanel *_net = nullptr;

    // Command line.
    toolkit::Toggle *_override = nullptr;
    toolkit::Field *_extra = nullptr;
    toolkit::Field *_command = nullptr;
    toolkit::Wrap *_tokens = nullptr;
    toolkit::Chip *_budget = nullptr;
    toolkit::Label *_resolved = nullptr;
    toolkit::GlyphButton *_copy = nullptr;

    // Shown instead of everything when the config holds no profiles.
    toolkit::Box *_none = nullptr;
    toolkit::Box *_body = nullptr;
};

}
