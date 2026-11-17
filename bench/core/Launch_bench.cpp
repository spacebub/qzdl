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

#include <array>
#include <filesystem>
#include <string>

#include <benchmark/benchmark.h>

#include "core/config/Config.h"
#include "core/config/Options.h"
#include "core/launch/Arguments.h"
#include "core/launch/Command.h"
#include "core/launch/Dialect.h"
#include "core/launch/Flags.h"
#include "core/launch/Storage.h"
#include "support/Fixtures.h"

namespace {

const Config &loaded(const int files) {
    static Config held;
    static int last = -1;

    if (files != last) {
        held = bench::Fixtures::config(8, files);
        held.engines.front().file = "/ports/gzdoom";
        held.activeProfile().engine = held.engines.front().name;
        last = files;
    }

    return held;
}

void Dialect_ofPath(benchmark::State &state) {
    const std::filesystem::path file = "/usr/games/gzdoom";

    for ([[maybe_unused]] const auto step : state) {
        benchmark::DoNotOptimize(Dialect::of(file));
    }
}

BENCHMARK(Dialect_ofPath);

void Dialect_ofConfig(benchmark::State &state) {
    const Config &config = loaded(16);

    for ([[maybe_unused]] const auto step : state) {
        benchmark::DoNotOptimize(Dialect::of(config));
    }
}

BENCHMARK(Dialect_ofConfig);

void Dialect_support(benchmark::State &state) {
    const Dialect::Traits traits = Dialect::of(std::filesystem::path("/usr/games/dsda-doom"));

    for ([[maybe_unused]] const auto step : state) {
        benchmark::DoNotOptimize(traits.has(Dialect::Feature::timedemo));
        benchmark::DoNotOptimize(traits.has(Dialect::Feature::saveFolder));
        benchmark::DoNotOptimize(traits.has(Dialect::Feature::hosts));
    }
}

BENCHMARK(Dialect_support);

void Dialect_complevels(benchmark::State &state) {
    for ([[maybe_unused]] const auto step : state) {
        benchmark::DoNotOptimize(Dialect::complevels(Dialect::Complevels::dsda));
    }
}

BENCHMARK(Dialect_complevels);

// The command preview is rebuilt on every keystroke in the profile page.
void Arguments_of(benchmark::State &state) {
    const Config &config = loaded(static_cast<int>(state.range(0)));

    for ([[maybe_unused]] const auto step : state) {
        benchmark::DoNotOptimize(Arguments::of(config));
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}

BENCHMARK(Arguments_of)->Arg(0)->Arg(16)->Arg(128)->Arg(512);

// Every word and option the port reads, given, on a ZDoom and a Boom family port.
void Arguments_ofGameplay(benchmark::State &state) {
    Config config = loaded(16);
    Profile &profile = config.activeProfile();
    const bool boom = state.range(0) != 0;

    config.engines.front().file = boom ? "/ports/dsda-doom" : "/ports/gzdoom";
    config.engines.front().dosbox = false;

    for (const Flags::Word word : Flags::ALL) {
        profile.setFlag(word, std::to_string(1000U + (Flags::index(word) * 4097U)));
    }

    for (const Options::Option &option : Options::ALL) {
        if (option.kind == Options::Kind::toggle) {
            profile.setOption(option.feature, true);
        } else {
            profile.setNumber(option.feature, "150");
        }
    }

    profile.complevel = boom ? 21 : -1;

    for ([[maybe_unused]] const auto step : state) {
        benchmark::DoNotOptimize(Arguments::of(config));
    }
}

BENCHMARK(Arguments_ofGameplay)->Arg(0)->Arg(1);

void Flags_parse(benchmark::State &state) {
    const std::array<std::string, 4> typed = {"0", "2113540", "4294967295", "12a"};
    size_t at = 0;

    for ([[maybe_unused]] const auto step : state) {
        benchmark::DoNotOptimize(Flags::parse(typed[at++ % typed.size()]));
    }
}

BENCHMARK(Flags_parse);

void Arguments_maps(benchmark::State &state) {
    const Config &config = loaded(16);

    for ([[maybe_unused]] const auto step : state) {
        benchmark::DoNotOptimize(Arguments::maps(config));
    }
}

BENCHMARK(Arguments_maps);

void Command_line(benchmark::State &state) {
    const Config &config = loaded(static_cast<int>(state.range(0)));

    for ([[maybe_unused]] const auto step : state) {
        benchmark::DoNotOptimize(Command::line(config));
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}

BENCHMARK(Command_line)->Arg(0)->Arg(16)->Arg(128)->Arg(512);

void Command_pattern(benchmark::State &state) {
    const Config &config = loaded(16);

    for ([[maybe_unused]] const auto step : state) {
        benchmark::DoNotOptimize(Command::pattern(config));
    }
}

BENCHMARK(Command_pattern);

void Command_trouble(benchmark::State &state) {
    const Config &config = loaded(16);

    for ([[maybe_unused]] const auto step : state) {
        benchmark::DoNotOptimize(Command::trouble(config));
    }
}

BENCHMARK(Command_trouble);

void Command_custom(benchmark::State &state) {
    Config config = bench::Fixtures::config(8, 16);

    config.activeProfile().set(Switch::customCommand, true);
    config.activeProfile().command = "{source_port} -iwad {game} -file {addon_0} {addon_1} -config {extracfg}";

    for ([[maybe_unused]] const auto step : state) {
        benchmark::DoNotOptimize(Command::custom(config));
    }
}

BENCHMARK(Command_custom);

void Command_dosSpend(benchmark::State &state) {
    const Config &config = loaded(16);

    for ([[maybe_unused]] const auto step : state) {
        benchmark::DoNotOptimize(Command::dosSpend(config));
    }
}

BENCHMARK(Command_dosSpend);

void Storage_paths(benchmark::State &state) {
    const Config &config = loaded(16);

    for ([[maybe_unused]] const auto step : state) {
        benchmark::DoNotOptimize(Storage::configFile(config));
        benchmark::DoNotOptimize(Storage::saveDirectory(config));
        benchmark::DoNotOptimize(Storage::replayDirectory(config));
        benchmark::DoNotOptimize(Storage::profileDirectory(config.activeProfile()));
    }
}

BENCHMARK(Storage_paths);

void Storage_saveSlot(benchmark::State &state) {
    const std::string name = "save07.zds";

    for ([[maybe_unused]] const auto step : state) {
        benchmark::DoNotOptimize(Storage::saveSlot(name));
    }
}

BENCHMARK(Storage_saveSlot);

}
