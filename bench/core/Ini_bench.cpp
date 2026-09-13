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

#include "core/util/Ini.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace {

void Ini_read(benchmark::State &state) {
    const std::filesystem::path &file = bench::Corpus::ini();

    for ([[maybe_unused]] auto step : state) {
        Ini ini;

        benchmark::DoNotOptimize(ini.read(file));
    }
}

BENCHMARK(Ini_read);

void Ini_write(benchmark::State &state) {
    Ini ini;

    ini.read(bench::Corpus::ini());

    const std::filesystem::path at = bench::Sandbox::scratch("ini") / "out.zdl";

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(ini.write(at));
    }
}

BENCHMARK(Ini_write);

void Ini_section(benchmark::State &state) {
    Ini ini;

    ini.read(bench::Corpus::ini());

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(ini.section("zdl.files"));
    }
}

BENCHMARK(Ini_section);

void Ini_get(benchmark::State &state) {
    Ini ini;

    ini.read(bench::Corpus::ini());

    const Ini::Section *section = ini.section("zdl.files");

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(section->get("f63"));
    }
}

BENCHMARK(Ini_get);

void Ini_startingWith(benchmark::State &state) {
    Ini ini;

    ini.read(bench::Corpus::ini());

    const Ini::Section *section = ini.section("zdl.iwads");

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(section->startingWith("i"));
    }
}

BENCHMARK(Ini_startingWith);

void Ini_set(benchmark::State &state) {
    Ini ini;

    ini.read(bench::Corpus::ini());

    Ini::Section &section = ini.ensure("zdl.files");
    int at = 0;

    for ([[maybe_unused]] auto step : state) {
        section.set("f" + std::to_string(at++ % 64), "/addons/changed.wad");
    }
}

BENCHMARK(Ini_set);

}
