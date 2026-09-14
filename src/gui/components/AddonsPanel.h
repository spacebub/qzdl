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

#include "gui/components/AddonList.h"
#include "gui/components/Reach.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/controls/Pill.h"
#include "gui/toolkit/layout/Panel.h"

namespace components {

// The add-on list with its heading: how many are loaded, and the way to add or
// clear them.
class AddonsPanel : public toolkit::Panel {
public:
    // Narrower than this and the names are unreadable.
    static constexpr double LEAST = 260.0;

    explicit AddonsPanel(Reach *reach);

    void sync() const;

private:
    Reach *_reach;

    toolkit::Pill *_loaded = nullptr;
    toolkit::GlyphButton *_clearFiles = nullptr;
    AddonList *_files = nullptr;
};

}
