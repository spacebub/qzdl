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

#include <filesystem>
#include <string>

#include <benchmark/benchmark.h>

#include "core/config/Config.h"
#include "core/launch/Arguments.h"
#include "core/launch/Command.h"
#include "core/launch/Dialect.h"
#include "core/launch/Storage.h"
#include "support/Fixtures.h"

namespace {

const Config &loaded(const int files) {
    static Config held;
    static int last = -1;

    if (files != last) {
        held = bench::Fixtures::config(8, files);
        held.ports.front().file = "/ports/gzdoom";
        held.activeProfile().port = held.ports.front().name;
        last = files;
    }

    return held;
}

void Dialect_ofPath(benchmark::State &state) {
    const std::filesystem::path program = "/usr/games/gzdoom";

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Dialect::of(program));
    }
}

BENCHMARK(Dialect_ofPath);

void Dialect_ofConfig(benchmark::State &state) {
    const Config &config = loaded(16);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Dialect::of(config));
    }
}

BENCHMARK(Dialect_ofConfig);

void Dialect_support(benchmark::State &state) {
    const Dialect::Port speaks = Dialect::of(std::filesystem::path("/usr/games/dsda-doom"));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Dialect::demos(speaks));
        benchmark::DoNotOptimize(Dialect::saves(speaks));
        benchmark::DoNotOptimize(Dialect::net(speaks));
    }
}

BENCHMARK(Dialect_support);

void Dialect_complevels(benchmark::State &state) {
    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Dialect::complevels(Dialect::Complevels::dsda));
    }
}

BENCHMARK(Dialect_complevels);

// The command preview is rebuilt on every keystroke in the profile page.
void Arguments_of(benchmark::State &state) {
    const Config &config = loaded(static_cast<int>(state.range(0)));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Arguments::of(config));
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}

BENCHMARK(Arguments_of)->Arg(0)->Arg(16)->Arg(128)->Arg(512);

void Arguments_maps(benchmark::State &state) {
    const Config &config = loaded(16);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Arguments::maps(config));
    }
}

BENCHMARK(Arguments_maps);

void Command_line(benchmark::State &state) {
    const Config &config = loaded(static_cast<int>(state.range(0)));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Command::line(config));
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}

BENCHMARK(Command_line)->Arg(0)->Arg(16)->Arg(128)->Arg(512);

void Command_pattern(benchmark::State &state) {
    const Config &config = loaded(16);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Command::pattern(config));
    }
}

BENCHMARK(Command_pattern);

void Command_trouble(benchmark::State &state) {
    const Config &config = loaded(16);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Command::trouble(config));
    }
}

BENCHMARK(Command_trouble);

void Command_custom(benchmark::State &state) {
    Config config = bench::Fixtures::config(8, 16);

    config.activeProfile().customCommand = true;
    config.activeProfile().command =
        "{source_port} -iwad {game} -file {addon_0} {addon_1} -config {extracfg}";

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Command::custom(config));
    }
}

BENCHMARK(Command_custom);

void Command_dosSpend(benchmark::State &state) {
    const Config &config = loaded(16);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Command::dosSpend(config));
    }
}

BENCHMARK(Command_dosSpend);

void Storage_paths(benchmark::State &state) {
    const Config &config = loaded(16);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Storage::configFile(config));
        benchmark::DoNotOptimize(Storage::saveDirectory(config));
        benchmark::DoNotOptimize(Storage::replayDirectory(config));
        benchmark::DoNotOptimize(Storage::profileDirectory(config.activeProfile()));
    }
}

BENCHMARK(Storage_paths);

void Storage_saveSlot(benchmark::State &state) {
    const std::string name = "save07.zds";

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Storage::saveSlot(name));
    }
}

BENCHMARK(Storage_saveSlot);

}
