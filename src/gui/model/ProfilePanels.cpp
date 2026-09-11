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
#include "gui/model/ConfigBridge.h"
#include "gui/model/ProfilePanels.h"
#include "gui/util/Format.h"

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
    State::Cfg &state = cfg();
    const MultiplayerSettings &mp = multiplayer();

    state.netRole = netRoleOf(mp);
    state.gameType = mp.gameType;
    state.players = mp.players;
    state.host = mp.host;
    state.netPort = mp.port;
    state.fragLimit = mp.fragLimit;
    state.timeLimit = mp.timeLimit;
    state.dmflags = mp.dmflags;
    state.dmflags2 = mp.dmflags2;
    state.extratic = mp.extratic;
    state.netmode = mp.netmode;
    state.dup = mp.dup;
    state.savegame = mp.savegame;
    state.listed = mp.listed;
    state.multiplayerSet = mp != MultiplayerSettings();

    const Dialect::NetSupport net = Dialect::net(Dialect::of(config()));

    state.netHosts = net.hosts;
    state.netJoins = net.joins;
    state.netPlayers = net.players;
    state.netListing = net.listing;
    state.netFragLimit = net.fragLimit;
    state.netFlags = net.flags;
    state.netSavegame = net.savegame;
    state.netExtratic = net.extratic;
    state.hasNetmode = net.netmode;
    state.netDup = net.dup;

    _hub->scheduleSave();

    State::get().touch();
}

void ProfilePanels::pushReplay() {
    State::Cfg &state = cfg();
    const ReplaySettings &demo = replay();
    const Dialect::DemoSupport speaks = Dialect::demos(Dialect::of(config()));
    const std::filesystem::path folder = Storage::replayDirectory(config());
    const std::filesystem::path file = Storage::replayFile(config());

    state.replayOpen = active().replayOpen;
    state.replayMode = demo.mode;
    state.replayFile = demo.file;
    state.replayPlayback = demo.playback;
    state.replayLongtics = demo.longtics;
    state.replaySoloNet = demo.soloNet;
    state.replaySet = demo != ReplaySettings();

    state.replayRecords = speaks.records;
    state.replayTimed = speaks.timed;
    state.replayFast = speaks.fast;
    state.replayHasComplevel = speaks.complevel != Dialect::Complevels::none;

    const std::vector<int> offered = Dialect::complevels(speaks.complevel);
    std::vector<std::string> complevels;
    std::vector<std::string> numbers;

    complevels.reserve(offered.size());
    numbers.reserve(offered.size());

    for (const int level : offered) {
        complevels.emplace_back(complevelName(level));
        numbers.emplace_back(level < 0 ? "" : std::to_string(level));
    }

    state.replayComplevels = std::move(complevels);
    state.replayComplevelNumbers = std::move(numbers);
    state.replayComplevel = complevelIndex(offered, demo.compatibility);
    state.replayHasLongtics = speaks.longtics;
    state.replayHasSoloNet = speaks.soloNet;

    if (!_replaysRead || _replaysFrom != folder.string()) {
        _replaysFrom = folder.string();
        _replaysRead = true;
        _replays = Storage::replays(config());
    }

    const auto at = std::ranges::find(_replays, file.filename().string());

    state.replayFolder = Format::fromPath(folder);
    state.replayFiles = _replays;
    state.replayPath = Format::fromPath(file);
    state.replayIndex = at == _replays.end() || file.parent_path() != folder
        ? -1
        : static_cast<int>(at - _replays.begin());

    std::error_code asked;

    state.replayNameTaken = demo.mode == 1 && !file.empty()
        && std::filesystem::exists(file, asked);
    state.replayTrouble = Storage::replayTrouble(config());

    _hub->scheduleSave();

    State::get().touch();
}

void ProfilePanels::pushSave() {
    State::Cfg &state = cfg();
    const Dialect::SaveSupport speaks = Dialect::saves(Dialect::of(config()));
    const std::filesystem::path folder = Storage::saveFolder(config());
    const std::filesystem::path file = Storage::saveFile(config());

    state.saveOpen = active().saveOpen;
    state.saveEnabled = save().enabled;
    state.saveFile = save().file;

    state.saveLoads = speaks.names != Dialect::SaveNames::none && speaks.folder;
    state.saveSlots = speaks.names == Dialect::SaveNames::slot;

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
        const int slot = speaks.names == Dialect::SaveNames::slot ? Storage::saveSlot(name) : -1;

        slots.emplace_back(slot < 0 ? std::string() : "Slot " + std::to_string(slot));
    }

    const auto at = std::ranges::find(_saves, file.filename().string());

    state.saveFolder = Format::fromPath(folder);
    state.saveFiles = _saves;
    state.saveSlotLabels = std::move(slots);
    state.savePath = Format::fromPath(file);
    state.saveIndex = at == _saves.end() || file.parent_path() != folder
        ? -1
        : static_cast<int>(at - _saves.begin());
    state.saveTrouble = Storage::saveTrouble(config());

    _hub->scheduleSave();

    State::get().touch();
}

// --- multiplayer ---------------------------------------------------------------

void ProfilePanels::setMultiplayerOpen(const bool value) const {
    active().dialogOpen = value;

    _hub->profile().push();
}

void ProfilePanels::setNetRole(const int value) {
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
}

void ProfilePanels::setGameType(const int value) {
    multiplayer().gameType = value;

    pushMultiplayer();
    _hub->profile().pushCommand();
    _hub->profile().pushCards();
}

void ProfilePanels::setPlayers(const int value) {
    multiplayer().players = value;

    pushMultiplayer();
    _hub->profile().pushCommand();
    _hub->profile().pushCards();
}

void ProfilePanels::setHost(const std::string &value) {
    multiplayer().host = value;

    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setNetPort(const std::string &value) {
    multiplayer().port = value;

    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setFragLimit(const std::string &value) {
    multiplayer().fragLimit = value;

    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setTimeLimit(const std::string &value) {
    multiplayer().timeLimit = value;

    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setDmflags(const std::string &value) {
    multiplayer().dmflags = value;

    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setDmflags2(const std::string &value) {
    multiplayer().dmflags2 = value;

    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setExtratic(const int value) {
    multiplayer().extratic = value;

    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setNetmode(const int value) {
    multiplayer().netmode = value;

    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setDup(const int value) {
    multiplayer().dup = value;

    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setListed(const bool value) {
    multiplayer().listed = value;

    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setSavegame(const std::string &value) {
    multiplayer().savegame = value;

    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::clearMultiplayer() {
    multiplayer() = MultiplayerSettings();

    pushMultiplayer();
    _hub->profile().pushCommand();
    _hub->profile().pushCards();
}

// --- replay --------------------------------------------------------------------

void ProfilePanels::setReplayOpen(const bool value) const {
    active().replayOpen = value;

    _hub->profile().push();
}

void ProfilePanels::setReplayMode(const int value) {
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
}

void ProfilePanels::setReplayFile(const std::string &value) {
    replay().file = value;

    pushReplay();
    _hub->profile().pushCommand();
}

void ProfilePanels::setReplayIndex(const int index) {
    replay().file = index >= 0 && std::cmp_less(index, _replays.size())
        ? _replays[static_cast<size_t>(index)]
        : std::string();

    pushReplay();
    _hub->profile().pushCommand();
}

void ProfilePanels::setReplayPlayback(const int value) {
    replay().playback = value;

    pushReplay();
    _hub->profile().pushCommand();
}

void ProfilePanels::setReplayComplevel(const int index) {
    replay().compatibility = complevelAt(
        Dialect::complevels(Dialect::demos(Dialect::of(config())).complevel), index);

    pushReplay();
    _hub->profile().pushCommand();
}

void ProfilePanels::setReplayLongtics(const bool value) {
    replay().longtics = value;

    pushReplay();
    _hub->profile().pushCommand();
}

void ProfilePanels::setReplaySoloNet(const bool value) {
    replay().soloNet = value;

    pushReplay();
    _hub->profile().pushCommand();
}

void ProfilePanels::refreshReplays() {
    _replaysRead = false;

    pushReplay();
}

void ProfilePanels::clearReplay() {
    replay() = ReplaySettings();

    pushReplay();
    _hub->profile().pushCommand();
    _hub->profile().pushCards();
}

// --- saves ---------------------------------------------------------------------

void ProfilePanels::setSaveOpen(const bool value) const {
    active().saveOpen = value;

    _hub->profile().push();
}

void ProfilePanels::setSaveEnabled(const bool value) {
    SaveSettings &picked = save();

    if (value == picked.enabled) {
        return;
    }

    picked.enabled = value;

    if (value) {
        // The last run may have written one.
        _savesRead = false;

        if (picked.file.empty()) {
            if (const std::vector<std::string> found = Storage::saves(config()); !found.empty()) {
                picked.file = found.front();
            }
        }
    }

    pushSave();
    _hub->profile().pushCommand();
}

void ProfilePanels::setSaveIndex(const int index) {
    save().file = index >= 0 && std::cmp_less(index, _saves.size())
        ? _saves[static_cast<size_t>(index)]
        : std::string();

    pushSave();
    _hub->profile().pushCommand();
}

void ProfilePanels::refreshSaves() {
    _savesRead = false;

    pushSave();
}
