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
#include <utility>
#include <vector>

#include <blend2d/blend2d.h>

#include "gui/draw/Theme.h"
#include "gui/toolkit/Widget.h"

namespace toolkit {

// The arithmetic behind a shelf of cards that reorders by being dragged: how many
// columns fit, where a cell sits, which slot each card shows in while one is in
// hand, and where every one of them stands once the drag ends.
//
// The shelves differ in their metrics and in what a finished drag does with the two
// indices. Everything between those two ends is this.
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

    [[nodiscard]] double carryX() const { return _carryX; }
    [[nodiscard]] double carryY() const { return _carryY; }

    // Where each card is drawn, by the index it is about to take, as an offset
    // from that index's cell. Valid only while the drag is still on the grid.
    template <typename Card>
    [[nodiscard]] std::vector<BLPoint> offsets(const std::vector<Card *> &cards) const {
        std::vector<BLPoint> where(cards.size());

        for (int index = 0; std::cmp_less(index, cards.size()); ++index) {
            const int at = index == _origin ? _target : slot(index);

            if (at < 0 || std::cmp_greater_equal(at, cards.size())) {
                continue;
            }

            const Card *card = cards[static_cast<size_t>(index)];

            where[static_cast<size_t>(at)] = index == _origin
                ? BLPoint{cellX(_origin) + _carryX + card->slideX() - cellX(_target),
                          cellY(_origin) + _carryY + card->slideY() - cellY(_target)}
                : BLPoint{card->slideX(), card->slideY()};
        }

        return where;
    }

    void grabbed(const int index, const double x, const double y) {
        _dragging = true;
        _origin = index;
        _target = index;
        _grabX = x - cellX(index);
        _grabY = y - cellY(index);

        _carryX = 0.0;
        _carryY = 0.0;
    }

    void carried(const int count, const double x, const double y) {
        _target = placeAt(count, x, y);

        _carryX = x - _grabX - cellX(_origin);
        _carryY = y - _grabY - cellY(_origin);

        _page->wake();
    }

    void landed() {
        _dragging = false;
        _origin = -1;
        _target = -1;
        _carryX = 0.0;
        _carryY = 0.0;
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

    double _carryX = 0.0;
    double _carryY = 0.0;
};

}
