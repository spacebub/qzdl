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

#include "gui/app/Reach.h"
#include "gui/components/LibraryCard.h"
#include "gui/model/State.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/Field.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/controls/Segmented.h"
#include "gui/toolkit/controls/Select.h"
#include "gui/toolkit/layout/Box.h"
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

    // Worked out from the width the shelf is given, and used by both of them.
    void measure(double width);

    [[nodiscard]] double cellX(int index) const;
    [[nodiscard]] double cellY(int index) const;

    // Where a card carried to this point would go.
    [[nodiscard]] int placeAt(double x, double y) const;

    // Cards the carried one passed shuffle up behind it.
    [[nodiscard]] int slot(int index) const;

    void grabbed(int index, double x, double y);
    void carried(double x, double y);
    void dropped();

    // The one place the list changes.
    void land();

    void buildProfile(components::LibraryCard *card, const State::ProfileCard &profile, int index);
    void buildGame(components::LibraryCard *card, const State::NameRow &game, int index);

    // The "new profile" tile at the end of the grid.
    void paintAdder(const toolkit::Painter &painter, bool lit) const;

    void addPressed();

    void paintNothing(const toolkit::Painter &painter) const;

    [[nodiscard]] BLRect adderBox() const;

    Reach *_reach;

    toolkit::Box *_head = nullptr;
    toolkit::Label *_title = nullptr;
    toolkit::Label *_note = nullptr;
    toolkit::Box *_tools = nullptr;
    toolkit::Button *_addPort = nullptr;
    toolkit::Select *_port = nullptr;
    toolkit::Segmented *_shelf = nullptr;
    toolkit::Field *_filter = nullptr;

    toolkit::Scroll *_scroll = nullptr;
    Shelf *_grid = nullptr;

    std::vector<components::LibraryCard *> _cards;

    // What the cards were built from, so a sync only rebuilds on a real change.
    std::string _mark;

    // The last title screen the cards were repainted for.
    int _artRev = -1;

    int _columns = 1;
    double _cell = Theme::cardWidth;

    // The carried card and where it is headed.
    std::string _carrying;
    int _origin = -1;
    int _target = -1;
    bool _dragging = false;

    double _grabX = 0.0;
    double _grabY = 0.0;

    Anim::Tween _carryX;
    Anim::Tween _carryY;

    // Non-zero while the card walks to its gap.
    double _landing = 0.0;
};

}
