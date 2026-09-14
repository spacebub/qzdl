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

#include "gui/state/State.h"
#include "support/Fixtures.h"

namespace {

std::vector<State::ProfileCard> cards(const int count) {
    std::vector<State::ProfileCard> out;

    out.reserve(static_cast<size_t>(count));

    for (int at = 0; at < count; ++at) {
        out.push_back({.index = at,
                       .id = "profile-" + std::to_string(at),
                       .key = "profile:" + std::to_string(at),
                       .name = bench::Fixtures::words(3, static_cast<unsigned>(at) + 11U),
                       .iwad = "Doom II",
                       .artKey = "doom2.wad",
                       .port = "GZDoom",
                       .dosPort = false,
                       .warp = "MAP07",
                       .files = 12,
                       .loaded = 9,
                       .netRole = NetRole::Alone,
                       .ready = true,
                       .netSupported = false,
                       .replayMode = ReplayMode::Off});
    }

    return out;
}

std::vector<State::FileRow> rows(const int count) {
    std::vector<State::FileRow> out;

    out.reserve(static_cast<size_t>(count));

    for (int at = 0; at < count; ++at) {
        out.push_back({.index = at,
                       .file = "/addons/addon" + std::to_string(at) + ".wad",
                       .name = "addon" + std::to_string(at) + ".wad",
                       .directory = "/addons",
                       .loaded = at % 4 != 0,
                       .missing = false});
    }

    return out;
}

void State_get(benchmark::State &state) {
    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(&State::get());
    }
}

BENCHMARK(State_get);

// A push only assigns when the built list differs, so the compare is the hot part.
void State_compareCards(benchmark::State &state) {
    const auto count = static_cast<int>(state.range(0));
    const std::vector<State::ProfileCard> left = cards(count);
    const std::vector<State::ProfileCard> right = cards(count);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(left == right);
    }

    state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(State_compareCards)->Arg(8)->Arg(64)->Arg(512);

void State_buildCards(benchmark::State &state) {
    const auto count = static_cast<int>(state.range(0));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(cards(count));
    }

    state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(State_buildCards)->Arg(8)->Arg(64)->Arg(512);

void State_assignCards(benchmark::State &state) {
    const auto count = static_cast<int>(state.range(0));
    const std::vector<State::ProfileCard> built = cards(count);

    std::vector<State::ProfileCard> into;

    for ([[maybe_unused]] auto step : state) {
        into = built;

        benchmark::DoNotOptimize(into);
    }

    state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(State_assignCards)->Arg(8)->Arg(64)->Arg(512);

void State_compareFileRows(benchmark::State &state) {
    const auto count = static_cast<int>(state.range(0));
    const std::vector<State::FileRow> left = rows(count);
    const std::vector<State::FileRow> right = rows(count);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(left == right);
    }

    state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(State_compareFileRows)->Arg(16)->Arg(512);

// Every field of the active profile is transcribed into Cfg on each push.
void State_copyCfg(benchmark::State &state) {
    State::Cfg source;

    source.profileCards = cards(static_cast<int>(state.range(0)));
    source.files = rows(static_cast<int>(state.range(0)));
    source.commandLine = bench::Fixtures::paragraph(2);

    for ([[maybe_unused]] auto step : state) {
        State::Cfg copy = source;

        benchmark::DoNotOptimize(copy);
    }
}

BENCHMARK(State_copyCfg)->Arg(8)->Arg(512);

void State_touch(benchmark::State &state) {
    int woken = 0;

    State::get().changed = [&woken] { woken++; };

    for ([[maybe_unused]] auto step : state) {
        State::get().touch();
    }

    State::get().changed = nullptr;

    benchmark::DoNotOptimize(woken);
}

BENCHMARK(State_touch);

}
