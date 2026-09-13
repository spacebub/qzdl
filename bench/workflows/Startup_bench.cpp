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

#include <utility>
#include <benchmark/benchmark.h>

#include "core/config/Config.h"
#include "core/config/Session.h"
#include "gui/state/State.h"
#include "support/Canvas.h"
#include "support/Corpus.h"
#include "support/Fixtures.h"
#include "support/Rig.h"

namespace {

// Reading the config off disk into the session, as the first thing ZDL does.
void Startup_sessionLoad(benchmark::State &state) {
    const auto profiles = static_cast<int>(state.range(0));
    const std::filesystem::path &path = bench::Corpus::json(profiles, 16);

    // Asserted, not assumed: a load that returns early measures nothing and reads
    // as a very fast one.
    for ([[maybe_unused]] auto step : state) {
        if (!Session::get().load(path)
            || std::cmp_not_equal(Session::get().config().profiles.size(), profiles)) {
            state.SkipWithError("the config did not load");

            break;
        }
    }

    state.SetItemsProcessed(state.iterations() * profiles);
}

BENCHMARK(Startup_sessionLoad)->Arg(8)->Arg(64)->Arg(512);

// The config in, every view's state out.
void Startup_pushEverything(benchmark::State &state) {
    const auto profiles = static_cast<int>(state.range(0));

    bench::Fixtures::install(bench::Fixtures::config(profiles, 16));

    bench::Rig &rig = bench::shared();

    rig.forget();

    for ([[maybe_unused]] auto step : state) {
        rig.config().reload();
    }

    state.SetItemsProcessed(state.iterations() * profiles);
}

BENCHMARK(Startup_pushEverything)->Arg(8)->Arg(64)->Arg(512);

// The whole library page built from nothing: every card, control and label.
void Startup_firstSync(benchmark::State &state) {
    const auto profiles = static_cast<int>(state.range(0));

    bench::Fixtures::install(bench::Fixtures::config(profiles, 16));

    bench::Rig &rig = bench::shared();

    rig.forget();
    rig.config().reload();
    rig.go(State::Page::Library);

    for ([[maybe_unused]] auto step : state) {
        state.PauseTiming();

        State::get().cfg.rev++;

        state.ResumeTiming();

        rig.sync();
    }

    state.SetItemsProcessed(state.iterations() * profiles);
}

BENCHMARK(Startup_firstSync)->Arg(8)->Arg(64)->Arg(512);

// Sync, lay out and paint the window whole: the first frame the desktop sees.
void Startup_firstFrame(benchmark::State &state) {
    const auto profiles = static_cast<int>(state.range(0));

    bench::Fixtures::install(bench::Fixtures::config(profiles, 16));

    bench::Rig &rig = bench::shared();

    rig.forget();
    rig.config().reload();
    rig.go(State::Page::Library);
    rig.sync();
    rig.ready();

    std::size_t painted = 0;

    for ([[maybe_unused]] auto step : state) {
        rig.sync();

        painted = rig.canvas().full();
    }

    state.counters["pixels"] = static_cast<double>(painted);
}

BENCHMARK(Startup_firstFrame)->Arg(8)->Arg(64);

// Every page visited once, which is what a first tour of the window costs.
void Startup_visitEveryPage(benchmark::State &state) {
    bench::Fixtures::install(bench::Fixtures::config(64, 16));

    bench::Rig &rig = bench::shared();

    rig.forget();
    rig.config().reload();
    rig.go(State::Page::Library);
    rig.sync();
    rig.ready();

    for ([[maybe_unused]] auto step : state) {
        for (const State::Page page : {State::Page::Library, State::Page::Profile,
                                       State::Page::Engines, State::Page::Settings}) {
            rig.go(page);
            rig.sync();

            benchmark::DoNotOptimize(rig.canvas().full());
        }
    }
}

BENCHMARK(Startup_visitEveryPage);

// The session as it comes off a real config file, bridges and all.
void Startup_wholeColdStart(benchmark::State &state) {
    const std::filesystem::path &path = bench::Corpus::json(64, 16);

    bench::Rig &rig = bench::shared();

    rig.forget();

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Session::get().load(path));

        rig.config().reload();
        rig.go(State::Page::Library);
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().full());
    }
}

BENCHMARK(Startup_wholeColdStart);

}
