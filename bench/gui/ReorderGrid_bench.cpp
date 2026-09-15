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

#include <cstddef>
#include <vector>

#include <benchmark/benchmark.h>
#include <blend2d/blend2d.h>

#include "gui/toolkit/Widget.h"
#include "gui/toolkit/layout/ReorderGrid.h"

namespace {

// What offsets() reads off a card.
struct Card {
    double x = 0.0;
    double y = 0.0;

    [[nodiscard]] double slideX() const { return x; }
    [[nodiscard]] double slideY() const { return y; }
};

// The library shelf's width, with the first card in hand two cells to the right of
// its own: the neighbours between are shuffled, which is the drag's usual shape.
class Shelf {
public:
    explicit Shelf(const int count) : _cards(static_cast<size_t>(count)) {
        grid.place(BLRect{0.0, 0.0, 1180.0, 800.0});

        for (Card &card : _cards) {
            held.push_back(&card);
        }

        grid.grabbed(0, grid.cellX(0) + 40.0, grid.cellY(0) + 40.0);
        grid.carried(count, grid.cellX(2) + 40.0, grid.cellY(0) + 40.0);
    }

    toolkit::Widget page;
    toolkit::ReorderGrid grid{&page};

    std::vector<Card *> held;

private:
    std::vector<Card> _cards;
};

// A resize hands the grid a new width every frame.
void ReorderGrid_measure(benchmark::State &state) {
    Shelf shelf(64);

    double width = 1180.0;
    double direction = -4.0;

    for ([[maybe_unused]] auto step : state) {
        width += direction;

        if (width < 900.0 || width > 1400.0) {
            direction = -direction;
        }

        shelf.grid.measure(width);

        benchmark::DoNotOptimize(shelf.grid.cell());
    }
}

BENCHMARK(ReorderGrid_measure);

// What the shelf's arrange reads per card per frame of a drag: the slot it shows
// in and where that cell is.
void ReorderGrid_cellOf(benchmark::State &state) {
    const auto count = static_cast<int>(state.range(0));

    Shelf shelf(count);

    for ([[maybe_unused]] auto step : state) {
        for (int index = 0; index < count; ++index) {
            const int at = shelf.grid.slot(index);

            benchmark::DoNotOptimize(shelf.grid.cellX(at));
            benchmark::DoNotOptimize(shelf.grid.cellY(at));
        }
    }

    state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(ReorderGrid_cellOf)->Arg(8)->Arg(64)->Arg(512);

// One pointer report while a card is in hand: the target under it and the carry.
void ReorderGrid_carried(benchmark::State &state) {
    Shelf shelf(64);

    const double left = shelf.grid.cellX(0);
    const double y = shelf.grid.cellY(1) + 40.0;
    double x = left;

    for ([[maybe_unused]] auto step : state) {
        x = x > left + 900.0 ? left : x + 14.0;

        shelf.grid.carried(64, x, y);

        benchmark::DoNotOptimize(shelf.grid.target());
    }
}

BENCHMARK(ReorderGrid_carried);

// The drop: where every card stands, by the index it is about to take.
void ReorderGrid_offsets(benchmark::State &state) {
    const auto count = static_cast<int>(state.range(0));

    Shelf shelf(count);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(shelf.grid.offsets(shelf.held));
    }

    state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(ReorderGrid_offsets)->Arg(8)->Arg(64)->Arg(512);

}
