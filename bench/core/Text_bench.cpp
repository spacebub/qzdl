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

#include <algorithm>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>

#include "core/util/Text.h"
#include "support/Fixtures.h"

namespace {

const std::string &sentence() {
    static const std::string made = bench::Fixtures::words(24, 7);

    return made;
}

std::vector<std::string> mapNames(const int count) {
    std::vector<std::string> names;

    names.reserve(static_cast<size_t>(count));

    for (int at = count; at > 0; --at) {
        names.push_back("MAP" + std::to_string(at));
    }

    return names;
}

void Text_lower(benchmark::State &state) {
    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Text::lower(sentence()));
    }
}

BENCHMARK(Text_lower);

void Text_upper(benchmark::State &state) {
    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Text::upper(sentence()));
    }
}

BENCHMARK(Text_upper);

void Text_trim(benchmark::State &state) {
    const std::string padded = "   \t" + sentence() + " \r\n";

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Text::trim(padded));
    }
}

BENCHMARK(Text_trim);

void Text_iequals(benchmark::State &state) {
    const std::string left = "GZDoom (Vulkan)";
    const std::string right = "gzdoom (vulkan)";

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Text::iequals(left, right));
    }
}

BENCHMARK(Text_iequals);

void Text_iendsWith(benchmark::State &state) {
    const std::string file = "/games/doom2/master/levels/MASTERLEV.WAD";

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Text::iendsWith(file, ".wad"));
    }
}

BENCHMARK(Text_iendsWith);

void Text_toInt(benchmark::State &state) {
    const std::string value = "32767";

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Text::toInt(value));
    }
}

BENCHMARK(Text_toInt);

void Text_split(benchmark::State &state) {
    const std::string list = "a.wad;b.wad;c.wad;d.pk3;e.wad;f.deh;g.bex;h.wad";

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Text::split(list, ';'));
    }
}

BENCHMARK(Text_split);

void Text_join(benchmark::State &state) {
    const std::vector<std::string> parts = Text::split(
        "a.wad;b.wad;c.wad;d.pk3;e.wad;f.deh;g.bex;h.wad", ';');

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Text::join(parts, ";"));
    }
}

BENCHMARK(Text_join);

// The map list is sorted with this on every warp push.
void Text_naturalSort(benchmark::State &state) {
    const std::vector<std::string> source = mapNames(static_cast<int>(state.range(0)));

    for ([[maybe_unused]] auto step : state) {
        std::vector<std::string> names = source;

        std::ranges::sort(names, Text::naturalLess);

        benchmark::DoNotOptimize(names);
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}

BENCHMARK(Text_naturalSort)->Arg(32)->Arg(256)->Arg(1024);

void Text_naturalLess(benchmark::State &state) {
    std::string left = "MAP2";
    std::string right = "MAP10";

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(left);
        benchmark::DoNotOptimize(right);
        benchmark::DoNotOptimize(Text::naturalLess(left, right));
    }
}

BENCHMARK(Text_naturalLess);

void Text_parseArguments(benchmark::State &state) {
    const std::string line =
        R"(-iwad "/games/DOOM2.WAD" -file "/addons/Valiant.wad" "/addons/pl2.wad" -skill 4 -warp 07 -complevel 9)";

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Text::parseArguments(line));
    }
}

BENCHMARK(Text_parseArguments);

void Text_quoteArgument(benchmark::State &state) {
    const std::string path = R"(C:\Program Files\GZDoom\gzdoom.exe)";

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Text::quoteArgument(path));
    }
}

BENCHMARK(Text_quoteArgument);

}
