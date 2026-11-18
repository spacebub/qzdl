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

#include <cstdint>
#include <span>
#include <vector>

#include <blend2d/blend2d.h>

#include "ttk/draw/Anim.h"
#include "ttk/toolkit/Painter.h"

#include "gui/state/State.h"

namespace components {

// The badges on a card: as many as fit, and the rest buried behind a mark that fans them out while it is hovered.
// The card owns it and paints it, so it can be drawn into a sheet or through a turn.
class BadgeRow {
public:
    // Which end of the row the first badge to keep sits at. From the end, the last badge is kept first.
    enum class From : std::uint8_t {
        Start,
        End,
    };

    enum class Fan : std::uint8_t {
        Up,
        Down,
    };

    // Quiet draws the mark and the fanned badges muted. Clear gives the mark contrast and each badge its own tone.
    enum class Tone : std::uint8_t {
        Quiet,
        Clear,
    };

    struct Look {
        From from = From::Start;
        Fan fan = Fan::Up;
        Tone tone = Tone::Quiet;
        double gap = 6.0;

        // The first badge to keep shows even when it does not fit.
        bool keepFirst = false;
    };

    static constexpr size_t MOST = 5;

    explicit BadgeRow(const Look &look) : _look(look) {}

    // Lays the badges out in `row` and paints them with the mark. The fan stays inside `bounds`.
    void paint(const ttk::Painter &painter,
               const BLRect &row,
               const BLRect &bounds,
               std::span<const State::BadgeSpec> badges);

    // The buried badges, as far out as the fan has got. Called after paint() with the same badges.
    void paintFan(const ttk::Painter &painter, std::span<const State::BadgeSpec> badges) const;

    // True when the pointer has moved on or off the mark, and the fan has started its way.
    bool hover(double x, double y, double now);

    void leave(double now);

    void advance(double now) { _spill.advance(now); }

    [[nodiscard]] float open() const { return _spill.value(); }

    [[nodiscard]] bool live() const { return _spill.live(); }

    // Everything the fan covers at `open`, from the row and mark last painted.
    [[nodiscard]] BLRect fanBox(double open) const;

    // Moves what was last painted, for a card blitted from a kept image.
    void shift(double dx, double dy);

    // The badge shown in the row under the point, or -1.
    [[nodiscard]] int at(double x, double y) const;

    // The badges and the mark as last painted. Empty when none were.
    [[nodiscard]] BLRect extent() const;

    [[nodiscard]] int buried() const { return _buried; }

    [[nodiscard]] const BLRect &mark() const { return _mark; }

private:
    struct Slot {
        double x = 0.0;
        double span = 0.0;
        bool shown = false;
    };

    void layOut(const ttk::Painter &painter, const BLRect &row, std::span<const State::BadgeSpec> badges);

    // Calls visit(slot, x, line) for each buried badge, where it lands once the fan is all the way out. A fan
    // wider than the bounds wraps onto further lines away from the mark.
    template <typename Visit>
    void eachFanned(Visit &&visit) const;

    // A buried badge `span` wide on its way to `x` on `line`, with the fan `open` of the way out of the mark.
    [[nodiscard]] BLRect fanned(double x, double span, double open, int line) const;

    Look _look;

    // Where each badge sits, in badge order.
    std::vector<Slot> _slots;
    BLRect _row{};
    BLRect _bounds{};
    BLRect _mark{};
    double _lineHeight = 0.0;
    int _buried = 0;
    bool _over = false;

    ttk::Anim::Tween _spill;
};

}
