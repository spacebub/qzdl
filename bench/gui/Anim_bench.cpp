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

#include "ttk/draw/Anim.h"

using namespace ttk;

namespace {

void Anim_shape(benchmark::State &state) {
    const auto curve = static_cast<Anim::Curve>(state.range(0));
    float at = 0.0F;

    for ([[maybe_unused]] auto step : state) {
        at += 0.01F;

        if (at > 1.0F) {
            at = 0.0F;
        }

        benchmark::DoNotOptimize(Anim::shape(curve, at));
    }
}

BENCHMARK(Anim_shape)
    ->Arg(static_cast<int>(Anim::Curve::Linear))
    ->Arg(static_cast<int>(Anim::Curve::CubicOut))
    ->Arg(static_cast<int>(Anim::Curve::BackOut));

void Anim_run(benchmark::State &state) {
    Anim::Tween tween;
    double now = 0.0;

    for ([[maybe_unused]] auto step : state) {
        now += 1.0 / 280.0;

        tween.run(1.0F, now, 0.18, Anim::Curve::CubicOut);

        benchmark::DoNotOptimize(tween);
    }
}

BENCHMARK(Anim_run);

void Anim_toward(benchmark::State &state) {
    Anim::Tween tween;
    double now = 0.0;
    float goal = 1.0F;

    for ([[maybe_unused]] auto step : state) {
        now += 1.0 / 280.0;
        goal = goal > 0.5F ? 0.0F : 1.0F;

        tween.toward(goal, now, 0.18, Anim::Curve::CubicOut);

        benchmark::DoNotOptimize(tween);
    }
}

BENCHMARK(Anim_toward);

// One tween stepped at the refresh rate, which every live widget does per frame.
void Anim_advance(benchmark::State &state) {
    Anim::Tween tween;
    double now = 0.0;

    tween.run(1.0F, now, 1e9, Anim::Curve::CubicOut);

    for ([[maybe_unused]] auto step : state) {
        now += 1.0 / 280.0;

        tween.advance(now);

        benchmark::DoNotOptimize(tween.value());
    }
}

BENCHMARK(Anim_advance);

void Anim_advanceStill(benchmark::State &state) {
    Anim::Tween tween;
    double now = 0.0;

    tween.set(1.0F);

    for ([[maybe_unused]] auto step : state) {
        now += 1.0 / 280.0;

        tween.advance(now);

        benchmark::DoNotOptimize(tween.live());
    }
}

BENCHMARK(Anim_advanceStill);

}
