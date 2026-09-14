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

#include "gui/components/Reach.h"
#include "gui/toolkit/controls/Field.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/controls/MultistateSwitch.h"
#include "gui/toolkit/controls/Stepper.h"
#include "gui/toolkit/controls/Toggle.h"
#include "gui/toolkit/layout/Box.h"
#include "gui/toolkit/layout/Collapsible.h"

namespace components {

// The side this profile is on, the game it opens, and the connection under both.
class NetPanel : public toolkit::CollapsiblePanel {
public:
    explicit NetPanel(Reach *reach);

    void sync();

private:
    // What the heading says when the panel is folded.
    [[nodiscard]] static std::string summary();

    // What the connection heading says while it is folded away.
    [[nodiscard]] static std::string tuningSummary();

    Reach *_reach;

    toolkit::MultistateSwitch *_role = nullptr;
    toolkit::GlyphButton *_reset = nullptr;
    toolkit::MultistateSwitch *_gameType = nullptr;
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
    toolkit::Label *_saveClash = nullptr;
    toolkit::Label *_note = nullptr;
    toolkit::Box *_hosting = nullptr;
    toolkit::Box *_joining = nullptr;
    toolkit::Box *_rules = nullptr;
    toolkit::Box *_tuning = nullptr;
    toolkit::DisclosureHeading *_tuningHead = nullptr;
    toolkit::MultistateSwitch *_netmode = nullptr;
    toolkit::Stepper *_dup = nullptr;
    toolkit::MultistateSwitch *_extratic = nullptr;
};

}
