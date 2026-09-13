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

bench::Rig &settings() {
    bench::Rig &rig = bench::shared();

    rig.forget();

    bench::Fixtures::install(bench::Fixtures::config(64, 16));

    rig.config().reload();

    rig.go(State::Page::Settings);
    rig.sync();
    rig.ready();

    return rig;
}

void Settings_fullPaint(benchmark::State &state) {
    bench::Rig &rig = settings();

    std::size_t painted = 0;

    for ([[maybe_unused]] auto step : state) {
        painted = rig.canvas().full();
    }

    state.counters["pixels"] = static_cast<double>(painted);
}

BENCHMARK(Settings_fullPaint);

// A wheel notch and the frames it animates over.
void Settings_scroll(benchmark::State &state) {
    bench::Rig &rig = settings();

    toolkit::Root &root = rig.ui();

    std::size_t painted = 0;
    double steps = -1.0;

    for ([[maybe_unused]] auto step : state) {
        root.wheel(steps, 600.0, 400.0);

        painted = 0;

        for (int frame = 0; frame < 8; ++frame) {
            painted += rig.canvas().frameAt(rig.canvas().tick());
        }

        steps = -steps;
    }

    state.counters["pixels"] = static_cast<double>(painted) / 8.0;
}

BENCHMARK(Settings_scroll);

void Settings_sync(benchmark::State &state) {
    bench::Rig &rig = settings();

    for ([[maybe_unused]] auto step : state) {
        rig.sync();
    }
}

BENCHMARK(Settings_sync);

}
