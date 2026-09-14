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

#include <algorithm>
#include <cmath>

#include <blend2d/blend2d.h>

#include "gui/draw/Anim.h"
#include "gui/draw/Theme.h"
#include "gui/toolkit/Widget.h"

namespace toolkit {

// The arithmetic behind a shelf of cards that reorders by being dragged: how many
// columns fit, where a cell sits, which slot each card shows in while one is in
// hand, and the tween that walks the carried one into its gap.
//
// The shelves differ in their metrics and in what a finished drag does with the two
// indices; everything between those two ends is this.
class ReorderGrid {
public:
    // The page's own spacing. `narrowest` is the width below which a column is
    // dropped, and `step` is what the cell width is rounded down to.
    struct Metrics {
        double bleed = Theme::bleed;
        double gutter = Theme::gutter;
        double top = Theme::shelfTop;
        double narrowest = Theme::cardWidth;
        double rowHeight = Theme::rowHeight;
        double step = Theme::cardStep;
    };

    // The page is what animates: it is woken while a card walks, and its clock
    // is the one the walk runs on.
    explicit ReorderGrid(Widget *page) : _page(page) {}

    void setMetrics(const Metrics &metrics) { _metrics = metrics; }

    void setRowHeight(const double height) { _metrics.rowHeight = height; }

    void measure(const double width) {
        const double room = std::max(_metrics.narrowest, width - (_metrics.bleed * 2.0));

        _columns = std::max(1, static_cast<int>(std::floor((room + _metrics.gutter)
                                                           / (_metrics.narrowest
                                                              + _metrics.gutter))));

        // Stepped: every distinct width is a card sprite and a shadow sprite built
        // from scratch, and a drag walks through one per frame. The row already ends
        // short of the header by up to a column's rounding.
        _cell = std::floor((room - ((_columns - 1) * _metrics.gutter)) / _columns
                           / _metrics.step)
            * _metrics.step;
    }

    // Where the shelf was put, which every cell is measured from.
    void place(const BLRect &box) {
        _box = box;

        measure(box.w);
    }

    [[nodiscard]] int columns() const { return _columns; }
    [[nodiscard]] double cell() const { return _cell; }
    [[nodiscard]] double rowHeight() const { return _metrics.rowHeight; }

    [[nodiscard]] int rowsFor(const int count) const {
        return (count + _columns - 1) / _columns;
    }

    // With room for the tile that adds one, which always follows the last card.
    [[nodiscard]] int rowsWithAdder(const int count) const {
        return (count / _columns) + 1;
    }

    [[nodiscard]] double cellX(const int index) const {
        return _box.x + _metrics.bleed + ((index % _columns) * (_cell + _metrics.gutter));
    }

    [[nodiscard]] double cellY(const int index) const {
        const int row = index / _columns;

        return _box.y + _metrics.top + (row * (_metrics.rowHeight + _metrics.gutter));
    }

    // Where a card shows while another is carried: the ones between the two ends
    // shuffle up or down by one.
    [[nodiscard]] int slot(const int index) const {
        if (_origin < 0 || index == _origin) {
            return index;
        }

        if (_origin < _target && index > _origin && index <= _target) {
            return index - 1;
        }

        if (_origin > _target && index >= _target && index < _origin) {
            return index + 1;
        }

        return index;
    }

    [[nodiscard]] bool dragging() const { return _dragging; }
    [[nodiscard]] int origin() const { return _origin; }
    [[nodiscard]] int target() const { return _target; }

    [[nodiscard]] double carryX() const { return _carryX.value(); }
    [[nodiscard]] double carryY() const { return _carryY.value(); }

    [[nodiscard]] bool carrying() const { return _carryX.live() || _carryY.live(); }

    // Whether the page has anything to animate: a drag, a walk, or a landing due.
    [[nodiscard]] bool busy() const { return carrying() || _landing > 0.0 || _dragging; }

    // Whether the dropped card has reached its gap, so the list can change.
    [[nodiscard]] bool due(const double now) const { return _landing > 0.0 && now >= _landing; }

    void grabbed(const int index, const double x, const double y) {
        _dragging = true;
        _origin = index;
        _target = index;
        _grabX = x - cellX(index);
        _grabY = y - cellY(index);

        _carryX.set(0.0F);
        _carryY.set(0.0F);
    }

    void carried(const int count, const double x, const double y) {
        _target = placeAt(count, x, y);

        _carryX.set(static_cast<float>(x - _grabX - cellX(_origin)));
        _carryY.set(static_cast<float>(y - _grabY - cellY(_origin)));

        _page->wake();
    }

    // The card walks to its gap; only then does the list change, once due() says
    // the walk has settled.
    void dropped() {
        _dragging = false;

        if (_origin < 0) {
            return;
        }

        const double now = _page->now();

        _carryX.run(static_cast<float>(cellX(_target) - cellX(_origin)), now, Theme::settling,
                    Anim::Curve::CubicOut);
        _carryY.run(static_cast<float>(cellY(_target) - cellY(_origin)), now, Theme::settling,
                    Anim::Curve::CubicOut);

        _landing = now + Theme::settling + 0.02;

        _page->wake();
    }

    void landed() {
        _landing = 0.0;
        _origin = -1;
        _target = -1;

        _carryX.set(0.0F);
        _carryY.set(0.0F);
    }

    void advance(const double now) {
        _carryX.advance(now);
        _carryY.advance(now);
    }

private:
    // The index the pointer is over, clamped to the shelf.
    [[nodiscard]] int placeAt(const int count, const double x, const double y) const {
        if (count == 0) {
            return 0;
        }

        const int row = static_cast<int>(std::floor((y - _box.y - _metrics.top)
                                                    / (_metrics.rowHeight + _metrics.gutter)));
        const int column = std::clamp(
            static_cast<int>(std::floor((x - _box.x - _metrics.bleed)
                                        / (_cell + _metrics.gutter))),
            0, _columns - 1);

        return std::clamp((row * _columns) + column, 0, count - 1);
    }

    Widget *_page;

    Metrics _metrics;

    BLRect _box{};

    int _columns = 1;
    double _cell = Theme::cardWidth;

    int _origin = -1;
    int _target = -1;
    bool _dragging = false;

    // Where in the card the pointer took hold.
    double _grabX = 0.0;
    double _grabY = 0.0;

    // When the dropped card is due in its gap; zero while nothing is walking.
    double _landing = 0.0;

    Anim::Tween _carryX;
    Anim::Tween _carryY;
};

}
