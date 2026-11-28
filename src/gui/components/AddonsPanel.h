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

#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/MultistateSwitch.h"
#include "ttk/toolkit/controls/Pill.h"
#include "ttk/toolkit/layout/Box.h"
#include "ttk/toolkit/layout/Panel.h"

#include "gui/components/AddonList.h"
#include "gui/components/Reach.h"

namespace components {

// The add-on list with its heading: how many are loaded, and the way to add or
// clear them. The file browser folds under it.
class AddonsPanel : public ttk::Panel {
public:
    // Narrower than this and the names are unreadable.
    static constexpr double LEAST = 260.0;

    explicit AddonsPanel(Reach *reach);

    void sync() const;

    // While the groups are on, wide enough for the whole head.
    [[nodiscard]] double least(ttk::Typeface &type) const;

private:
    class Split;

    Reach *_reach;

    ttk::Box *_head = nullptr;
    ttk::Label *_title = nullptr;
    ttk::Widget *_spacer = nullptr;
    ttk::MultistateSwitch *_view = nullptr;
    ttk::Label *_newGroup = nullptr;
    ttk::Pill *_loaded = nullptr;
    ttk::GlyphButton *_clearFiles = nullptr;
    Split *_split = nullptr;
    ttk::Label *_note = nullptr;
};

}
