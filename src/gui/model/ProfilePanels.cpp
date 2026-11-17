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
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ttk/util/Format.h"

#include "core/config/Options.h"
#include "core/config/Schema.h"
#include "core/launch/Arguments.h"
#include "core/launch/Dialect.h"
#include "core/launch/Flags.h"
#include "core/launch/Launcher.h"
#include "core/launch/Netgame.h"
#include "core/launch/PortSaves.h"
#include "core/launch/Storage.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/ProfilePanels.h"

using namespace ttk;

namespace {

// Not offered maps to 0, the port's own.
int complevelIndex(const std::span<const int> offered, const int level) {
    const auto found = std::ranges::find(offered, level);

    return found == offered.end() ? 0 : static_cast<int>(found - offered.begin());
}

int complevelAt(const std::span<const int> offered, const int index) {
    return index > 0 && std::cmp_less(index, offered.size()) ? offered[static_cast<size_t>(index)] : -1;
}

// 0 for the port's own, the mode after it, or -1 when the words match none.
int modeIndex(const Profile &profile, const std::span<const Flags::Mode> modes) {
    const std::string &first = profile.flag(Flags::Word::compatflags);
    const std::string &second = profile.flag(Flags::Word::compatflags2);

    if (first.empty() && second.empty()) {
        return 0;
    }

    // An empty word beside a typed one runs as the port has it, which a mode cannot promise.
    const std::optional<std::uint32_t> one = Flags::parse(first);
    const std::optional<std::uint32_t> two = Flags::parse(second);

    if (!one || !two) {
        return -1;
    }

    const auto found = std::ranges::find_if(
            modes, [&](const Flags::Mode &mode) { return mode.compatflags == *one && mode.compatflags2 == *two; });

    return found == modes.end() ? -1 : static_cast<int>(found - modes.begin()) + 1;
}

}

NetRole ProfilePanels::netRoleOf(const MultiplayerSettings &mp) {
    if (mp.gameType == GameType::None) {
        return NetRole::Alone;
    }

    return mp.players > 0 ? NetRole::Host : NetRole::Join;
}

void ProfilePanels::pushMultiplayer() {
    State::Cfg &state = cfg();
    const MultiplayerSettings &mp = multiplayer();

    state.netRole = netRoleOf(mp);
    state.gameType = mp.gameType;
    state.players = mp.players;
    state.host = mp.host;
    state.netPort = mp.port;
    state.fragLimit = mp.fragLimit;
    state.timeLimit = mp.timeLimit;
    state.extratic = mp.extratic;
    state.netmode = mp.netmode;
    state.dup = mp.dup;
    state.savegame = mp.savegame;
    state.listed = mp.listed;
    state.playerName = mp.playerName;
    state.playerColor = mp.playerColor;
    state.team = mp.team;
    state.multiplayerSet = mp != MultiplayerSettings();

    const Dialect::Port port = Dialect::of(config());

    state.features = port.features;
    state.netUnsupported = Netgame::unsupported(config().activeProfile(), port);
    state.netSaveByName = port.loadsBy(Dialect::SaveNames::name);
    state.netSaveExt = port.saveExt;
    state.netSaveFolder = Netgame::readsNetgameFolder(config().activeProfile(), port)
                                  ? Format::from_path(Storage::netgameSaveFolder(config()))
                                  : std::string();

    State::get().touch();
}

void ProfilePanels::pushGameplay() {
    const Profile &profile = active();
    const Dialect::Port port = Dialect::of(config());

    cfg().gameplayOpen = profile.on(Switch::gameplayOpen);
    cfg().features = port.features;

    pushFlagFields(profile);
    pushCompatibility(profile, port);
    pushOptionFields(profile, port);

    State::get().touch();
}

void ProfilePanels::pushFlagFields(const Profile &profile) {
    State::Cfg &state = cfg();

    for (const Flags::Word word : Flags::ALL) {
        State::FlagField &field = state.flagFields[Flags::index(word)];

        field.text = profile.flag(word);
        field.bad = !field.text.empty() && !Flags::parse(field.text);
    }
}

void ProfilePanels::pushCompatibility(const Profile &profile, const Dialect::Port &port) {
    State::Cfg &state = cfg();
    const Dialect::Compatibility kind = Dialect::compatibility(port);
    const std::span<const Flags::Mode> modes = Flags::modes(port.line);
    const std::span<const int> offered = Dialect::complevels(port.complevel);

    state.compatibilityIndex = kind == Dialect::Compatibility::modes ? modeIndex(profile, modes)
                                                                     : complevelIndex(offered, profile.complevel);

    // The lists hang on the port alone, so a keystroke in a field leaves them as they are.
    if (_compatibilityBuilt && _compatibilityLine == port.line && _compatibilityLevels == port.complevel) {
        return;
    }

    _compatibilityBuilt = true;
    _compatibilityLine = port.line;
    _compatibilityLevels = port.complevel;

    std::vector<std::string> names;
    std::vector<std::string> badges;

    state.compatibility = kind;

    if (kind == Dialect::Compatibility::modes) {
        names.reserve(modes.size() + 1);
        names.emplace_back(Dialect::levelName(-1));

        for (const Flags::Mode &mode : modes) {
            names.emplace_back(mode.label);
        }
    } else {
        names.reserve(offered.size());
        badges.reserve(offered.size());

        for (const int level : offered) {
            names.emplace_back(Dialect::levelName(level));
            badges.emplace_back(Dialect::complevelWord(port.complevel, level));
        }
    }

    state.compatibilityNames = std::move(names);
    state.compatibilityBadges = std::move(badges);
}

void ProfilePanels::pushOptionFields(const Profile &profile, const Dialect::Port &port) {
    for (std::size_t at = 0; at < Options::ALL.size(); ++at) {
        const Options::Option &option = Options::ALL[at];
        State::OptionField &field = cfg().optionFields[at];

        field.text = profile.number(option.feature);
        field.on = profile.checked(option);
        field.hint = Dialect::describe(port, option.feature);
        field.bad = false;

        if (!port.has(option.feature)) {
            continue;
        }

        const Dialect::Arg &arg = Dialect::arg(port, option.feature);

        field.least = arg.range.least;
        field.most = arg.range.most;
        field.bad = option.kind == Options::Kind::number && !field.text.empty() && !Dialect::parse(arg, field.text);
    }
}

void ProfilePanels::pushReplay() {
    State::Cfg &state = cfg();
    const ReplaySettings &demo = replay();
    const Dialect::Port port = Dialect::of(config());
    const std::filesystem::path folder = Storage::replayDirectory(config());
    const Arguments::Replay resolved = Arguments::replayOf(config(), port);
    const std::filesystem::path &file = resolved.file;

    state.replayOpen = active().on(Switch::replayOpen);
    state.replayMode = demo.mode;
    state.replayFile = demo.file;
    state.replayPlayback = demo.playback;
    state.replayLongtics = demo.longtics;
    state.replaySoloNet = demo.soloNet;
    state.replaySet = demo != ReplaySettings();
    state.features = port.features;

    state.replayHasComplevel = Dialect::replayComplevel(port) != Dialect::Complevels::none;
    state.complevelName = Dialect::levelLabel(port.complevel, active().complevel);
    state.complevelNeeded = Arguments::needsComplevel(active(), port);
    state.skillNeeded = Arguments::needsSkill(active(), port);

    if (!_replaysRead || _replaysFrom != folder.string()) {
        _replaysFrom = folder.string();
        _replaysRead = true;
        _replays = Storage::replays(config());
    }

    const auto at = std::ranges::find(_replays, file.filename().string());

    state.replayFolder = Format::from_path(folder);
    state.replayFiles = _replays;
    state.replayPath = Format::from_path(file);
    state.replayIndex =
            at == _replays.end() || file.parent_path() != folder ? -1 : static_cast<int>(at - _replays.begin());

    const std::filesystem::path &source = resolved.source;
    const auto from = std::ranges::find(_replays, source.filename().string());

    state.replaySource = demo.source;
    state.replaySourcePath = Format::from_path(source);
    state.replaySourceIndex =
            from == _replays.end() || source.parent_path() != folder ? -1 : static_cast<int>(from - _replays.begin());

    std::error_code asked;
    const bool recording = demo.mode == ReplayMode::Record || demo.mode == ReplayMode::Continue;

    state.replayNameTaken = recording && !file.empty() && std::filesystem::exists(file, asked);
    state.replayTrouble = Storage::replayTrouble(config());
    state.replayLeftOut = resolved.leftOut;

    State::get().touch();
}

void ProfilePanels::pushSave() {
    State::Cfg &state = cfg();
    const Dialect::Port port = Dialect::of(config());
    const std::filesystem::path folder = Storage::saveFolder(config());

    state.saveLoads = port.has(Dialect::Feature::loadgame) && port.has(Dialect::Feature::saveFolder);
    state.saveSlots = port.loadsBy(Dialect::SaveNames::slot);

    // Another port, release or game lists other saves.
    const PortEntry none;
    const PortEntry *entry = config().findPort(config().activeProfile().port);
    const PortEntry &asked = entry == nullptr ? none : *entry;
    const std::string from =
            folder.string() + '\n' + Launcher::executable(config()).string() + '\n' + config().activeIwadFile();

    if (!_savesRead || _savesFrom != from || _savesAnswered != Dialect::answerKey(asked)
        || _savesProfile != config().activeProfileId) {
        // The same profile moved to other saves, so its pick named a save in the folder it left.
        const bool moved = _savesRead && _savesProfile == config().activeProfileId;

        _savesFrom = from;
        _savesAnswered = Dialect::answerKey(asked);
        _savesProfile = config().activeProfileId;
        _savesRead = true;
        _saves = Storage::saves(config());
        _saveSlots.clear();
        _saveSlots.reserve(_saves.size());

        for (const std::string &name : _saves) {
            _saveSlots.emplace_back(PortSaves::slotLabel(config(), port, name));
        }

        // A full path names its save wherever the list comes from.
        if (std::string &picked = save().file; moved && !std::filesystem::path(picked).is_absolute()
                                               && std::ranges::find(_saves, picked) == _saves.end()) {
            std::string next = save().enabled && !_saves.empty() ? _saves.front() : std::string();

            if (next != picked) {
                picked = std::move(next);

                _hub->scheduleSave();
            }
        }
    }

    const std::filesystem::path file = Storage::saveFile(config());

    state.saveOpen = active().on(Switch::saveOpen);
    state.saveEnabled = save().enabled;
    state.saveFile = save().file;

    const auto at = std::ranges::find(_saves, file.lexically_relative(folder).generic_string());

    state.saveFolder = Format::from_path(folder);
    state.saveFiles = _saves;
    state.saveSlotLabels = _saveSlots;
    state.savePath = Format::from_path(file);
    state.saveIndex = at == _saves.end() || folder.empty() ? -1 : static_cast<int>(at - _saves.begin());
    state.savesFromPort = Storage::configFile(config()).empty();
    state.saveTrouble = Storage::saveTrouble(config());

    State::get().touch();
}

void ProfilePanels::setMultiplayerOpen(const bool value) const {
    active().set(Switch::dialogOpen, value);

    _hub->scheduleSave();
    _hub->profile().push();
}

void ProfilePanels::setNetRole(const NetRole value) const {
    MultiplayerSettings &mp = multiplayer();

    if (value == netRoleOf(mp)) {
        return;
    }

    if (value == NetRole::Alone) {
        mp.gameType = GameType::None;
    } else {
        if (mp.gameType == GameType::None) {
            mp.gameType = GameType::Coop;
        }

        mp.players = value == NetRole::Host ? std::max(mp.players, 2) : 0;
    }

    _hub->scheduleSave();
    pushMultiplayer();
    _hub->profile().pushCommand();
    _hub->profile().pushCards();
}

void ProfilePanels::setGameType(const GameType value) const {
    multiplayer().gameType = value;

    _hub->scheduleSave();
    pushMultiplayer();
    _hub->profile().pushCommand();
    _hub->profile().pushCards();
}

void ProfilePanels::setPlayers(const int value) const {
    multiplayer().players = value;

    _hub->scheduleSave();
    pushMultiplayer();
    _hub->profile().pushCommand();
    _hub->profile().pushCards();
}

void ProfilePanels::setHost(const std::string &value) const {
    multiplayer().host = value;

    _hub->scheduleSave();
    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setNetPort(const std::string &value) const {
    multiplayer().port = value;

    _hub->scheduleSave();
    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setFragLimit(const std::string &value) const {
    multiplayer().fragLimit = value;

    _hub->scheduleSave();
    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setTimeLimit(const std::string &value) const {
    multiplayer().timeLimit = value;

    _hub->scheduleSave();
    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setExtratic(const bool value) const {
    multiplayer().extratic = value;

    _hub->scheduleSave();
    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setNetmode(const int value) const {
    multiplayer().netmode = value;

    _hub->scheduleSave();
    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setDup(const int value) const {
    multiplayer().dup = value;

    _hub->scheduleSave();
    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setListed(const bool value) const {
    multiplayer().listed = value;

    _hub->scheduleSave();
    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setSavegame(const std::string &value) const {
    multiplayer().savegame = value;

    _hub->scheduleSave();
    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setPlayerName(const std::string &value) const {
    multiplayer().playerName = value;

    _hub->scheduleSave();
    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setPlayerColor(const std::string &value) const {
    multiplayer().playerColor = value;

    _hub->scheduleSave();
    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::setTeam(const int value) const {
    multiplayer().team = value < 0 ? -1 : value;

    _hub->scheduleSave();
    pushMultiplayer();
    _hub->profile().pushCommand();
}

void ProfilePanels::clearMultiplayer() const {
    multiplayer() = MultiplayerSettings();

    _hub->scheduleSave();
    pushMultiplayer();
    _hub->profile().pushCommand();
    _hub->profile().pushCards();
}

void ProfilePanels::setGeneralOpen(const bool value) const {
    active().set(Switch::generalOpen, value);

    _hub->scheduleSave();
    _hub->profile().push();
}

void ProfilePanels::setReplayOpen(const bool value) const {
    active().set(Switch::replayOpen, value);

    _hub->scheduleSave();
    _hub->profile().push();
}

void ProfilePanels::setReplayMode(const ReplayMode value) {
    ReplaySettings &demo = replay();

    if (value == demo.mode) {
        return;
    }

    const ReplayMode was = demo.mode;

    demo.mode = value;

    // Play and Continue both pick a demo to play. The new demo Continue records needs a name of its own.
    std::error_code asked;

    if (was == ReplayMode::Play && value == ReplayMode::Continue
        && std::filesystem::is_regular_file(Storage::replayFile(config()), asked)) {
        demo.source = std::exchange(demo.file, std::string());
    } else if (was == ReplayMode::Continue && value == ReplayMode::Play && !demo.source.empty()
               && !std::filesystem::is_regular_file(Storage::replayFile(config()), asked)) {
        demo.file = demo.source;
    }

    if (value == ReplayMode::Play || value == ReplayMode::Continue) {
        // The last run may have written one.
        _replaysRead = false;

        std::string &picked = value == ReplayMode::Play ? demo.file : demo.source;

        if (picked.empty()) {
            if (const std::vector<std::string> found = Storage::replays(config()); !found.empty()) {
                picked = found.front();
            }
        }
    }

    _hub->scheduleSave();
    pushReplay();
    _hub->profile().pushCommand();
    _hub->profile().pushCards();
}

void ProfilePanels::setReplayFile(const std::string &value) {
    replay().file = value;

    _hub->scheduleSave();
    pushReplay();
    _hub->profile().pushCommand();
}

void ProfilePanels::setReplayIndex(const int index) {
    replay().file =
            index >= 0 && std::cmp_less(index, _replays.size()) ? _replays[static_cast<size_t>(index)] : std::string();

    _hub->scheduleSave();
    pushReplay();
    _hub->profile().pushCommand();
}

void ProfilePanels::setReplaySource(const std::string &value) {
    replay().source = value;

    _hub->scheduleSave();
    pushReplay();
    _hub->profile().pushCommand();
}

void ProfilePanels::setReplaySourceIndex(const int index) {
    replay().source =
            index >= 0 && std::cmp_less(index, _replays.size()) ? _replays[static_cast<size_t>(index)] : std::string();

    _hub->scheduleSave();
    pushReplay();
    _hub->profile().pushCommand();
}

void ProfilePanels::setReplayPlayback(const Playback value) {
    replay().playback = value;

    _hub->scheduleSave();
    pushReplay();
    _hub->profile().pushCommand();
}

void ProfilePanels::setGameplayOpen(const bool value) {
    active().set(Switch::gameplayOpen, value);

    _hub->scheduleSave();
    pushGameplay();
}

void ProfilePanels::setCompatibility(const int index) {
    const Dialect::Port port = Dialect::of(config());
    const std::span<const Flags::Mode> modes = Flags::modes(port.line);
    Profile &profile = active();

    if (Dialect::compatibility(port) != Dialect::Compatibility::modes) {
        profile.complevel = complevelAt(Dialect::complevels(port.complevel), index);
    } else if (index > 0 && std::cmp_less_equal(index, modes.size())) {
        const Flags::Mode &mode = modes[static_cast<size_t>(index) - 1];

        profile.setFlag(Flags::Word::compatflags, std::to_string(mode.compatflags));
        profile.setFlag(Flags::Word::compatflags2, std::to_string(mode.compatflags2));
    } else {
        profile.setFlag(Flags::Word::compatflags, {});
        profile.setFlag(Flags::Word::compatflags2, {});
    }

    _hub->scheduleSave();
    pushGameplay();
    pushReplay();
    _hub->profile().pushCommand();
}

void ProfilePanels::setNumber(const Dialect::Feature option, const std::string &value) {
    active().setNumber(option, value);

    _hub->scheduleSave();
    pushGameplay();
    _hub->profile().pushCommand();
}

void ProfilePanels::setOption(const Dialect::Feature option, const bool on) {
    active().setChecked(Options::ALL[Options::indexOf(option)], on);

    _hub->scheduleSave();
    pushGameplay();
    _hub->profile().pushCommand();
}

void ProfilePanels::setFlag(const Flags::Word word, const std::string &value) {
    active().setFlag(word, value);

    _hub->scheduleSave();
    pushGameplay();
    _hub->profile().pushCommand();
}

void ProfilePanels::setFlags(const Flags::Edits &edits) {
    for (const auto &[word, value] : edits) {
        active().setFlag(word, value);
    }

    _hub->scheduleSave();
    pushGameplay();
    _hub->profile().pushCommand();
}

void ProfilePanels::setReplayLongtics(const bool value) {
    replay().longtics = value;

    _hub->scheduleSave();
    pushReplay();
    _hub->profile().pushCommand();
}

void ProfilePanels::setReplaySoloNet(const bool value) {
    replay().soloNet = value;

    _hub->scheduleSave();
    pushReplay();
    _hub->profile().pushCommand();
}

void ProfilePanels::refreshReplays() {
    _replaysRead = false;

    pushReplay();
}

void ProfilePanels::clearReplay() {
    replay() = ReplaySettings();

    _hub->scheduleSave();
    pushReplay();
    _hub->profile().pushCommand();
    _hub->profile().pushCards();
}

void ProfilePanels::setSaveOpen(const bool value) const {
    active().set(Switch::saveOpen, value);

    _hub->scheduleSave();
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

    _hub->scheduleSave();
    pushSave();
    _hub->profile().pushCommand();
}

void ProfilePanels::setSaveIndex(const int index) {
    save().file =
            index >= 0 && std::cmp_less(index, _saves.size()) ? _saves[static_cast<size_t>(index)] : std::string();

    _hub->scheduleSave();
    pushSave();
    _hub->profile().pushCommand();
}

void ProfilePanels::refreshSaves() {
    _savesRead = false;

    pushSave();
}
