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
#include "ttk/toolkit/controls/Select.h"
#include "ttk/toolkit/controls/Toggle.h"
#include "ttk/toolkit/layout/Box.h"
#include "ttk/toolkit/layout/Collapsible.h"

#include "gui/components/Reach.h"

namespace components {

// What the profile records into, or plays back, and the compatibility a recording
// is made under.
class ReplayPanel : public ttk::CollapsiblePanel {
public:
    explicit ReplayPanel(Reach *reach);

    void sync();

private:
    // What the heading says when the panel is folded.
    [[nodiscard]] static std::string summary();

    Reach *_reach;

    ttk::MultistateSwitch *_mode = nullptr;
    ttk::GlyphButton *_reset = nullptr;
    ttk::Field *_name = nullptr;
    ttk::Select *_file = nullptr;
    ttk::GlyphButton *_refresh = nullptr;
    ttk::GlyphButton *_browse = nullptr;
    ttk::MultistateSwitch *_speed = nullptr;
    ttk::Select *_complevel = nullptr;
    ttk::Toggle *_longtics = nullptr;
    ttk::Toggle *_soloNet = nullptr;
    ttk::Label *_note = nullptr;
    ttk::Label *_path = nullptr;
    ttk::Box *_record = nullptr;
    ttk::Box *_play = nullptr;
    ttk::Box *_tune = nullptr;
};

}
