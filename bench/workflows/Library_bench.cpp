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
#include <string>

#include <benchmark/benchmark.h>

#include "gui/state/State.h"
#include "support/Canvas.h"
#include "support/Fixtures.h"
#include "support/Rig.h"

namespace {

// Installed unconditionally: every file here drives the same Session and State,
// so a cached fixture would make the numbers depend on which ran first.
bench::Rig &shelf(const int profiles) {
    bench::Rig &rig = bench::shared();

    rig.forget();

    bench::Fixtures::install(bench::Fixtures::config(profiles, 16));

    rig.config().reload();

    State::get().nav.shelf = State::Shelf::Profiles;

    rig.go(State::Page::Library);
    rig.sync();
    rig.ready();

    return rig;
}

// The middle of the first card, which sits at 66,156 and is 272 by 232. A point
// off the cards would leave the hover benchmarks measuring an empty shelf.
constexpr double CARD_X = 300.0;
constexpr double CARD_Y = 260.0;

void Library_sync(benchmark::State &state) {
    const auto profiles = static_cast<int>(state.range(0));

    bench::Rig &rig = shelf(profiles);

    for ([[maybe_unused]] auto step : state) {
        rig.sync();
    }

    state.SetItemsProcessed(state.iterations() * profiles);
}

BENCHMARK(Library_sync)->Arg(8)->Arg(64)->Arg(512);

// One keystroke in the filter: refilter, rebuild the shelf, lay out and paint.
void Library_filterKeystroke(benchmark::State &state) {
    const auto profiles = static_cast<int>(state.range(0));

    bench::Rig &rig = shelf(profiles);

    const std::string needle = "ancient";
    size_t at = 0;

    for ([[maybe_unused]] auto step : state) {
        rig.config().library().setFilter(needle.substr(0, (at++ % needle.size()) + 1));
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());
    }

    state.SetItemsProcessed(state.iterations() * profiles);
}

BENCHMARK(Library_filterKeystroke)->Arg(64)->Arg(512);

// A wheel notch and the frames it animates over.
void Library_scroll(benchmark::State &state) {
    bench::Rig &rig = shelf(64);

    toolkit::Root &root = rig.ui();
    std::size_t painted = 0;
    double steps = -1.0;

    for ([[maybe_unused]] auto step : state) {
        root.wheel(steps, CARD_X, CARD_Y);

        painted = 0;

        for (int frame = 0; frame < 8; ++frame) {
            painted += rig.canvas().frameAt(rig.canvas().tick());
        }

        steps = -steps;
    }

    state.counters["pixels"] = static_cast<double>(painted);
}

BENCHMARK(Library_scroll);

// The pointer crossing a card: the hover tween runs and the face turns.
void Library_hoverCard(benchmark::State &state) {
    bench::Rig &rig = shelf(64);

    toolkit::Root &root = rig.ui();
    double x = CARD_X;
    std::size_t painted = 0;

    double turning = 0.0;

    for ([[maybe_unused]] auto step : state) {
        x = x > CARD_X + 180.0 ? CARD_X : x + 4.0;

        root.motion(x, CARD_Y);

        painted += rig.canvas().frameAt(rig.canvas().tick());
        turning += rig.ui().busy() ? 1.0 : 0.0;
    }

    // Averaged: a hover mixes small frames with whole-shelf ones. Zero means the
    // pointer never landed on a card and the number is meaningless. `turning` is the
    // share of frames a card was still moving, which is the dear path: an average
    // alone hides a regression that moves frames from one path to the other.
    state.counters["pixels"] = static_cast<double>(painted) / static_cast<double>(state.iterations());
    state.counters["turning"] = turning / static_cast<double>(state.iterations());
}

BENCHMARK(Library_hoverCard);

// The pointer moving about inside one card, which is where it spends most of its
// time: the card is already lit, so the frame is its turn and its glow alone.
void Library_hoverWithin(benchmark::State &state) {
    bench::Rig &rig = shelf(64);

    toolkit::Root &root = rig.ui();

    root.motion(CARD_X, CARD_Y);

    for (int at = 0; at < 120; ++at) {
        rig.canvas().frameAt(rig.canvas().tick());
    }

    double x = CARD_X;
    std::size_t painted = 0;
    double turning = 0.0;

    for ([[maybe_unused]] auto step : state) {
        x = x > CARD_X + 60.0 ? CARD_X : x + 3.0;

        root.motion(x, CARD_Y);

        painted += rig.canvas().frameAt(rig.canvas().tick());
        turning += rig.ui().busy() ? 1.0 : 0.0;
    }

    state.counters["pixels"] = static_cast<double>(painted) / static_cast<double>(state.iterations());
    state.counters["turning"] = turning / static_cast<double>(state.iterations());
}

BENCHMARK(Library_hoverWithin);

// A card's run status changing, which is the common path while a game is running:
// the pill is repainted and the card's kept sheet is rebuilt from it.
void Library_cardStatus(benchmark::State &state) {
    bench::Rig &rig = shelf(64);

    const std::string key = State::get().cfg.shelfProfiles.front().key;
    const std::string name = State::get().cfg.shelfProfiles.front().name;

    std::size_t painted = 0;
    bool up = false;

    for ([[maybe_unused]] auto step : state) {
        up = !up;

        if (up) {
            rig.runs().refused(key, name, "nothing to launch");
        } else {
            rig.runs().close(key);
        }

        rig.sync();

        painted += rig.canvas().frameAt(rig.canvas().tick());
    }

    state.counters["pixels"] = static_cast<double>(painted)
        / static_cast<double>(state.iterations());
}

BENCHMARK(Library_cardStatus);

// Nothing moved: the damage list is empty and the frame costs the walk alone.
void Library_idleFrame(benchmark::State &state) {
    bench::Rig &rig = shelf(64);

    rig.canvas().full();

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(rig.canvas().frameAt(rig.canvas().tick()));
    }
}

BENCHMARK(Library_idleFrame);

// A card carried across the shelf: the one in hand moves with the pointer and the
// neighbours it passes walk to the gaps it leaves, so a frame damages many cells.
// `emptyFrames` is the share that found nothing to repaint while still in flight,
// which is what the shell would spend a turn of the loop on for no pixels.
void Library_dragCard(benchmark::State &state) {
    bench::Rig &rig = shelf(64);

    toolkit::Root &root = rig.ui();

    root.motion(CARD_X, CARD_Y);
    root.press(toolkit::Pointer{.x = CARD_X, .y = CARD_Y});

    double x = CARD_X;
    std::size_t painted = 0;
    std::size_t idle = 0;
    std::size_t live = 0;

    for ([[maybe_unused]] auto step : state) {
        x = x > CARD_X + 560.0 ? CARD_X : x + 14.0;

        root.motion(x, CARD_Y);

        painted = rig.canvas().frameAt(rig.canvas().tick());

        idle += painted == 0 ? 1 : 0;
        live += root.busy() ? 1 : 0;
    }

    root.release(toolkit::Pointer{.x = x, .y = CARD_Y});

    const double runs = static_cast<double>(state.iterations());

    state.counters["emptyFrames"] = static_cast<double>(idle) / runs;
    state.counters["busy"] = static_cast<double>(live) / runs;
    state.counters["pixels"] = static_cast<double>(painted);
}

BENCHMARK(Library_dragCard);

void Library_fullPaint(benchmark::State &state) {
    const auto profiles = static_cast<int>(state.range(0));

    bench::Rig &rig = shelf(profiles);

    std::size_t painted = 0;

    for ([[maybe_unused]] auto step : state) {
        painted = rig.canvas().full();
    }

    state.counters["pixels"] = static_cast<double>(painted);
}

BENCHMARK(Library_fullPaint)->Arg(8)->Arg(64)->Arg(512);

// A window drag-resize: every card is handed a new size on every frame.
void Library_resize(benchmark::State &state) {
    bench::Rig &rig = shelf(64);

    int width = 1280;
    int direction = -4;

    for ([[maybe_unused]] auto step : state) {
        width += direction;

        if (width < 900 || width > 1400) {
            direction = -direction;
        }

        rig.canvas().resize(width, 800);
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frameAt(rig.canvas().tick()));
    }

    rig.canvas().resize(1280, 800);
}

BENCHMARK(Library_resize);

// Switching shelves, which rebuilds the grid from a different list.
void Library_switchShelf(benchmark::State &state) {
    bench::Rig &rig = shelf(64);

    bool games = false;

    for ([[maybe_unused]] auto step : state) {
        games = !games;

        State::get().nav.shelf = games ? State::Shelf::Games : State::Shelf::Profiles;

        rig.config().library().pushShelf();
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());
    }
}

BENCHMARK(Library_switchShelf);

}
