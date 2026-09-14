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

#include "gui/components/Reach.h"
#include "gui/toolkit/controls/Chip.h"
#include "gui/toolkit/controls/Field.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/controls/Toggle.h"
#include "gui/toolkit/layout/Panel.h"
#include "gui/toolkit/layout/Wrap.h"

namespace components {

// The command line the profile launches with: the extra arguments on the generated
// one, or a custom one written in its place, and what either comes out to.
class CommandPanel : public toolkit::Panel {
public:
    explicit CommandPanel(Reach *reach);

    void sync() const;

private:
    Reach *_reach;

    toolkit::Toggle *_override = nullptr;
    toolkit::Field *_extra = nullptr;
    toolkit::Field *_command = nullptr;
    toolkit::Wrap *_tokens = nullptr;
    toolkit::Chip *_budget = nullptr;
    toolkit::Label *_resolved = nullptr;
    toolkit::GlyphButton *_copy = nullptr;
};

}
