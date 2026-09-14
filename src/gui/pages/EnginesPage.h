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
#include <vector>

#include "gui/components/EngineCard.h"
#include "gui/components/Reach.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/Fact.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/controls/MultistateSwitch.h"
#include "gui/toolkit/layout/ReorderGrid.h"
#include "gui/toolkit/layout/Scroll.h"

namespace pages {

// The source ports that are set up, and
// the ones that can be fetched.
class EnginesPage : public toolkit::Widget {
public:
    explicit EnginesPage(Reach *reach);

    void sync();

    bool advance(double now) override;

    [[nodiscard]] static bool installed();

private:

    // One card on either shelf, built from the row it shows.
    class Shelf;

    void rebuild();

    // The one place the list changes.
    void land();

    [[nodiscard]] BLRect adderBox() const;

    Reach *_reach;

    toolkit::Label *_title = nullptr;
    toolkit::Label *_note = nullptr;
    toolkit::Button *_recheck = nullptr;
    toolkit::MultistateSwitch *_which = nullptr;

    toolkit::Scroll *_scroll = nullptr;
    Shelf *_grid = nullptr;

    std::vector<components::EngineCard *> _cards;

    std::string _mark;

    toolkit::ReorderGrid _reorder{this};

    // Which card is in hand, by id: the list may be rebuilt under a drag.
    std::string _carrying;

    // GitHub is only asked once the browse shelf is opened.
    bool _asked = false;

    // Where the ports are kept, under the browse shelf.
    toolkit::Fact *_kept = nullptr;
};

}
