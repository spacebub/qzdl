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
#include "gui/toolkit/controls/Select.h"
#include "gui/toolkit/controls/Toggle.h"
#include "gui/toolkit/layout/Box.h"
#include "gui/toolkit/layout/Collapsible.h"

namespace components {

// What the profile records into, or plays back, and the compatibility a recording
// is made under.
class ReplayPanel : public toolkit::CollapsiblePanel {
public:
    explicit ReplayPanel(Reach *reach);

    void sync();

    // What the heading says when the panel is folded.
    [[nodiscard]] static std::string summary();

private:
    Reach *_reach;

    toolkit::MultistateSwitch *_mode = nullptr;
    toolkit::GlyphButton *_reset = nullptr;
    toolkit::Field *_name = nullptr;
    toolkit::Select *_file = nullptr;
    toolkit::GlyphButton *_refresh = nullptr;
    toolkit::GlyphButton *_browse = nullptr;
    toolkit::MultistateSwitch *_speed = nullptr;
    toolkit::Select *_complevel = nullptr;
    toolkit::Toggle *_longtics = nullptr;
    toolkit::Toggle *_soloNet = nullptr;
    toolkit::Label *_note = nullptr;
    toolkit::Label *_path = nullptr;
    toolkit::Box *_record = nullptr;
    toolkit::Box *_play = nullptr;
    toolkit::Box *_tune = nullptr;
};

}
