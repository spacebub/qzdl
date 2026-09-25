/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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

#include <benchmark/benchmark.h>

#include "core/util/Ansi.h"

#include "support/Fixtures.h"

namespace {

constexpr int LINES = 64;

// What a port prints between two escapes.
std::string plain() {
    std::string out;

    for (int at = 0; at < LINES; at++) {
        out += bench::Fixtures::words(8 + (at % 5), static_cast<unsigned>(at) + 1U);
        out += "\r\n";
    }

    return out;
}

// The same lines the way uzdoom writes them to a terminal: coloured, and each one
// wrapped in a progress bar redraw at the bottom row.
std::string decorated() {
    const std::string clean = "\0337\033[24;0H\033[0J\0338";
    const std::string bar = "\0337\033[24;0H\033[2K[" + std::string(30, '=') + std::string(48, '.')
                            + "\033[24;80H]\0338";
    std::string out;

    for (int at = 0; at < LINES; at++) {
        out += clean;
        out += "\033[38;2;223;223;223m";
        out += bench::Fixtures::words(8 + (at % 5), static_cast<unsigned>(at) + 1U);
        out += "\033[0m\r\n";
        out += bar;
    }

    return out;
}

void run(benchmark::State &state, const std::string &input) {
    std::string partial;

    for ([[maybe_unused]] auto step : state) {
        Ansi filter;

        partial.clear();
        filter.filter(input, partial);
        benchmark::DoNotOptimize(partial);
    }

    state.SetBytesProcessed(static_cast<int64_t>(input.size()) * state.iterations());
}

// The append the filter replaced.
void Ansi_append(benchmark::State &state) {
    const std::string input = plain();
    std::string partial;

    for ([[maybe_unused]] auto step : state) {
        partial.clear();
        partial += input;
        benchmark::DoNotOptimize(partial);
    }

    state.SetBytesProcessed(static_cast<int64_t>(input.size()) * state.iterations());
}

BENCHMARK(Ansi_append);

void Ansi_plain(benchmark::State &state) {
    run(state, plain());
}

BENCHMARK(Ansi_plain);

void Ansi_decorated(benchmark::State &state) {
    run(state, decorated());
}

BENCHMARK(Ansi_decorated);

}
