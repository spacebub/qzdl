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
#include <fstream>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>

#include "ttk/system/Paths.h"

#include "core/config/Session.h"
#include "core/launch/Storage.h"
#include "gui/state/State.h"
#include "support/Canvas.h"
#include "support/Corpus.h"
#include "support/Fixtures.h"
#include "support/Rig.h"

namespace {

// Installed unconditionally. See the note in Library_bench.cpp.
bench::Rig &page(const int profiles, const int files) {
    bench::Rig &rig = bench::shared();

    rig.forget();

    bench::Fixtures::install(bench::Fixtures::config(profiles, files));

    rig.config().reload();

    rig.go(State::Page::Profile);
    rig.sync();
    rig.ready();

    return rig;
}

void Profile_sync(benchmark::State &state) {
    const auto files = static_cast<int>(state.range(0));

    bench::Rig &rig = page(64, files);

    for ([[maybe_unused]] auto step : state) {
        rig.sync();
    }

    state.SetItemsProcessed(state.iterations() * files);
}

BENCHMARK(Profile_sync)->Arg(0)->Arg(16)->Arg(512);

void Profile_fullPaint(benchmark::State &state) {
    const auto files = static_cast<int>(state.range(0));

    bench::Rig &rig = page(64, files);

    std::size_t painted = 0;

    for ([[maybe_unused]] auto step : state) {
        painted = rig.canvas().full();
    }

    state.counters["pixels"] = static_cast<double>(painted);
}

// A wheel notch down the page and the frames it animates over. The profile page
// is a column of controls, not a grid of sprites, so it damages differently.
void Profile_scroll(benchmark::State &state) {
    const auto files = static_cast<int>(state.range(0));

    bench::Rig &rig = page(64, files);

    ttk::Root &root = rig.ui();

    std::size_t painted = 0;
    double steps = -1.0;

    for ([[maybe_unused]] auto step : state) {
        root.wheel(steps, 600.0, 400.0);

        painted = 0;

        for (int frame = 0; frame < 8; ++frame) {
            painted += rig.canvas().frameAt(rig.canvas().tick());
        }

        steps = -steps;
    }

    state.counters["pixels"] = static_cast<double>(painted) / 8.0;
}

BENCHMARK(Profile_scroll)->Arg(16)->Arg(512);

BENCHMARK(Profile_fullPaint)->Arg(16)->Arg(512);

// Picking another profile: the config, every panel and the whole page.
void Profile_switchProfile(benchmark::State &state) {
    bench::Rig &rig = page(64, 16);

    int at = 0;

    for ([[maybe_unused]] auto step : state) {
        rig.config().profile().setProfileIndex(at++ % 64);
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());

        state.PauseTiming();
        rig.still();
        state.ResumeTiming();
    }
}

BENCHMARK(Profile_switchProfile);

// A fold opening or closing, which relays out everything below it.
void Profile_toggleMultiplayer(benchmark::State &state) {
    bench::Rig &rig = page(64, 16);

    bool open = false;

    for ([[maybe_unused]] auto step : state) {
        open = !open;

        rig.config().panels().setMultiplayerOpen(open);
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());
    }
}

BENCHMARK(Profile_toggleMultiplayer);

// Typing into the extra arguments field, sync and frame per keystroke.
void Profile_typeExtra(benchmark::State &state) {
    const auto files = static_cast<int>(state.range(0));

    bench::Rig &rig = page(64, files);

    const std::string typed = "-fast -respawn -nomonsters";
    size_t at = 0;

    for ([[maybe_unused]] auto step : state) {
        rig.config().profile().setExtra(typed.substr(0, (at++ % typed.size()) + 1));
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());
    }
}

BENCHMARK(Profile_typeExtra)->Arg(16)->Arg(512);

// The same on the port's own settings, where every edit asks the port where it keeps saves.
void Profile_typeExtraOnPortSettings(benchmark::State &state) {
    bench::Rig &rig = page(64, 16);
    const std::filesystem::path home = ttk::Paths::home_directory();
#ifdef _WIN32
    const std::filesystem::path ini = home / "Documents" / "My Games" / "GZDoom" / "gzdoom.ini";
#else
    const std::filesystem::path ini = home / ".config" / "gzdoom" / "gzdoom.ini";
#endif
    const std::filesystem::path saves = home / "doom-saves";

    std::filesystem::create_directories(ini.parent_path());
    std::ofstream(ini) << "[GlobalSettings]\nsave_dir=$HOME/doom-saves\n";
    bench::Corpus::saves(saves, 8);

    // The first profile is on a DOS port, which keeps no saves of its own.
    rig.config().profile().setProfileIndex(1);
    rig.config().profile().setSharedConfig(true);
    rig.config().panels().setSaveOpen(true);
    rig.sync();
    rig.ready();

    // Without the ini read, the port answers a default folder and the bench would time that.
    if (std::error_code code; !std::filesystem::equivalent(Storage::saveFolder(Session::get().config()), saves, code)) {
        state.SkipWithError("the port did not take its saves folder from the ini");
    }

    const std::string typed = "-fast -respawn -nomonsters";
    size_t at = 0;

    for ([[maybe_unused]] auto step : state) {
        rig.config().profile().setExtra(typed.substr(0, (at++ % typed.size()) + 1));
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());
    }

    std::filesystem::remove(ini);
    std::filesystem::remove_all(saves);
}

BENCHMARK(Profile_typeExtraOnPortSettings);

void Profile_setSkill(benchmark::State &state) {
    bench::Rig &rig = page(64, 16);

    int at = 0;

    for ([[maybe_unused]] auto step : state) {
        rig.config().profile().setSkill((at++ % 5) + 1);
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());
    }
}

BENCHMARK(Profile_setSkill);

// The file list growing and emptying, which is the list view's worst case.
void Profile_addAndClearFiles(benchmark::State &state) {
    bench::Rig &rig = page(8, 0);

    const std::vector<std::string> &files = bench::Corpus::addons(static_cast<int>(state.range(0)));

    for ([[maybe_unused]] auto step : state) {
        rig.config().lists().addFiles(files);
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());

        rig.config().lists().clearFiles();
        rig.sync();
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}

BENCHMARK(Profile_addAndClearFiles)->Arg(16)->Arg(64);

void Profile_toggleFile(benchmark::State &state) {
    bench::Rig &rig = page(64, 64);

    int at = 0;
    bool on = false;

    for ([[maybe_unused]] auto step : state) {
        on = !on;

        rig.config().lists().setFileEnabled(at++ % 64, on);
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());
    }
}

BENCHMARK(Profile_toggleFile);

void Profile_moveFile(benchmark::State &state) {
    bench::Rig &rig = page(64, 64);

    int at = 0;

    for ([[maybe_unused]] auto step : state) {
        const int from = at++ % 63;

        rig.config().lists().moveFile(from, from + 1);
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());
    }
}

BENCHMARK(Profile_moveFile);

}
