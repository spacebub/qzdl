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

// Installed unconditionally. See the note in Library_bench.cpp. The fixture
// carries twelve ports, which is four rows of the installed shelf.
bench::Rig &installed() {
    bench::Rig &rig = bench::shared();

    rig.forget();

    bench::Fixtures::install(bench::Fixtures::config(64, 16));

    rig.config().reload();

    State::get().nav.engines = State::EnginesTab::Installed;

    rig.go(State::Page::Engines);
    rig.sync();
    rig.ready();

    return rig;
}

// The middle of the first card, which sits at 66,146 and is 368 by 152: above
// its button row and left of its pills, so a press there takes hold of it.
constexpr double CARD_X = 250.0;
constexpr double CARD_Y = 222.0;

// A port carried across the shelf, the same drive as Library_dragCard: the one in
// hand moves with the pointer and the neighbours it passes walk to the gaps it
// leaves. The engine cards are panels rather than sprites, so they damage
// differently from the library's.
void Engines_dragCard(benchmark::State &state) {
    bench::Rig &rig = installed();

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

    // The drop lands and the neighbours reach their gaps, or the next call finds
    // the shelf frozen mid-walk with nothing under the pointer: the fixture puts
    // the ports back, which leaves the cards as they were.
    for (int frame = 0; frame < 60 && root.busy(); ++frame) {
        rig.canvas().frameAt(rig.canvas().tick());
    }

    const double runs = static_cast<double>(state.iterations());

    state.counters["emptyFrames"] = static_cast<double>(idle) / runs;
    state.counters["busy"] = static_cast<double>(live) / runs;
    state.counters["pixels"] = static_cast<double>(painted);
}

BENCHMARK(Engines_dragCard);

// Where a carried first card is let go: in the cell two to the right, short of
// its middle, so the drop leaves it a walk of 120 px.
constexpr double DROP_X = CARD_X + 648.0;

// Off the clock, a frame stands for a 60 Hz one: the tweens run on time, not on
// frames, so the same walk finishes in a fifth of the frames.
constexpr double UNTIMED = 60.0;

// Carries the first card to DROP_X and gives the neighbours time to reach their
// gaps, which is what a release finds. The drag is measured above.
void carry(bench::Rig &rig) {
    toolkit::Root &root = rig.ui();

    root.motion(CARD_X, CARD_Y);
    root.press(toolkit::Pointer{.x = CARD_X, .y = CARD_Y});

    for (double x = CARD_X; x < DROP_X; x += 40.0) {
        root.motion(x, CARD_Y);
        rig.canvas().frameAt(rig.canvas().tick(UNTIMED));
    }

    root.motion(DROP_X, CARD_Y);

    for (int frame = 0; frame < 15; ++frame) {
        rig.canvas().frameAt(rig.canvas().tick(UNTIMED));
    }
}

// Frames until nothing is in flight: the walk to the new cells, and the card
// left under the pointer lighting. Answers the pixels painted.
std::size_t rest(bench::Rig &rig, std::size_t &frames, const double hz) {
    std::size_t painted = 0;

    for (int frame = 0; frame < 120 && rig.ui().busy(); ++frame) {
        painted += rig.canvas().frameAt(rig.canvas().tick(hz));
        ++frames;
    }

    return painted;
}

// One drop, end to end. False when the shelf came out in the order it went in.
bool drop(bench::Rig &rig) {
    const std::string first = State::get().cfg.ports.front().name;
    std::size_t frames = 0;

    carry(rig);

    rig.ui().release(toolkit::Pointer{.x = DROP_X, .y = CARD_Y});
    rig.sync();
    rig.canvas().frameAt(rig.canvas().tick());

    rest(rig, frames, UNTIMED);

    return State::get().cfg.ports.front().name != first;
}

// The release: the list changes, the shelf is rebuilt in the new order, laid out
// and repainted whole, and every card that moved starts its walk. One frame, the
// dear one of a reorder.
void Engines_dropCard(benchmark::State &state) {
    bench::Rig &rig = installed();

    if (!drop(rig)) {
        state.SkipWithError("the drop did not reorder the shelf");

        return;
    }

    toolkit::Root &root = rig.ui();
    std::size_t painted = 0;
    std::size_t frames = 0;

    for ([[maybe_unused]] auto step : state) {
        state.PauseTiming();
        carry(rig);
        state.ResumeTiming();

        root.release(toolkit::Pointer{.x = DROP_X, .y = CARD_Y});
        rig.sync();

        painted = rig.canvas().frameAt(rig.canvas().tick());

        state.PauseTiming();
        rest(rig, frames, UNTIMED);
        state.ResumeTiming();
    }

    state.counters["pixels"] = static_cast<double>(painted);
}

BENCHMARK(Engines_dropCard);

// The frames after the drop, while the dropped card and the neighbours it passed
// walk to their cells. An iteration is the whole walk and `frames` is how many it
// took, so the time over `frames` is one frame's share of the budget.
void Engines_settleWalk(benchmark::State &state) {
    bench::Rig &rig = installed();

    if (!drop(rig)) {
        state.SkipWithError("the drop did not reorder the shelf");

        return;
    }

    toolkit::Root &root = rig.ui();
    std::size_t painted = 0;
    std::size_t frames = 0;

    for ([[maybe_unused]] auto step : state) {
        state.PauseTiming();
        carry(rig);
        root.release(toolkit::Pointer{.x = DROP_X, .y = CARD_Y});
        rig.sync();
        rig.canvas().frameAt(rig.canvas().tick());
        state.ResumeTiming();

        painted += rest(rig, frames, 280.0);
    }

    state.counters["frames"] = static_cast<double>(frames) / static_cast<double>(state.iterations());
    state.counters["pixels"] = frames == 0 ? 0.0 : static_cast<double>(painted) / static_cast<double>(frames);
}

BENCHMARK(Engines_settleWalk);

}
