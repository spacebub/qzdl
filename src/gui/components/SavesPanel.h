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
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/controls/MultistateSwitch.h"
#include "gui/toolkit/controls/Select.h"
#include "gui/toolkit/layout/Collapsible.h"

namespace components {

// The save this profile starts from, out of the ones in its folder.
class SavesPanel : public toolkit::CollapsiblePanel {
public:
    explicit SavesPanel(Reach *reach);

    void sync();

private:
    // What the heading says when the panel is folded.
    [[nodiscard]] static std::string summary();

    Reach *_reach;

    toolkit::MultistateSwitch *_on = nullptr;
    toolkit::Select *_file = nullptr;
    toolkit::GlyphButton *_refresh = nullptr;
    toolkit::Label *_note = nullptr;
    toolkit::Label *_path = nullptr;
};

}
