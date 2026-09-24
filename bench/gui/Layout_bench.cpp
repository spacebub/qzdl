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

#include <memory>
#include <string>

#include <benchmark/benchmark.h>

#include "ttk/draw/Theme.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/layout/Box.h"
#include "ttk/toolkit/layout/Pair.h"
#include "ttk/toolkit/layout/Panel.h"
#include "ttk/toolkit/layout/Scroll.h"
#include "ttk/toolkit/layout/Spacer.h"
#include "ttk/toolkit/layout/Wrap.h"

#include "support/Canvas.h"
#include "support/Fixtures.h"

using namespace ttk;

namespace {

bench::Canvas &sheet() {
    static bench::Canvas made(1280, 800);

    return made;
}

std::unique_ptr<ttk::Label> row(const int at) {
    auto made = std::make_unique<ttk::Label>(
        bench::Fixtures::words(4, static_cast<unsigned>(at) + 1U));

    made->fixedHeight = 24.0;

    return made;
}

void fill(ttk::Widget *into, const int count) {
    for (int at = 0; at < count; ++at) {
        into->add(row(at));
    }
}

std::unique_ptr<ttk::Box> column(const int count) {
    std::unique_ptr<ttk::Box> made = ttk::Box::column();

    made->spacing(Theme::gap)->pad(Theme::pad);

    fill(made.get(), count);

    return made;
}

void Box_arrangeColumn(benchmark::State &state) {
    const auto count = static_cast<int>(state.range(0));

    ttk::Box *made = bench::mount(sheet(), column(count), 600.0, 4000.0);

    for ([[maybe_unused]] auto step : state) {
        made->arrange(sheet().type());
    }

    state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(Box_arrangeColumn)->Arg(8)->Arg(64)->Arg(512);

void Box_arrangeRow(benchmark::State &state) {
    const auto count = static_cast<int>(state.range(0));

    std::unique_ptr<ttk::Box> held = ttk::Box::row();

    held->spacing(Theme::gap);

    fill(held.get(), count);

    ttk::Box *made = bench::mount(sheet(), std::move(held), 1200.0, 42.0);

    for ([[maybe_unused]] auto step : state) {
        made->arrange(sheet().type());
    }

    state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(Box_arrangeRow)->Arg(8)->Arg(64);

void Box_naturalHeight(benchmark::State &state) {
    const auto count = static_cast<int>(state.range(0));

    ttk::Box *made = bench::mount(sheet(), column(count), 600.0, 4000.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(made->natural_height(sheet().type(), 600.0));
    }

    state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(Box_naturalHeight)->Arg(8)->Arg(64)->Arg(512);

// A row that shares its spare width: the share() pass runs over every child.
void Box_stretch(benchmark::State &state) {
    const auto count = static_cast<int>(state.range(0));

    std::unique_ptr<ttk::Box> held = ttk::Box::row();

    held->spacing(Theme::gap);

    for (int at = 0; at < count; ++at) {
        ttk::Label *child = held->append(row(at));

        child->stretch = 1.0;
    }

    ttk::Box *made = bench::mount(sheet(), std::move(held), 1200.0, 42.0);

    for ([[maybe_unused]] auto step : state) {
        made->arrange(sheet().type());
    }

    state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(Box_stretch)->Arg(8)->Arg(64);

void Box_nested(benchmark::State &state) {
    const auto depth = static_cast<int>(state.range(0));

    std::unique_ptr<ttk::Box> held = ttk::Box::column();

    ttk::Box *deepest = held.get();

    for (int at = 0; at < depth; ++at) {
        auto inner = ttk::Box::column();

        inner->spacing(4.0)->pad(4.0);

        fill(inner.get(), 4);

        deepest = deepest->append(std::move(inner));
    }

    ttk::Box *made = bench::mount(sheet(), std::move(held), 600.0, 4000.0);

    for ([[maybe_unused]] auto step : state) {
        made->arrange(sheet().type());
    }
}

BENCHMARK(Box_nested)->Arg(4)->Arg(16);

void Wrap_arrange(benchmark::State &state) {
    const auto count = static_cast<int>(state.range(0));

    auto held = std::make_unique<ttk::Wrap>();

    held->spacing(8.0, 8.0);

    fill(held.get(), count);

    ttk::Wrap *made = bench::mount(sheet(), std::move(held), 900.0, 600.0);

    for ([[maybe_unused]] auto step : state) {
        made->arrange(sheet().type());
    }

    state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(Wrap_arrange)->Arg(16)->Arg(128);

void Wrap_naturalHeight(benchmark::State &state) {
    const auto count = static_cast<int>(state.range(0));

    auto held = std::make_unique<ttk::Wrap>();

    held->spacing(8.0, 8.0);

    fill(held.get(), count);

    ttk::Wrap *made = bench::mount(sheet(), std::move(held), 900.0, 600.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(made->natural_height(sheet().type(), 900.0));
    }
}

BENCHMARK(Wrap_naturalHeight)->Arg(16)->Arg(128);

void Pair_arrange(benchmark::State &state) {
    auto held = std::make_unique<ttk::Pair>(420.0);

    held->spacing(Theme::gap);

    fill(held.get(), 2);

    ttk::Pair *made = bench::mount(sheet(), std::move(held), 900.0, 120.0);

    for ([[maybe_unused]] auto step : state) {
        made->arrange(sheet().type());
    }
}

BENCHMARK(Pair_arrange);

void Panel_paint(benchmark::State &state) {
    auto held = std::make_unique<ttk::Panel>();

    held->rounding = Theme::radius;
    held->bordered = true;

    ttk::Panel *made = bench::mount(sheet(), std::move(held), 560.0, 180.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Panel_paint);

// A scroller's arrange places the whole content, however tall it is.
void Scroll_arrange(benchmark::State &state) {
    const auto count = static_cast<int>(state.range(0));

    auto held = std::make_unique<ttk::Scroll>();

    held->hold(column(count));

    ttk::Scroll *made = bench::mount(sheet(), std::move(held), 900.0, 600.0);

    for ([[maybe_unused]] auto step : state) {
        made->arrange(sheet().type());
    }

    state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(Scroll_arrange)->Arg(64)->Arg(512);

void Scroll_wheel(benchmark::State &state) {
    auto held = std::make_unique<ttk::Scroll>();

    held->hold(column(512));

    ttk::Scroll *made = bench::mount(sheet(), std::move(held), 900.0, 600.0);

    const ttk::Pointer at{.x = 400.0, .y = 300.0};
    double steps = -1.0;

    for ([[maybe_unused]] auto step : state) {
        steps = made->offset() > made->reach() - 10.0 ? 1.0 : steps;
        steps = made->offset() < 10.0 ? -1.0 : steps;

        benchmark::DoNotOptimize(made->wheel(steps, at));
    }
}

BENCHMARK(Scroll_wheel);

// Painting a long list through a viewport: the rows out of view are skipped.
void Scroll_paint(benchmark::State &state) {
    const auto count = static_cast<int>(state.range(0));

    auto held = std::make_unique<ttk::Scroll>();

    held->hold(column(count));

    ttk::Scroll *made = bench::mount(sheet(), std::move(held), 900.0, 600.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Scroll_paint)->Arg(64)->Arg(512);

}
