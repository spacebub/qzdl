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
#include <memory>

#include <benchmark/benchmark.h>

#include "ttk/dialogs/ConfirmDialog.h"

#include "gui/dialogs/AboutDialog.h"
#include "gui/state/State.h"
#include "support/Canvas.h"
#include "support/Fixtures.h"
#include "support/Rig.h"

namespace {

// A frame is 1000 ms over the display's refresh rate, about 4 ms at 240 Hz. Every
// number here is one frame's share of that.
bench::Rig &window(const State::Page page) {
    bench::Rig &rig = bench::shared();

    rig.forget();

    bench::Fixtures::install(bench::Fixtures::config(64, 32));

    rig.config().reload();

    rig.go(page);
    rig.sync();
    rig.ready();

    return rig;
}

// Nothing dirty and nothing in flight: what an idle window costs per turn.
void Frame_idle(benchmark::State &state) {
    bench::Rig &rig = window(State::Page::Library);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(rig.canvas().frameAt(rig.canvas().tick()));
    }
}

BENCHMARK(Frame_idle);

// A caret blinking on a focused field. `busy` is the share of turns the loop would
// have had to stay awake for: a blinker that sleeps between beats answers near zero,
// one that holds the live list answers one.
void Frame_caretIdle(benchmark::State &state) {
    bench::Rig &rig = window(State::Page::Profile);

    rig.ui().focus_next(false);

    if (rig.ui().focused() == nullptr) {
        state.SkipWithError("nothing took the keyboard, so nothing is blinking");

        return;
    }

    double busy = 0.0;

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(rig.canvas().frameAt(rig.canvas().tick()));

        busy += rig.ui().busy() ? 1.0 : 0.0;
    }

    state.counters["busy"] = busy / static_cast<double>(state.iterations());
}

BENCHMARK(Frame_caretIdle);

// The whole window repainted, which is what a resize or a theme change costs.
void Frame_fullRepaint(benchmark::State &state) {
    bench::Rig &rig = window(static_cast<State::Page>(state.range(0)));

    std::size_t painted = 0;

    for ([[maybe_unused]] auto step : state) {
        painted = rig.canvas().full();
    }

    state.counters["pixels"] = static_cast<double>(painted);
}

BENCHMARK(Frame_fullRepaint)
    ->Arg(static_cast<int>(State::Page::Library))
    ->Arg(static_cast<int>(State::Page::Profile))
    ->Arg(static_cast<int>(State::Page::Engines))
    ->Arg(static_cast<int>(State::Page::Settings));

// One control's worth of damage, which is the common case.
void Frame_smallDamage(benchmark::State &state) {
    bench::Rig &rig = window(State::Page::Library);

    ttk::Root &root = rig.ui();

    for ([[maybe_unused]] auto step : state) {
        root.damage(BLRect{300, 400, 140, 42});

        benchmark::DoNotOptimize(rig.canvas().frameAt(rig.canvas().tick()));
    }
}

BENCHMARK(Frame_smallDamage);

// The pointer reports many times a frame. Each one re-picks the tree.
void Frame_pointerMotion(benchmark::State &state) {
    bench::Rig &rig = window(State::Page::Library);

    ttk::Root &root = rig.ui();
    double x = 0.0;

    for ([[maybe_unused]] auto step : state) {
        x = x > 1200.0 ? 0.0 : x + 7.0;

        root.motion(x, 400.0);
    }
}

BENCHMARK(Frame_pointerMotion);

void Frame_cursor(benchmark::State &state) {
    bench::Rig &rig = window(State::Page::Library);

    ttk::Root &root = rig.ui();

    root.motion(300.0, 400.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(root.cursor());
    }
}

BENCHMARK(Frame_cursor);

// The animation step over the live list, with nothing to paint after it.
void Frame_advance(benchmark::State &state) {
    bench::Rig &rig = window(State::Page::Library);

    ttk::Root &root = rig.ui();
    double now = 0.0;

    for ([[maybe_unused]] auto step : state) {
        now += 1.0 / 280.0;

        root.advance(now);

        benchmark::DoNotOptimize(root.busy());
    }
}

BENCHMARK(Frame_advance);

// A relayout of the whole tree, which a size or fold change forces.
void Frame_relayout(benchmark::State &state) {
    bench::Rig &rig = window(State::Page::Profile);

    ttk::Root &root = rig.ui();

    for ([[maybe_unused]] auto step : state) {
        root.relayout();

        benchmark::DoNotOptimize(root.settle());

        root.take();
    }
}

BENCHMARK(Frame_relayout);

// Taking the damage list, which is merged and clipped on the way out.
void Frame_takeDamage(benchmark::State &state) {
    bench::Rig &rig = window(State::Page::Library);

    ttk::Root &root = rig.ui();

    const auto count = static_cast<int>(state.range(0));

    for ([[maybe_unused]] auto step : state) {
        for (int at = 0; at < count; ++at) {
            root.damage(BLRect{static_cast<double>(at * 30), 100, 120, 40});
        }

        benchmark::DoNotOptimize(root.take());
    }
}

BENCHMARK(Frame_takeDamage)->Arg(1)->Arg(2)->Arg(4)->Arg(8)->Arg(32);

// A page switched every frame, which is the worst a shortcut can do.
void Frame_pageSwitch(benchmark::State &state) {
    bench::Rig &rig = window(State::Page::Library);

    bool library = true;

    for ([[maybe_unused]] auto step : state) {
        library = !library;

        rig.go(library ? State::Page::Library : State::Page::Profile);
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frameAt(rig.canvas().tick()));
    }
}

BENCHMARK(Frame_pageSwitch);

// A dialog going up over the page and coming down again.
void Frame_dialogOpenClose(benchmark::State &state) {
    bench::Rig &rig = window(State::Page::Library);

    for ([[maybe_unused]] auto step : state) {
        rig.dialogs().show(std::make_unique<ttk::ConfirmDialog>(
            "Remove profile", "This cannot be undone.", "Remove", true, [] {}));

        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frameAt(rig.canvas().tick()));

        rig.dialogs().dismiss();
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frameAt(rig.canvas().tick()));
    }
}

BENCHMARK(Frame_dialogOpenClose);

// The about dialog, which is the heaviest one: a mark, a paragraph and paths.
void Frame_aboutDialog(benchmark::State &state) {
    bench::Rig &rig = window(State::Page::Library);

    for ([[maybe_unused]] auto step : state) {
        rig.dialogs().show(std::make_unique<dialogs::AboutDialog>());
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frameAt(rig.canvas().tick()));

        rig.dialogs().dismiss();
        rig.sync();
    }
}

BENCHMARK(Frame_aboutDialog);

// Toasts stacking and expiring over the page.
void Frame_toasts(benchmark::State &state) {
    bench::Rig &rig = window(State::Page::Library);

    int at = 0;

    for ([[maybe_unused]] auto step : state) {
        rig.notify().info(bench::Fixtures::words(6, static_cast<unsigned>(at++)), "Notice");
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frameAt(rig.canvas().tick()));
    }
}

BENCHMARK(Frame_toasts);

}
