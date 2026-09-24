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

#include <string>
#include <vector>

#include <benchmark/benchmark.h>

#include "ttk/draw/Theme.h"
#include "ttk/draw/Typeface.h"

#include "support/Canvas.h"
#include "support/Fixtures.h"

using namespace ttk;

namespace {

const std::vector<std::string> &labels() {
    static const std::vector<std::string> made = bench::Fixtures::lines(512);

    return made;
}

// Warm: the run has been shaped and its mask rasterised already.
void Typeface_widthCached(benchmark::State &state) {
    Typeface &type = bench::fonts();
    const BLFont &font = type.at(Typeface::regular, Theme::fontBody);
    const std::string run = "Knee Deep in the Dead";

    benchmark::DoNotOptimize(type.width(font, run));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(type.width(font, run));
    }
}

BENCHMARK(Typeface_widthCached);

// A 512-run working set, as a full page of rows has. The cache holds them all.
void Typeface_widthManyRuns(benchmark::State &state) {
    Typeface &type = bench::fonts();
    const BLFont &font = type.at(Typeface::regular, Theme::fontBody);
    const std::vector<std::string> &runs = labels();
    size_t at = 0;

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(type.width(font, runs[at++ % runs.size()]));
    }
}

BENCHMARK(Typeface_widthManyRuns);

void Typeface_elide(benchmark::State &state) {
    Typeface &type = bench::fonts();
    const BLFont &font = type.at(Typeface::regular, Theme::fontBody);
    const std::string run = "/games/doom/addons/Eviternity II RC1.wad";

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(type.elide(font, run, 180.0F));
    }
}

BENCHMARK(Typeface_elide);

void Typeface_at(benchmark::State &state) {
    Typeface &type = bench::fonts();

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(&type.at(Typeface::semibold, Theme::fontLarge));
    }
}

BENCHMARK(Typeface_at);

void Typeface_lineHeight(benchmark::State &state) {
    Typeface &type = bench::fonts();
    const BLFont &font = type.at(Typeface::regular, Theme::fontBody);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(type.line_height(font));
    }
}

BENCHMARK(Typeface_lineHeight);

// The cached A8 mask laid down. This is what a resting label costs per frame.
void Typeface_drawCached(benchmark::State &state) {
    bench::Canvas canvas(400, 60);

    Typeface &type = canvas.type();
    const BLFont &font = type.at(Typeface::regular, Theme::fontBody);
    const std::string run = "Knee Deep in the Dead";

    type.draw(canvas.context(), font, BLPoint{8.0, 8.0}, run, Theme::palette().text);

    for ([[maybe_unused]] auto step : state) {
        type.draw(canvas.context(), font, BLPoint{8.0, 8.0}, run, Theme::palette().text);
    }

    canvas.context().flush(BL_CONTEXT_FLUSH_SYNC);
}

BENCHMARK(Typeface_drawCached);

void Typeface_drawManyRuns(benchmark::State &state) {
    bench::Canvas canvas(400, 60);

    Typeface &type = canvas.type();
    const BLFont &font = type.at(Typeface::regular, Theme::fontBody);
    const std::vector<std::string> &runs = labels();
    size_t at = 0;

    for ([[maybe_unused]] auto step : state) {
        type.draw(canvas.context(), font, BLPoint{8.0, 8.0}, runs[at++ % runs.size()],
                  Theme::palette().text);
    }

    canvas.context().flush(BL_CONTEXT_FLUSH_SYNC);
}

BENCHMARK(Typeface_drawManyRuns);

void Typeface_drawTracked(benchmark::State &state) {
    bench::Canvas canvas(400, 60);

    Typeface &type = canvas.type();
    const BLFont &font = type.at(Typeface::semibold, Theme::fontTiny);
    const std::string run = "LIBRARY";

    for ([[maybe_unused]] auto step : state) {
        type.draw_tracked(canvas.context(), font, BLPoint{8.0, 8.0}, run, Theme::palette().muted, 1.2F);
    }

    canvas.context().flush(BL_CONTEXT_FLUSH_SYNC);
}

BENCHMARK(Typeface_drawTracked);

void Typeface_widthTracked(benchmark::State &state) {
    Typeface &type = bench::fonts();
    const BLFont &font = type.at(Typeface::semibold, Theme::fontTiny);
    const std::string run = "LIBRARY";

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(type.width_tracked(font, run, 1.2F));
    }
}

BENCHMARK(Typeface_widthTracked);

// A page of labels, which is the shape of a real frame's text cost.
void Typeface_pageOfLabels(benchmark::State &state) {
    bench::Canvas canvas(1280, 800);

    Typeface &type = canvas.type();
    const BLFont &font = type.at(Typeface::regular, Theme::fontBody);
    const auto count = static_cast<int>(state.range(0));
    const std::vector<std::string> &runs = labels();

    for (int at = 0; at < count; ++at) {
        type.draw(canvas.context(), font, BLPoint{8.0, 8.0}, runs[static_cast<size_t>(at)],
                  Theme::palette().text);
    }

    for ([[maybe_unused]] auto step : state) {
        for (int at = 0; at < count; ++at) {
            type.draw(canvas.context(), font,
                      BLPoint{8.0, static_cast<double>((at % 40) * 20)},
                      runs[static_cast<size_t>(at)], Theme::palette().text);
        }

        canvas.context().flush(BL_CONTEXT_FLUSH_SYNC);
    }

    state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(Typeface_pageOfLabels)->Arg(20)->Arg(80)->Arg(200);

}
