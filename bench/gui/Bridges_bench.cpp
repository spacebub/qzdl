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

#include <memory>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>

#include "core/config/Session.h"
#include "gui/model/ConfigBridge.h"
#include "gui/state/State.h"
#include "support/Canvas.h"
#include "support/Fixtures.h"
#include "support/Rig.h"

namespace {

// Installed unconditionally: the Session and State are singletons every
// benchmark file shares, so a cached fixture leaks between them.
ConfigBridge &loaded(const int profiles, const int files) {
    bench::shared().forget();
    bench::Fixtures::install(bench::Fixtures::config(profiles, files));

    bench::shared().config().reload();

    return bench::shared().config();
}

void Bridge_reload(benchmark::State &state) {
    const auto profiles = static_cast<int>(state.range(0));

    ConfigBridge &bridge = loaded(profiles, 16);

    for ([[maybe_unused]] auto step : state) {
        bridge.reload();
    }

    state.SetItemsProcessed(state.iterations() * profiles);
}

BENCHMARK(Bridge_reload)->Arg(8)->Arg(64)->Arg(512);

void ProfileBridge_push(benchmark::State &state) {
    ConfigBridge &bridge = loaded(64, static_cast<int>(state.range(0)));

    for ([[maybe_unused]] auto step : state) {
        bridge.profile().push();
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}

BENCHMARK(ProfileBridge_push)->Arg(0)->Arg(16)->Arg(512);

void ProfileBridge_pushCards(benchmark::State &state) {
    const auto profiles = static_cast<int>(state.range(0));

    ConfigBridge &bridge = loaded(profiles, 16);

    for ([[maybe_unused]] auto step : state) {
        bridge.profile().pushCards();
    }

    state.SetItemsProcessed(state.iterations() * profiles);
}

BENCHMARK(ProfileBridge_pushCards)->Arg(8)->Arg(64)->Arg(512);

// What a keystroke in the profile page pays: the preview and the save are only
// scheduled here, so this is the debounce rather than the rebuild.
void ProfileBridge_pushCommand(benchmark::State &state) {
    ConfigBridge &bridge = loaded(64, 16);

    for ([[maybe_unused]] auto step : state) {
        bridge.profile().pushCommand();
    }
}

BENCHMARK(ProfileBridge_pushCommand);

// The rebuild the debounce leads to.
void ProfileBridge_showCommand(benchmark::State &state) {
    loaded(64, static_cast<int>(state.range(0)));

    for ([[maybe_unused]] auto step : state) {
        ProfileBridge::showCommand();
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}

BENCHMARK(ProfileBridge_showCommand)->Arg(0)->Arg(16)->Arg(512);

void ProfileBridge_cardOf(benchmark::State &state) {
    loaded(64, 16);

    int at = 0;

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(ProfileBridge::cardOf(at++ % 64));
    }
}

BENCHMARK(ProfileBridge_cardOf);

void ProfileBridge_badgesOf(benchmark::State &state) {
    loaded(64, 16);

    std::vector<State::ProfileCard> cards;

    for (int at = 0; at < 64; ++at) {
        cards.push_back(ProfileBridge::cardOf(at));
    }

    size_t at = 0;

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(ProfileBridge::badgesOf(cards[at++ % cards.size()]));
    }
}

BENCHMARK(ProfileBridge_badgesOf);

void ListsBridge_push(benchmark::State &state) {
    ConfigBridge &bridge = loaded(64, static_cast<int>(state.range(0)));

    for ([[maybe_unused]] auto step : state) {
        bridge.lists().push();
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}

BENCHMARK(ListsBridge_push)->Arg(16)->Arg(512);

void SettingsBridge_push(benchmark::State &state) {
    ConfigBridge &bridge = loaded(64, 16);

    for ([[maybe_unused]] auto step : state) {
        bridge.settings().push();
    }
}

BENCHMARK(SettingsBridge_push);

void ProfilePanels_pushMultiplayer(benchmark::State &state) {
    ConfigBridge &bridge = loaded(64, 16);

    for ([[maybe_unused]] auto step : state) {
        bridge.panels().pushMultiplayer();
    }
}

BENCHMARK(ProfilePanels_pushMultiplayer);

void ProfilePanels_pushReplay(benchmark::State &state) {
    ConfigBridge &bridge = loaded(64, 16);

    for ([[maybe_unused]] auto step : state) {
        bridge.panels().pushReplay();
    }
}

BENCHMARK(ProfilePanels_pushReplay);

// Typed into on every keystroke: the shelf is refiltered and rebuilt.
void LibraryBridge_setFilter(benchmark::State &state) {
    const auto profiles = static_cast<int>(state.range(0));

    ConfigBridge &bridge = loaded(profiles, 16);

    const std::string needle = "doom";
    size_t at = 0;

    for ([[maybe_unused]] auto step : state) {
        bridge.library().setFilter(needle.substr(0, (at++ % needle.size()) + 1));
    }

    state.SetItemsProcessed(state.iterations() * profiles);
}

BENCHMARK(LibraryBridge_setFilter)->Arg(8)->Arg(64)->Arg(512);

void LibraryBridge_pushShelf(benchmark::State &state) {
    const auto profiles = static_cast<int>(state.range(0));

    ConfigBridge &bridge = loaded(profiles, 16);

    for ([[maybe_unused]] auto step : state) {
        bridge.library().pushShelf();
    }

    state.SetItemsProcessed(state.iterations() * profiles);
}

BENCHMARK(LibraryBridge_pushShelf)->Arg(8)->Arg(64)->Arg(512);

void LibraryBridge_pushGameRev(benchmark::State &state) {
    ConfigBridge &bridge = loaded(64, 16);

    for ([[maybe_unused]] auto step : state) {
        bridge.library().pushGameRev();
    }
}

BENCHMARK(LibraryBridge_pushGameRev);

void ProfileBridge_setProfileIndex(benchmark::State &state) {
    ConfigBridge &bridge = loaded(64, 16);

    int at = 0;

    for ([[maybe_unused]] auto step : state) {
        bridge.profile().setProfileIndex(at++ % 64);
    }
}

BENCHMARK(ProfileBridge_setProfileIndex);

// One field edited, with the save and the preview it schedules.
void ProfileBridge_setWarp(benchmark::State &state) {
    ConfigBridge &bridge = loaded(64, 16);

    int at = 0;

    for ([[maybe_unused]] auto step : state) {
        bridge.profile().setWarp("MAP" + std::to_string((at++ % 32) + 1));
    }
}

BENCHMARK(ProfileBridge_setWarp);

}
