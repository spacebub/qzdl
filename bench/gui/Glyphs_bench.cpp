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

#include <benchmark/benchmark.h>

#include "gui/draw/Glyphs.h"
#include "gui/draw/Mark.h"
#include "gui/draw/Paint.h"
#include "gui/draw/Svg.h"
#include "gui/draw/Theme.h"
#include "support/Canvas.h"

namespace {

constexpr const char *COG =
    "M12 8a4 4 0 100 8 4 4 0 000-8zM12 2v3M12 19v3M2 12h3M19 12h3"
    "M4.9 4.9l2.1 2.1M17 17l2.1 2.1M19.1 4.9L17 7M7 17l-2.1 2.1";

void Glyphs_draw(benchmark::State &state) {
    bench::Canvas canvas(128, 128);

    const auto which = static_cast<Glyphs::Glyph>(state.range(0));

    for ([[maybe_unused]] auto step : state) {
        Glyphs::draw(canvas.context(), which, BLPoint{32, 32}, 1.4F, Theme::of().text);
    }

    canvas.context().flush(BL_CONTEXT_FLUSH_SYNC);
}

BENCHMARK(Glyphs_draw)
    ->Arg(static_cast<int>(Glyphs::Glyph::Play))
    ->Arg(static_cast<int>(Glyphs::Glyph::Cog))
    ->Arg(static_cast<int>(Glyphs::Glyph::Search))
    ->Arg(static_cast<int>(Glyphs::Glyph::Check));

void Glyphs_drawTurned(benchmark::State &state) {
    bench::Canvas canvas(128, 128);

    for ([[maybe_unused]] auto step : state) {
        Glyphs::draw(canvas.context(), Glyphs::Glyph::Down, BLPoint{32, 32}, 1.4F,
                     Theme::of().muted, 90.0F);
    }

    canvas.context().flush(BL_CONTEXT_FLUSH_SYNC);
}

BENCHMARK(Glyphs_drawTurned);

void Glyphs_span(benchmark::State &state) {
    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Glyphs::span(1.4F));
    }
}

BENCHMARK(Glyphs_span);

void Svg_parse(benchmark::State &state) {
    for ([[maybe_unused]] auto step : state) {
        BLPath path;

        benchmark::DoNotOptimize(Svg::parse(COG, path));
        benchmark::DoNotOptimize(path);
    }
}

BENCHMARK(Svg_parse);

void Svg_glyph(benchmark::State &state) {
    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Svg::glyph(COG, 24.0F, 16.0F));
    }
}

BENCHMARK(Svg_glyph);

// Cached by size and tint. A card's shadow is asked for every frame it moves.
void Paint_shadowCached(benchmark::State &state) {
    benchmark::DoNotOptimize(&Paint::shadow(244, 232, Theme::radius, 24.0, Theme::of().shadow));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(&Paint::shadow(244, 232, Theme::radius, 24.0,
                                                Theme::of().shadow));
    }
}

BENCHMARK(Paint_shadowCached);

// A size the cache has not seen, which is every frame of a window being dragged
// wider: the sprite is filled, blurred over six passes and kept. The two blurs
// are the ones a card asks for, at rest and lifted.
void Paint_shadowBuilt(benchmark::State &state) {
    const auto blur = static_cast<double>(state.range(0));

    int wide = 200;

    for ([[maybe_unused]] auto step : state) {
        wide = wide > 400 ? 200 : wide + 1;

        benchmark::DoNotOptimize(&Paint::shadow(wide, 232, Theme::radius, blur,
                                                Theme::of().shadow));
    }
}

BENCHMARK(Paint_shadowBuilt)->Arg(4)->Arg(10);

void Paint_bleed(benchmark::State &state) {
    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Paint::bleed(24.0));
    }
}

BENCHMARK(Paint_bleed);

void Paint_down(benchmark::State &state) {
    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Paint::down(BLRect{0, 0, 244, 138}));
    }
}

BENCHMARK(Paint_down);

// Resampling is the expensive primitive. This is the art blit on a card.
void Paint_cover(benchmark::State &state) {
    bench::Canvas canvas(512, 400);

    const BLImage &art = Mark::of(256);

    for ([[maybe_unused]] auto step : state) {
        Paint::cover(canvas.context(), BLRect{0, 0, 244, 138}, art, Theme::radius);
    }

    canvas.context().flush(BL_CONTEXT_FLUSH_SYNC);
}

BENCHMARK(Paint_cover);

void Mark_of(benchmark::State &state) {
    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(&Mark::of(128));
    }
}

BENCHMARK(Mark_of);

// A whole-pixel 1:1 blit against a fractional one, which costs a resample.
void Paint_blitAligned(benchmark::State &state) {
    bench::Canvas canvas(512, 400);

    const BLImage &art = Mark::of(256);

    for ([[maybe_unused]] auto step : state) {
        canvas.context().blit_image(BLPoint{16, 16}, art);
    }

    canvas.context().flush(BL_CONTEXT_FLUSH_SYNC);
}

BENCHMARK(Paint_blitAligned);

void Paint_blitFractional(benchmark::State &state) {
    bench::Canvas canvas(512, 400);

    const BLImage &art = Mark::of(256);

    for ([[maybe_unused]] auto step : state) {
        canvas.context().blit_image(BLPoint{16.5, 16.5}, art);
    }

    canvas.context().flush(BL_CONTEXT_FLUSH_SYNC);
}

BENCHMARK(Paint_blitFractional);

}
