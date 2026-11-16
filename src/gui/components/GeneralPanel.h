/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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

#include <array>
#include <string>

#include "ttk/toolkit/controls/Fact.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/MultistateSwitch.h"
#include "ttk/toolkit/controls/Toggle.h"
#include "ttk/toolkit/layout/Box.h"
#include "ttk/toolkit/layout/Collapsible.h"

#include "core/config/Options.h"
#include "gui/components/Reach.h"

namespace components {

// How the port runs the profile: level stats, autoload, sound, window or fullscreen, and where screenshots go.
class GeneralPanel : public ttk::CollapsiblePanel {
public:
    explicit GeneralPanel(Reach *reach);

    void sync();

private:
    void pickFolder() const;

    Reach *_reach;

    ttk::Box *_files = nullptr;
    ttk::Box *_sound = nullptr;
    ttk::Box *_display = nullptr;
    ttk::MultistateSwitch *_mode = nullptr;
    ttk::Box *_screenshots = nullptr;
    ttk::MultistateSwitch *_shots = nullptr;
    ttk::Fact *_where = nullptr;
    ttk::Field *_folder = nullptr;

    // The toggles of the files and sound options. Null for the other rows.
    std::array<ttk::Toggle *, Options::ALL.size()> _toggles{};
};

}
