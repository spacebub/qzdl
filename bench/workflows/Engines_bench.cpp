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

}
