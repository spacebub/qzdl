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

#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/MultistateSwitch.h"
#include "ttk/toolkit/controls/Stepper.h"
#include "ttk/toolkit/controls/Toggle.h"
#include "ttk/toolkit/layout/Box.h"
#include "ttk/toolkit/layout/Collapsible.h"

#include "gui/components/Reach.h"

namespace components {

// The side this profile is on, the game it opens, and the connection under both.
class NetPanel : public ttk::CollapsiblePanel {
public:
    explicit NetPanel(Reach *reach);

    void sync();

private:
    // What the heading says when the panel is folded.
    [[nodiscard]] static std::string summary();

    // What the connection heading says while it is folded away.
    [[nodiscard]] static std::string tuningSummary();

    Reach *_reach;

    ttk::MultistateSwitch *_role = nullptr;
    ttk::GlyphButton *_reset = nullptr;
    ttk::MultistateSwitch *_gameType = nullptr;
    ttk::Stepper *_players = nullptr;
    ttk::Field *_netPort = nullptr;
    ttk::Toggle *_listed = nullptr;
    ttk::Field *_host = nullptr;
    ttk::Field *_joinPort = nullptr;
    ttk::Field *_fragLimit = nullptr;
    ttk::Field *_timeLimit = nullptr;
    ttk::Field *_dmflags = nullptr;
    ttk::Field *_dmflags2 = nullptr;
    ttk::Field *_savegame = nullptr;
    ttk::Label *_saveClash = nullptr;
    ttk::Label *_note = nullptr;
    ttk::Box *_hosting = nullptr;
    ttk::Box *_joining = nullptr;
    ttk::Box *_rules = nullptr;
    ttk::Box *_tuning = nullptr;
    ttk::DisclosureHeading *_tuningHead = nullptr;
    ttk::MultistateSwitch *_netmode = nullptr;
    ttk::Stepper *_dup = nullptr;
    ttk::MultistateSwitch *_extratic = nullptr;
};

}
