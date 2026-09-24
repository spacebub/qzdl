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

#include "ttk/toolkit/controls/Chip.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/Toggle.h"
#include "ttk/toolkit/layout/Panel.h"
#include "ttk/toolkit/layout/Wrap.h"

#include "gui/components/Reach.h"

namespace components {

// The command line the profile launches with: the extra arguments on the generated
// one, or a custom one written in its place, and what either comes out to.
class CommandPanel : public ttk::Panel {
public:
    explicit CommandPanel(Reach *reach);

    void sync() const;

private:
    Reach *_reach;

    ttk::Toggle *_override = nullptr;
    ttk::Field *_extra = nullptr;
    ttk::Field *_command = nullptr;
    ttk::Wrap *_tokens = nullptr;
    ttk::Chip *_budget = nullptr;
    ttk::Label *_resolved = nullptr;
    ttk::GlyphButton *_copy = nullptr;
};

}
