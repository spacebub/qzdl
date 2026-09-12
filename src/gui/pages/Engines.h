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
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/Fact.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/controls/Segmented.h"
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
    class Card;
    class Shelf;

    void rebuild();

    // Worked out from the width the shelf is given, and used by both of them.
    void measure(double width);

    [[nodiscard]] double cellX(int index) const;
    [[nodiscard]] double cellY(int index) const;

    [[nodiscard]] int placeAt(double x, double y) const;
    [[nodiscard]] int slot(int index) const;

    void grabbed(int index, double x, double y);
    void carried(double x, double y);
    void dropped();
    void land();

    [[nodiscard]] BLRect adderBox() const;

    Reach *_reach;

    toolkit::Label *_title = nullptr;
    toolkit::Label *_note = nullptr;
    toolkit::Button *_recheck = nullptr;
    toolkit::Segmented *_which = nullptr;

    toolkit::Scroll *_scroll = nullptr;
    Shelf *_grid = nullptr;

    std::vector<Card *> _cards;

    std::string _mark;

    int _columns = 1;
    double _cell = 330.0;
    double _rowHeight = 152.0;

    std::string _carrying;
    int _origin = -1;
    int _target = -1;
    bool _dragging = false;

    double _grabX = 0.0;
    double _grabY = 0.0;

    Anim::Tween _carryX;
    Anim::Tween _carryY;

    double _landing = 0.0;

    // GitHub is only asked once the browse shelf is opened.
    bool _asked = false;

    // Where the ports are kept, under the browse shelf.
    toolkit::Fact *_kept = nullptr;
};

}
