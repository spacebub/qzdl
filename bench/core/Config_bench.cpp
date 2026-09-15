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
#include <cstdint>
#include <filesystem>
#include <string>

#include <benchmark/benchmark.h>

#include "core/config/Config.h"
#include "core/config/Import.h"
#include "core/util/Ini.h"
#include "support/Corpus.h"
#include "support/Fixtures.h"
#include "support/Sandbox.h"

namespace {

void Config_load(benchmark::State &state) {
    const auto profiles = static_cast<int>(state.range(0));
    const std::filesystem::path &path = bench::Corpus::json(profiles, 16);

    for ([[maybe_unused]] auto step : state) {
        Config config;

        if (!config.load(path) || std::cmp_not_equal(config.profiles.size(), profiles)) {
            state.SkipWithError("the config did not load");

            break;
        }

        benchmark::DoNotOptimize(config);
    }

    state.SetItemsProcessed(state.iterations() * profiles);
}

BENCHMARK(Config_load)->Arg(8)->Arg(64)->Arg(512);

void Config_save(benchmark::State &state) {
    const auto profiles = static_cast<int>(state.range(0));
    const Config config = bench::Fixtures::config(profiles, 16);
    const std::filesystem::path at = bench::Sandbox::scratch("config") / "saved.json";

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(config.save(at));
    }

    state.SetItemsProcessed(state.iterations() * profiles);
}

BENCHMARK(Config_save)->Arg(8)->Arg(64)->Arg(512);

void Config_loadShaped(benchmark::State &state) {
    const std::filesystem::path &path = bench::Corpus::json(
        static_cast<int>(state.range(0)), static_cast<int>(state.range(1)), static_cast<int>(state.range(2)));

    for ([[maybe_unused]] auto step : state) {
        Config config;

        if (!config.load(path)) {
            state.SkipWithError("the config did not load");

            break;
        }

        benchmark::DoNotOptimize(config);
    }

    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(std::filesystem::file_size(path)));
}

BENCHMARK(Config_loadShaped)->Apply(bench::Fixtures::shapes);

void Config_saveShaped(benchmark::State &state) {
    const Config config = bench::Fixtures::shaped(
        static_cast<int>(state.range(0)), static_cast<int>(state.range(1)), static_cast<int>(state.range(2)));
    const std::filesystem::path at = bench::Sandbox::scratch("config-shaped") / "saved.json";

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(config.save(at));
    }

    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(std::filesystem::file_size(at)));
}

BENCHMARK(Config_saveShaped)->Apply(bench::Fixtures::shapes);

void Config_copy(benchmark::State &state) {
    const Config config = bench::Fixtures::config(static_cast<int>(state.range(0)), 16);

    for ([[maybe_unused]] auto step : state) {
        Config copy = config;

        benchmark::DoNotOptimize(copy);
    }
}

BENCHMARK(Config_copy)->Arg(8)->Arg(64)->Arg(512);

void Config_addProfile(benchmark::State &state) {
    const Config base = bench::Fixtures::config(64, 16);

    for ([[maybe_unused]] auto step : state) {
        state.PauseTiming();

        Config config = base;

        state.ResumeTiming();

        benchmark::DoNotOptimize(config.addProfile("Added"));
    }
}

BENCHMARK(Config_addProfile);

void Config_duplicateActiveProfile(benchmark::State &state) {
    const Config base = bench::Fixtures::config(64, 16);

    for ([[maybe_unused]] auto step : state) {
        state.PauseTiming();

        Config config = base;

        state.ResumeTiming();

        benchmark::DoNotOptimize(config.duplicateActiveProfile("Copy"));
    }
}

BENCHMARK(Config_duplicateActiveProfile);

void Config_uniqueProfileName(benchmark::State &state) {
    const Config config = bench::Fixtures::config(static_cast<int>(state.range(0)), 16);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(config.uniqueProfileName("Copy"));
    }
}

BENCHMARK(Config_uniqueProfileName)->Arg(8)->Arg(512);

void Config_setActiveProfile(benchmark::State &state) {
    Config config = bench::Fixtures::config(512, 16);
    int at = 0;

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(config.setActiveProfile("profile-" + std::to_string(at++ % 512)));
    }
}

BENCHMARK(Config_setActiveProfile);

void Config_indexOfProfile(benchmark::State &state) {
    const Config config = bench::Fixtures::config(512, 16);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(config.indexOfProfile("profile-511"));
    }
}

BENCHMARK(Config_indexOfProfile);

void Config_findIwad(benchmark::State &state) {
    const Config config = bench::Fixtures::config(8, 16);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(config.findIwad("Game 11"));
    }
}

BENCHMARK(Config_findIwad);

void Config_findPort(benchmark::State &state) {
    const Config config = bench::Fixtures::config(8, 16);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(config.findPort("Port 11"));
    }
}

BENCHMARK(Config_findPort);

void Config_reset(benchmark::State &state) {
    for ([[maybe_unused]] auto step : state) {
        state.PauseTiming();

        Config config = bench::Fixtures::config(64, 16);

        state.ResumeTiming();

        config.reset();

        benchmark::DoNotOptimize(config);
    }
}

BENCHMARK(Config_reset);

void Import_loadLegacyFile(benchmark::State &state) {
    const std::filesystem::path &path = bench::Corpus::ini();

    for ([[maybe_unused]] auto step : state) {
        Config config;

        benchmark::DoNotOptimize(Import::loadLegacyFile(path, config));
        benchmark::DoNotOptimize(config);
    }
}

BENCHMARK(Import_loadLegacyFile);

void Import_fromLegacy(benchmark::State &state) {
    Ini ini;

    ini.read(bench::Corpus::ini());

    for ([[maybe_unused]] auto step : state) {
        Config config;

        Import::fromLegacy(ini, config);

        benchmark::DoNotOptimize(config);
    }
}

BENCHMARK(Import_fromLegacy);

void Import_saveZdlFile(benchmark::State &state) {
    const Config config = bench::Fixtures::config(4, 64);
    const std::filesystem::path at = bench::Sandbox::scratch("zdl") / "out.zdl";

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Import::saveZdlFile(at, config.profiles.front()));
    }
}

BENCHMARK(Import_saveZdlFile);

void Import_loadZdlFile(benchmark::State &state) {
    const Config config = bench::Fixtures::config(4, 64);
    const std::filesystem::path at = bench::Sandbox::scratch("zdl-read") / "in.zdl";

    Import::saveZdlFile(at, config.profiles.front());

    for ([[maybe_unused]] auto step : state) {
        Profile profile;

        if (!Import::loadZdlFile(at, profile)) {
            state.SkipWithError("the .zdl did not load");

            break;
        }

        benchmark::DoNotOptimize(profile);
    }
}

BENCHMARK(Import_loadZdlFile);

void Import_profileToSection(benchmark::State &state) {
    const Config config = bench::Fixtures::config(4, 64);
    const Profile &profile = config.profiles.front();

    for ([[maybe_unused]] auto step : state) {
        Ini::Section section;

        Import::profileToSection(profile, section);

        benchmark::DoNotOptimize(section);
    }
}

BENCHMARK(Import_profileToSection);

void Import_profileFromSection(benchmark::State &state) {
    const Config config = bench::Fixtures::config(4, 64);

    Ini::Section section;

    Import::profileToSection(config.profiles.front(), section);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Import::profileFromSection(section));
    }
}

BENCHMARK(Import_profileFromSection);

}
