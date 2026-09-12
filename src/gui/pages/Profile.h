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

#include "gui/app/Reach.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/Chip.h"
#include "gui/toolkit/controls/Fact.h"
#include "gui/toolkit/controls/Field.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/controls/Pill.h"
#include "gui/toolkit/controls/Segmented.h"
#include "gui/toolkit/controls/Select.h"
#include "gui/toolkit/controls/Stepper.h"
#include "gui/toolkit/controls/Toggle.h"
#include "gui/toolkit/layout/Box.h"
#include "gui/toolkit/layout/Scroll.h"
#include "gui/toolkit/layout/Wrap.h"

namespace pages {

// What one profile launches, and everything that hangs off it.
class ProfilePage : public toolkit::Widget {
public:
    explicit ProfilePage(Reach *reach);

    void sync();

private:
    // A panel that folds away, with a control of its own on the heading row.
    class Fold;

    // The add-on list, which reorders by being dragged.
    class Files;

    // The profile chooser at the top of the page.
    class Chooser;

    void buildRun(toolkit::Box *into);
    void buildReplay(toolkit::Box *into);
    void buildSaves(toolkit::Box *into);
    void buildNet(toolkit::Box *into);
    void buildCommand(toolkit::Box *into);

    void syncRun();
    void syncReplay();
    void syncSaves();
    void syncNet();
    void syncCommand();

    void showMenu();

    // What the heading says under the name.
    [[nodiscard]] static std::string summary();
    [[nodiscard]] static std::string netSummary();
    [[nodiscard]] static std::string saveSummary();
    [[nodiscard]] static std::string replaySummary();
    [[nodiscard]] static std::string tuningSummary();

    static bool ready();

    Reach *_reach;

    // The head.
    Chooser *_chooser = nullptr;
    toolkit::GlyphButton *_terminal = nullptr;
    toolkit::GlyphButton *_cog = nullptr;
    toolkit::Button *_launch = nullptr;

    toolkit::Scroll *_scroll = nullptr;

    // Add-ons.
    toolkit::Pill *_loaded = nullptr;
    toolkit::GlyphButton *_addFiles = nullptr;
    toolkit::GlyphButton *_clearFiles = nullptr;
    Files *_files = nullptr;

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

    // Replay.
    Fold *_replay = nullptr;
    toolkit::Segmented *_replayMode = nullptr;
    toolkit::GlyphButton *_replayReset = nullptr;
    toolkit::Field *_replayName = nullptr;
    toolkit::Select *_replayFile = nullptr;
    toolkit::GlyphButton *_replayRefresh = nullptr;
    toolkit::GlyphButton *_replayBrowse = nullptr;
    toolkit::Segmented *_replaySpeed = nullptr;
    toolkit::Select *_complevel = nullptr;
    toolkit::Toggle *_longtics = nullptr;
    toolkit::Toggle *_soloNet = nullptr;
    toolkit::Label *_replayNote = nullptr;
    toolkit::Label *_replayPath = nullptr;
    toolkit::Box *_replayRecord = nullptr;
    toolkit::Box *_replayPlay = nullptr;
    toolkit::Box *_replayTune = nullptr;

    // Saves.
    Fold *_saves = nullptr;
    toolkit::Segmented *_saveOn = nullptr;
    toolkit::Select *_saveFile = nullptr;
    toolkit::GlyphButton *_saveRefresh = nullptr;
    toolkit::Label *_saveNote = nullptr;
    toolkit::Label *_savePath = nullptr;

    // Multiplayer.
    Fold *_net = nullptr;
    toolkit::Segmented *_role = nullptr;
    toolkit::GlyphButton *_netReset = nullptr;
    toolkit::Segmented *_gameType = nullptr;
    toolkit::Stepper *_players = nullptr;
    toolkit::Field *_netPort = nullptr;
    toolkit::Toggle *_listed = nullptr;
    toolkit::Field *_host = nullptr;
    toolkit::Field *_joinPort = nullptr;
    toolkit::Field *_fragLimit = nullptr;
    toolkit::Field *_timeLimit = nullptr;
    toolkit::Field *_dmflags = nullptr;
    toolkit::Field *_dmflags2 = nullptr;
    toolkit::Field *_savegame = nullptr;
    toolkit::Label *_netNote = nullptr;
    toolkit::Box *_hosting = nullptr;
    toolkit::Box *_joining = nullptr;
    toolkit::Box *_rules = nullptr;
    toolkit::Box *_tuning = nullptr;
    toolkit::Segmented *_netmode = nullptr;
    toolkit::Stepper *_dup = nullptr;
    toolkit::Segmented *_extratic = nullptr;
    toolkit::Label *_tuningSaid = nullptr;

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
