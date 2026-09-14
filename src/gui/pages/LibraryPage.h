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

#include "gui/components/Reach.h"
#include "gui/components/LibraryCard.h"
#include "gui/state/State.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/Field.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/controls/MultistateSwitch.h"
#include "gui/toolkit/controls/Select.h"
#include "gui/toolkit/layout/Box.h"
#include "gui/toolkit/layout/ReorderGrid.h"
#include "gui/toolkit/layout/Scroll.h"

namespace pages {

// The heading, the tools, and a grid of cards that reorder by being
// carried to a gap.
//
// The model only changes once the carried card has walked to where it is going,
// which is what keeps the neighbours from shuffling twice.
class LibraryPage : public toolkit::Widget {
public:
    explicit LibraryPage(Reach *reach);

    // Rebuilds the cards from the state.
    void sync();

    bool advance(double now) override;

    [[nodiscard]] static bool profiles();

private:
    // The scrolled part: the cards, the tile that adds one, and what is said when
    // a filter matches nothing.
    class Shelf;

    // The one place the list changes.
    void land();

    void buildProfile(components::LibraryCard *card, const State::ProfileCard &profile, int index);
    void buildGame(components::LibraryCard *card, const State::NameRow &game, int index);

    // The "new profile" tile at the end of the grid.
    void paintAdder(const toolkit::Painter &painter, bool lit) const;

    void addPressed() const;

    void paintNothing(const toolkit::Painter &painter) const;

    [[nodiscard]] BLRect adderBox() const;

    Reach *_reach;

    toolkit::Box *_head = nullptr;
    toolkit::Label *_title = nullptr;
    toolkit::Label *_note = nullptr;
    toolkit::Box *_tools = nullptr;
    toolkit::Button *_addPort = nullptr;
    toolkit::Select *_port = nullptr;
    toolkit::MultistateSwitch *_shelf = nullptr;
    toolkit::Field *_filter = nullptr;

    toolkit::Scroll *_scroll = nullptr;
    Shelf *_grid = nullptr;

    std::vector<components::LibraryCard *> _cards;

    // What the cards were built from, so a sync only rebuilds on a real change.
    struct Mark {
        State::Shelf shelf{};
        int shelfRev = -1;
        int gameRev = -1;
        bool paths = false;

        bool operator==(const Mark &) const = default;
    };

    Mark _mark;

    // The last title screen the cards were repainted for.
    int _artRev = -1;

    // The last run state the cards' pills were set from.
    int _runRev = -1;

    toolkit::ReorderGrid _reorder{this};

    // Which card is in hand, by id: the list may be rebuilt under a drag.
    std::string _carrying;
};

}
