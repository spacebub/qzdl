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

#include <functional>
#include <string_view>
#include <vector>

#include <blend2d/blend2d.h>

#include "ttk/draw/Anim.h"
#include "ttk/toolkit/Painter.h"

namespace Band {

// The header line of a group, and the strip above the cards of a shelf section that holds it.
constexpr double LINE = 30.0;
constexpr double STRIP = 36.0;

// The lane of the fold chevron at the left end, and the menu button at the right end.
constexpr double LANE = 22.0;
constexpr double BUTTON = 28.0;

struct Header {
    std::string_view caption;
    std::string_view count;

    // Where the name starts, from the left end.
    double indent = LANE;

    // How far the group shows, from 0 folded to 1 open. It turns the chevron.
    float open = 1.0F;

    // A group, which folds and has a menu. The ungrouped rest does neither.
    bool named = false;
    bool over = false;
    bool menu = false;
    bool overMenu = false;

    // The target of a carry.
    bool lit = false;
};

// The caption of a group over a rule to the right end.
void paint(const ttk::Painter &painter, const BLRect &box, const Header &header);

[[nodiscard]] BLRect menuBox(const BLRect &box);

[[nodiscard]] bool onMenu(const BLRect &box, double x, double y);

// The groups sliding open or shut, by group id. A group folds in the config only once it has slid shut, so its
// rows and cards stay on screen while they slide away.
class Folds {
public:
    // Turns the group the other way from where it is headed. `fold` is called with false at once to open a folded
    // group, and with true once it has slid shut.
    void turn(int group, bool collapsed, double now, std::function<void(bool)> fold);

    [[nodiscard]] float open(int group, bool collapsed) const;

    // Steps the slides and folds the groups that slid shut. Returns whether any still slides.
    bool advance(double now);

    // Drops the slides that have ended, once the lists show the folds they made.
    void settled();

    [[nodiscard]] bool empty() const { return _slides.empty(); }

private:
    struct Slide {
        int group = 0;
        bool open = false;
        ttk::Anim::Tween shown;
        std::function<void(bool)> fold;
    };

    std::vector<Slide> _slides;
};

}
