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
#include <array>
#include <utility>

#include "core/launch/Dialect.h"
#include "core/launch/Launcher.h"
#include "core/launch/Storage.h"
#include "gui/Convert.h"
#include "gui/bridge/ConfigBridge.h"
#include "gui/bridge/ProfilePanels.h"

namespace {

// The number goes on the command line; the interface only sees list positions.
constexpr std::array COMPLEVELS = std::to_array<std::pair<int, std::string_view>>({
    {-1, "The port's own"},
    {0, "Doom v1.2"},
    {1, "Doom v1.666"},
    {2, "Doom v1.9"},
    {3, "Ultimate Doom & Doom95"},
    {4, "Final Doom"},
    {5, "DOSDoom"},
    {6, "TASDoom"},
    {7, "Boom's inaccurate vanilla"},
    {8, "Boom v2.01"},
    {9, "Boom v2.02"},
    {10, "LxDoom"},
    {11, "MBF"},
    {12, "PrBoom v2.03 beta"},
    {13, "PrBoom v2.1.0-v2.1.1"},
    {14, "PrBoom v2.2.x"},
    {15, "PrBoom v2.3.x"},
    {16, "PrBoom v2.4.0"},
    {17, "PrBoom, current"},
    {21, "MBF21"},
    {24, "id24"},
});

std::string_view complevelName(const int level) {
    for (const auto &[each, said] : COMPLEVELS) {
        if (each == level) {
            return said;
        }
    }

    return {};
}

// Not offered maps to 0, the port's own.
int complevelIndex(const std::vector<int> &offered, const int level) {
    const auto found = std::ranges::find(offered, level);

    return found == offered.end() ? 0 : static_cast<int>(found - offered.begin());
}

int complevelAt(const std::vector<int> &offered, const int index) {
    return index > 0 && std::cmp_less(index, offered.size())
        ? offered[static_cast<size_t>(index)]
        : -1;
}

}

int ProfilePanels::netRoleOf(const MultiplayerSettings &mp) {
    if (mp.gameType == 0) {
        return 0;
    }

    return mp.players > 0 ? 1 : 2;
}

void ProfilePanels::pushMultiplayer() const {
    const ui::Cfg &state = cfg();
    const MultiplayerSettings &mp = multiplayer();

    state.set_net_role(netRoleOf(mp));
    state.set_game_type(mp.gameType);
    state.set_players(mp.players);
    state.set_host(Convert::text(mp.host));
    state.set_net_port(Convert::text(mp.port));
    state.set_frag_limit(Convert::text(mp.fragLimit));
    state.set_time_limit(Convert::text(mp.timeLimit));
    state.set_dmflags(Convert::text(mp.dmflags));
    state.set_dmflags2(Convert::text(mp.dmflags2));
    state.set_extratic(mp.extratic);
    state.set_netmode(mp.netmode);
    state.set_dup(mp.dup);
    state.set_savegame(Convert::text(mp.savegame));
    state.set_listed(mp.listed);
    state.set_multiplayer_set(mp != MultiplayerSettings());

    const Dialect::NetSupport net = Dialect::net(Dialect::of(config()));

    state.set_net_hosts(net.hosts);
    state.set_net_joins(net.joins);
    state.set_net_players(net.players);
    state.set_net_listing(net.listing);
    state.set_net_frag_limit(net.fragLimit);
    state.set_net_flags(net.flags);
    state.set_net_savegame(net.savegame);
    state.set_net_extratic(net.extratic);
    state.set_has_netmode(net.netmode);
    state.set_net_dup(net.dup);

    _hub->scheduleSave();
}

void ProfilePanels::pushReplay() {
    const ui::Cfg &state = cfg();
    const ReplaySettings &demo = replay();
    const Dialect::DemoSupport speaks = Dialect::demos(Dialect::of(config()));
    const std::filesystem::path folder = Storage::replayDirectory(config());
    const std::filesystem::path file = Storage::replayFile(config());

    state.set_replay_open(active().replayOpen);
    state.set_replay_mode(demo.mode);
    state.set_replay_file(Convert::text(demo.file));
    state.set_replay_playback(demo.playback);
    state.set_replay_longtics(demo.longtics);
    state.set_replay_solo_net(demo.soloNet);
    state.set_replay_set(demo != ReplaySettings());

    state.set_replay_records(speaks.records);
    state.set_replay_timed(speaks.timed);
    state.set_replay_fast(speaks.fast);
    state.set_replay_has_complevel(speaks.complevel != Dialect::Complevels::none);

    const std::vector<int> offered = Dialect::complevels(speaks.complevel);
    std::vector<std::string> complevels;
    std::vector<std::string> numbers;
    complevels.reserve(offered.size());
    numbers.reserve(offered.size());

    for (const int level : offered) {
        complevels.emplace_back(complevelName(level));
        numbers.emplace_back(level < 0 ? "" : std::to_string(level));
    }

    state.set_replay_complevels(Convert::strings(complevels));
    state.set_replay_complevel_numbers(Convert::strings(numbers));
    state.set_replay_complevel(complevelIndex(offered, demo.compatibility));
    state.set_replay_has_longtics(speaks.longtics);
    state.set_replay_has_solo_net(speaks.soloNet);

    if (!_replaysRead || _replaysFrom != folder.string()) {
        _replaysFrom = folder.string();
        _replaysRead = true;
        _replays = Storage::replays(config());
    }

    const auto at = std::ranges::find(_replays, file.filename().string());

    state.set_replay_folder(Convert::fromPath(folder));
    state.set_replay_files(Convert::strings(_replays));
    state.set_replay_path(Convert::fromPath(file));
    state.set_replay_index(at == _replays.end() || file.parent_path() != folder
                           ? -1
                           : static_cast<int>(at - _replays.begin()));

    // Ports overwrite demos without asking.
    std::error_code asked;

    state.set_replay_overwrites(demo.mode == 1 && !file.empty()
                                && std::filesystem::exists(file, asked));
    state.set_replay_trouble(Convert::text(Storage::replayTrouble(config())));

    _hub->scheduleSave();
}

void ProfilePanels::pushSave() {
    const ui::Cfg &state = cfg();
    const Dialect::SaveSupport speaks = Dialect::saves(Dialect::of(config()));
    const std::filesystem::path folder = Storage::saveFolder(config());
    const std::filesystem::path file = Storage::saveFile(config());

    state.set_save_open(active().saveOpen);
    state.set_save_enabled(save().enabled);
    state.set_save_file(Convert::text(save().file));

    state.set_save_loads(speaks.names != Dialect::SaveNames::none && speaks.folder);
    state.set_save_slots(speaks.names == Dialect::SaveNames::slot);

    // A port change changes what counts as a save.
    const std::string from = folder.string() + '\n' + Launcher::executable(config()).string();

    if (!_savesRead || _savesFrom != from) {
        _savesFrom = from;
        _savesRead = true;
        _saves = Storage::saves(config());
    }

    std::vector<std::string> slots;
    slots.reserve(_saves.size());

    for (const std::string &name : _saves) {
        const int slot = speaks.names == Dialect::SaveNames::slot
            ? Storage::saveSlot(name)
            : -1;

        slots.emplace_back(slot < 0 ? std::string() : "Slot " + std::to_string(slot));
    }

    const auto at = std::ranges::find(_saves, file.filename().string());

    state.set_save_folder(Convert::fromPath(folder));
    state.set_save_files(Convert::strings(_saves));
    state.set_save_slot_labels(Convert::strings(slots));
    state.set_save_path(Convert::fromPath(file));
    state.set_save_index(at == _saves.end() || file.parent_path() != folder
                         ? -1
                         : static_cast<int>(at - _saves.begin()));
    state.set_save_trouble(Convert::text(Storage::saveTrouble(config())));

    _hub->scheduleSave();
}

void ProfilePanels::bind() {
    const ui::Cfg &state = cfg();

    // Multiplayer panel.

    state.on_set_multiplayer_open([this](const bool value) {
        active().dialogOpen = value;

        _hub->profile().push();
    });

    state.on_set_net_role([this](const int value) {
        MultiplayerSettings &mp = multiplayer();

        if (value == netRoleOf(mp)) {
            return;
        }

        if (value == 0) {
            mp.gameType = 0;
        } else {
            if (mp.gameType == 0) {
                mp.gameType = 1;
            }

            mp.players = value == 1 ? std::max(mp.players, 2) : 0;
        }

        pushMultiplayer();
        _hub->profile().pushCommand();
        _hub->profile().pushCards();
    });

    state.on_set_game_type([this](const int value) {
        multiplayer().gameType = value;

        pushMultiplayer();
        _hub->profile().pushCommand();
        _hub->profile().pushCards();
    });

    state.on_set_players([this](const int value) {
        multiplayer().players = value;

        pushMultiplayer();
        _hub->profile().pushCommand();
        _hub->profile().pushCards();
    });

    state.on_set_host([this](const slint::SharedString &value) {
        multiplayer().host = Convert::plain(value);

        pushMultiplayer();
        _hub->profile().pushCommand();
    });

    state.on_set_net_port([this](const slint::SharedString &value) {
        multiplayer().port = Convert::plain(value);

        pushMultiplayer();
        _hub->profile().pushCommand();
    });

    state.on_set_frag_limit([this](const slint::SharedString &value) {
        multiplayer().fragLimit = Convert::plain(value);

        pushMultiplayer();
        _hub->profile().pushCommand();
    });

    state.on_set_time_limit([this](const slint::SharedString &value) {
        multiplayer().timeLimit = Convert::plain(value);

        pushMultiplayer();
        _hub->profile().pushCommand();
    });

    state.on_set_dmflags([this](const slint::SharedString &value) {
        multiplayer().dmflags = Convert::plain(value);

        pushMultiplayer();
        _hub->profile().pushCommand();
    });

    state.on_set_dmflags2([this](const slint::SharedString &value) {
        multiplayer().dmflags2 = Convert::plain(value);

        pushMultiplayer();
        _hub->profile().pushCommand();
    });

    state.on_set_extratic([this](const int value) {
        multiplayer().extratic = value;

        pushMultiplayer();
        _hub->profile().pushCommand();
    });

    state.on_set_netmode([this](const int value) {
        multiplayer().netmode = value;

        pushMultiplayer();
        _hub->profile().pushCommand();
    });

    state.on_set_dup([this](const int value) {
        multiplayer().dup = value;

        pushMultiplayer();
        _hub->profile().pushCommand();
    });

    state.on_set_listed([this](const bool value) {
        multiplayer().listed = value;

        pushMultiplayer();
        _hub->profile().pushCommand();
    });

    state.on_set_savegame([this](const slint::SharedString &value) {
        multiplayer().savegame = Convert::plain(value);

        pushMultiplayer();
        _hub->profile().pushCommand();
    });

    state.on_clear_multiplayer([this] {
        multiplayer() = MultiplayerSettings();

        pushMultiplayer();
        _hub->profile().pushCommand();
        _hub->profile().pushCards();
    });

    // Replay panel.

    state.on_set_replay_open([this](const bool value) {
        active().replayOpen = value;

        _hub->profile().push();
    });

    state.on_set_replay_mode([this](const int value) {
        ReplaySettings &demo = replay();

        if (value == demo.mode) {
            return;
        }

        demo.mode = value;

        if (value == 2) {
            // The last run may have written one.
            _replaysRead = false;

            if (demo.file.empty()) {
                if (const std::vector<std::string> found = Storage::replays(config());
                    !found.empty()) {
                    demo.file = found.front();
                }
            }
        }

        pushReplay();
        _hub->profile().pushCommand();
        _hub->profile().pushCards();
    });

    state.on_set_replay_file([this](const slint::SharedString &value) {
        replay().file = Convert::plain(value);

        pushReplay();
        _hub->profile().pushCommand();
    });

    state.on_set_replay_index([this](const int index) {
        replay().file = index >= 0 && std::cmp_less(index, _replays.size())
            ? _replays[static_cast<size_t>(index)]
            : std::string();

        pushReplay();
        _hub->profile().pushCommand();
    });

    state.on_set_replay_playback([this](const int value) {
        replay().playback = value;

        pushReplay();
        _hub->profile().pushCommand();
    });

    state.on_set_replay_complevel([this](const int index) {
        replay().compatibility = complevelAt(
            Dialect::complevels(Dialect::demos(Dialect::of(config())).complevel), index);

        pushReplay();
        _hub->profile().pushCommand();
    });

    state.on_set_replay_longtics([this](const bool value) {
        replay().longtics = value;

        pushReplay();
        _hub->profile().pushCommand();
    });

    state.on_set_replay_solo_net([this](const bool value) {
        replay().soloNet = value;

        pushReplay();
        _hub->profile().pushCommand();
    });

    state.on_refresh_replays([this] {
        _replaysRead = false;

        pushReplay();
    });

    state.on_clear_replay([this] {
        replay() = ReplaySettings();

        pushReplay();
        _hub->profile().pushCommand();
        _hub->profile().pushCards();
    });

    // Saves panel.

    state.on_set_save_open([this](const bool value) {
        active().saveOpen = value;

        _hub->profile().push();
    });

    state.on_set_save_enabled([this](const bool value) {
        SaveSettings &picked = save();

        if (value == picked.enabled) {
            return;
        }

        picked.enabled = value;

        if (value) {
            // The last run may have written one.
            _savesRead = false;

            if (picked.file.empty()) {
                if (const std::vector<std::string> found = Storage::saves(config());
                    !found.empty()) {
                    picked.file = found.front();
                }
            }
        }

        pushSave();
        _hub->profile().pushCommand();
    });

    state.on_set_save_index([this](const int index) {
        save().file = index >= 0 && std::cmp_less(index, _saves.size())
            ? _saves[static_cast<size_t>(index)]
            : std::string();

        pushSave();
        _hub->profile().pushCommand();
    });

    state.on_refresh_saves([this] {
        _savesRead = false;

        pushSave();
    });
}
