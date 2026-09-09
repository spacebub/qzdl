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
#include <chrono>
#include <map>
#include <utility>

#include "core/Catalog.h"
#include "core/Detect.h"
#include "core/FileInfo.h"
#include "core/Import.h"
#include "core/Launcher.h"
#include "core/Paths.h"
#include "core/Session.h"
#include "core/Text.h"
#include "gui/ConfigBridge.h"

#include "Models.h"
#include "gui/Convert.h"

namespace {

Config &config() {
    return Session::get().config();
}

Profile &profile() {
    return config().activeProfile();
}

MultiplayerSettings &multiplayer() {
    return profile().multiplayer;
}

ReplaySettings &replay() {
    return profile().replay;
}

SaveSettings &save() {
    return profile().save;
}

/*
What each compatibility level is. The number is what goes on the command line;
the interface only ever sees a place in the list its port was offered, so
nothing in .slint has to know Doom's version history.
*/
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

// Where the level sits in what this port was offered; one it was not is the
// port's own, which is what the launch makes of it too.
int complevelIndex(const std::vector<int> &offered, const int level) {
    const auto found = std::ranges::find(offered, level);

    return found == offered.end() ? 0 : static_cast<int>(found - offered.begin());
}

int complevelAt(const std::vector<int> &offered, const int index) {
    return index > 0 && std::cmp_less(index, offered.size())
        ? offered[static_cast<size_t>(index)]
        : -1;
}

// 0 plays alone, 1 hosts, 2 joins. A player count is what makes it a host.
int netRoleOf(const MultiplayerSettings &mp) {
    if (mp.gameType == 0) {
        return 0;
    }

    return mp.players > 0 ? 1 : 2;
}

/*
What a card's picture is worked out from: the game, then the add-ons the port is
actually handed, in the order it gets them. The last of those carrying a title
screen is the one it would draw, so the whole list is what the answer hangs on.
*/
std::string artKeyOf(const Profile &each, const NameEntry *game) {
    std::string key = game == nullptr ? std::string() : game->file;

    for (const FileEntry &file : each.files) {
        if (file.enabled) {
            key += '\n';
            key += file.file;
        }
    }

    return key;
}

bool dosPortOf(const Profile &profile) {
    const NameEntry *port = config().findPort(profile.port);

    return port != nullptr && port->dosbox;
}

// The one the shelf was set to, or the open profile's while it has not been.
// A name the port list no longer has counts as unset.
std::string gamePortName() {
    const std::string &chosen = config().general.gamePort;

    return !chosen.empty() && config().findPort(chosen) != nullptr ? chosen : profile().port;
}

// Built rather than copied from the open config: a copy carries every other
// profile's file list along, once per card. The open profile's id and name come
// with it, since what a DOS launch stages hangs off them.
Config oneGame(const std::string &iwad) {
    Config made;
    Profile target;

    made.general = config().general;
    made.iwads = config().iwads;
    made.ports = config().ports;

    // A shelf with no profile open still launches, so what a DOS run stages
    // under the profile's id has a name of its own to go under.
    target.id = profile().id.empty() ? "shelf" : profile().id;
    target.name = profile().name;
    target.config = profile().config;
    target.port = gamePortName();
    target.iwad = iwad;
    target.sharedConfig = true;

    made.activeProfileId = target.id;
    made.profiles.push_back(std::move(target));

    return made;
}

std::string profileKeyOf(const std::string &id) {
    return "profile:" + id;
}

std::string gameKeyOf(const std::string &iwad) {
    return "game:" + iwad;
}

// Remembered for a moment: one change to a list asks this of every game and
// add-on, and a stat is not free on a network share or a sleeping disk.
bool missing(const std::filesystem::path &path) {
    using Clock = std::chrono::steady_clock;

    struct Known {
        Clock::time_point asked;
        bool gone = false;
    };

    static constexpr std::chrono::seconds FRESH{2};
    static std::map<std::filesystem::path, Known> seen;

    const Clock::time_point now = Clock::now();

    if (const auto found = seen.find(path);
        found != seen.end() && now - found->second.asked < FRESH) {
        return found->second.gone;
    }

    std::error_code code;
    const bool gone = !std::filesystem::exists(path, code);

    seen[path] = Known{.asked = now, .gone = gone};

    return gone;
}

/*
A game has to be a file. A source port looks for one with -iwad, which takes a
name with an archive's extension on it and never a folder, so a folder named as
a game is one that could only fail at launch.
*/
bool folder(const std::filesystem::path &path) {
    std::error_code code;

    return std::filesystem::is_directory(path, code);
}

bool contains(const std::string &value, const std::string &needle) {
    return Text::lower(value).contains(Text::lower(needle));
}

// The profile's name with anything a file system would refuse taken out.
std::string zdlFileName(const std::string &name) {
    static constexpr std::string_view FORBIDDEN = R"(/\:*?"<>|)";
    std::string stem;

    for (const char each : name) {
        stem.push_back(static_cast<unsigned char>(each) < 0x20 || FORBIDDEN.contains(each)
                       ? '-'
                       : each);
    }

    stem = Text::trim(stem);

    // Windows drops trailing dots and spaces, so the saved name would not match.
    while (!stem.empty() && (stem.back() == '.' || stem.back() == ' ')) {
        stem.pop_back();
    }

    return (stem.empty() ? "profile" : stem) + ".zdl";
}

}

ConfigBridge::ConfigBridge(const ui::Zdl *window, Notifier *notifier, Runs *runs)
    : _window(window), _notifier(notifier), _runs(runs) {
    const auto &cfg = _window->global<ui::Cfg>();

    // Handed over once; everything after this changes what is in them.
    cfg.set_profile_cards(_profileCards);
    cfg.set_shelf_profiles(_shelfProfiles);
    cfg.set_shelf_games(_shelfGames);
    cfg.set_files(_files);
    cfg.set_iwads(_iwads);
    cfg.set_ports(_ports);

    bind();
    reload();
}

const std::vector<NameEntry> &ConfigBridge::ports() {
    return config().ports;
}

void ConfigBridge::scheduleSave() {
    _pendingSave = true;

    // Nothing here has a Save beside it. A moment rather than at once: a field
    // being typed into changes on every key, and each of those is a whole file.
    _autosave.start(slint::TimerMode::SingleShot, AUTOSAVE, [this] { flush(); });
}

void ConfigBridge::flush() {
    if (!_pendingSave) {
        return;
    }

    _autosave.stop();
    _pendingSave = false;

    std::string error;

    // Said once: a config that cannot be written fails on every keystroke after.
    if (Session::get().save(&error)) {
        _warnedSave = false;

        return;
    }

    if (!_warnedSave) {
        _warnedSave = true;
        _notifier->error("Could not save the config: " + error);
    }
}

void ConfigBridge::touch() {
    pushMaps();
    pushCommand();
    pushProfiles();
}

void ConfigBridge::reload() {
    pushLists();
    pushProfiles();
    pushProfile();
    pushMultiplayer();
    pushGeneral();
    pushPath();
    touch();
}

ui::ProfileCard ConfigBridge::cardOf(const int index) {
    const Profile &each = config().profiles[static_cast<size_t>(index)];
    int loaded = 0;

    for (const FileEntry &file : each.files) {
        if (file.enabled) {
            ++loaded;
        }
    }

    const NameEntry *game = config().findIwad(each.iwad);

    return ui::ProfileCard{
        .index = index,
        .id = Convert::text(each.id),
        .key = Convert::text(profileKeyOf(each.id)),
        .name = Convert::text(each.name.empty() ? "(unnamed)" : each.name),
        .iwad = Convert::text(each.iwad),
        .art_key = Convert::text(artKeyOf(each, game)),
        .port = Convert::text(each.port),
        .dos_port = dosPortOf(each),
        .warp = Convert::text(each.warp),
        .files = static_cast<int>(each.files.size()),
        .loaded = loaded,
        .net_role = netRoleOf(each.multiplayer),

        // One that writes its own command needs no port to run.
        .ready = !each.port.empty() || each.customCommand,
    };
}

ui::NameRow ConfigBridge::rowOf(const std::vector<NameEntry> &list, const int index,
                               const bool ports) {
    const NameEntry &entry = list[static_cast<size_t>(index)];
    const std::filesystem::path path(entry.file);
    const std::string root = Convert::plain(Convert::fromPath(Catalog::directory()));

    return ui::NameRow{
        .index = index,
        .name = Convert::text(entry.name),
        .file = Convert::text(entry.file),
        .directory = Convert::fromPath(path.parent_path()),
        .kind = Convert::text(Text::lower(path.extension().string())),
        .missing = missing(path),
        .dosbox = entry.dosbox,
        .fetched = ports && !root.empty()
            && Convert::plain(Convert::fromPath(path)).starts_with(root + "/"),
        .detected = ports && Detect::of(path) != nullptr,
    };
}

std::string ConfigBridge::uniqueName(const std::vector<NameEntry> &list, const std::string &base,
                                     const int ignoring) {
    std::string candidate = Text::trim(base);

    if (candidate.empty()) {
        candidate = "Unnamed";
    }

    const auto taken = [&list, ignoring](const std::string &name) {
        for (size_t index = 0; index < list.size(); ++index) {
            if (std::cmp_not_equal(index, ignoring) && Text::iequals(list[index].name, name)) {
                return true;
            }
        }

        return false;
    };

    if (!taken(candidate)) {
        return candidate;
    }

    for (int suffix = 2;; suffix++) {
        if (std::string numbered = candidate + " (" + std::to_string(suffix) + ")";
            !taken(numbered)) {
            return numbered;
        }
    }
}

void ConfigBridge::pushProfiles() {
    const auto &cfg = _window->global<ui::Cfg>();
    std::vector<ui::ProfileCard> cards;

    cards.reserve(config().profiles.size());

    for (size_t index = 0; index < config().profiles.size(); ++index) {
        cards.push_back(cardOf(static_cast<int>(index)));
    }

    Models::reconcile(*_profileCards, cards);

    cfg.set_rev(++_rev);

    pushShelf();
    scheduleSave();
}

void ConfigBridge::pushShelf() {
    std::vector<ui::ProfileCard> profiles;
    std::vector<ui::NameRow> games;

    for (size_t index = 0; index < config().profiles.size(); ++index) {
        if (_filter.empty() || contains(config().profiles[index].name, _filter)) {
            profiles.push_back(cardOf(static_cast<int>(index)));
        }
    }

    for (size_t index = 0; index < config().iwads.size(); ++index) {
        if (_filter.empty() || contains(config().iwads[index].name, _filter)) {
            games.push_back(rowOf(config().iwads, static_cast<int>(index), false));
        }
    }

    Models::reconcile(*_shelfProfiles, profiles);
    Models::reconcile(*_shelfGames, games);
}

void ConfigBridge::pushLists() {
    const auto &cfg = _window->global<ui::Cfg>();
    const Profile &active = profile();

    std::vector<ui::FileRow> files;
    int enabled = 0;

    files.reserve(active.files.size());

    for (size_t index = 0; index < active.files.size(); ++index) {
        const FileEntry &entry = active.files[index];
        const std::filesystem::path path(entry.file);

        if (entry.enabled) {
            ++enabled;
        }

        files.push_back(ui::FileRow{
            .index = static_cast<int>(index),
            .file = Convert::text(entry.file),
            .name = Convert::text(path.filename().string()),
            .directory = Convert::fromPath(path.parent_path()),
            .loaded = entry.enabled,
            .missing = missing(path),
        });
    }

    Models::reconcile(*_files, files);
    cfg.set_enabled_count(enabled);

    std::vector<ui::NameRow> iwads;
    std::vector<ui::NameRow> ports;
    std::vector<std::string> iwadNames;
    std::vector<std::string> portNames;
    std::vector<std::string> portBadges;

    for (size_t index = 0; index < config().iwads.size(); ++index) {
        iwads.push_back(rowOf(config().iwads, static_cast<int>(index), false));
        iwadNames.push_back(config().iwads[index].name);
    }

    for (size_t index = 0; index < config().ports.size(); ++index) {
        ports.push_back(rowOf(config().ports, static_cast<int>(index), true));
        portNames.push_back(config().ports[index].name);
        portBadges.emplace_back(config().ports[index].dosbox ? "DOS" : "");
    }

    Models::reconcile(*_iwads, iwads);
    Models::reconcile(*_ports, ports);
    cfg.set_iwad_names(Convert::strings(iwadNames));
    cfg.set_port_names(Convert::strings(portNames));
    cfg.set_port_badges(Convert::strings(portBadges));

    pushShelf();
    pushGameRev();
}

void ConfigBridge::pushProfile() {
    const auto &cfg = _window->global<ui::Cfg>();
    const Profile &active = profile();

    cfg.set_profile_index(config().activeProfileIndex());
    cfg.set_profile_name(Convert::text(active.name));
    cfg.set_profile_key(Convert::text(profileKeyOf(config().activeProfileId)));
    cfg.set_iwad(Convert::text(active.iwad));
    cfg.set_port(Convert::text(active.port));
    cfg.set_skill(active.skill);
    cfg.set_monsters(active.monsters);
    cfg.set_warp(Convert::text(active.warp));
    cfg.set_extra(Convert::text(active.extra));
    cfg.set_multiplayer_open(active.dialogOpen);
    cfg.set_shared_config(active.sharedConfig);
    cfg.set_command_override(active.customCommand);
    cfg.set_command(Convert::text(active.command));
    cfg.set_dos_fullscreen(active.dosFullscreen);
    cfg.set_capture_output(active.captureOutput);
    cfg.set_config_file(Convert::fromPath(Launcher::getConfigPath(active)));
    cfg.set_dos_port(Launcher::isDosPort(config()));
    cfg.set_rev(++_rev);

    // The port decides what can be said about a demo, and the folder they are
    // kept in is the profile's own, so both follow the profile. The same goes
    // for the saves beside them.
    pushReplay();
    pushSave();
    pushGameRev();
    scheduleSave();
}

void ConfigBridge::pushMultiplayer() {
    const auto &cfg = _window->global<ui::Cfg>();
    const MultiplayerSettings &mp = multiplayer();

    cfg.set_net_role(netRoleOf(mp));
    cfg.set_game_type(mp.gameType);
    cfg.set_players(mp.players);
    cfg.set_host(Convert::text(mp.host));
    cfg.set_net_port(Convert::text(mp.port));
    cfg.set_frag_limit(Convert::text(mp.fragLimit));
    cfg.set_time_limit(Convert::text(mp.timeLimit));
    cfg.set_dmflags(Convert::text(mp.dmflags));
    cfg.set_dmflags2(Convert::text(mp.dmflags2));
    cfg.set_extratic(mp.extratic);
    cfg.set_netmode(mp.netmode);
    cfg.set_dup(mp.dup);
    cfg.set_savegame(Convert::text(mp.savegame));
    cfg.set_multiplayer_set(mp != MultiplayerSettings());

    scheduleSave();
}

void ConfigBridge::pushReplay() {
    const auto &cfg = _window->global<ui::Cfg>();
    const ReplaySettings &demo = replay();
    const Launcher::DemoSupport speaks = Launcher::demoSupport(config());
    const std::filesystem::path folder = Launcher::getReplayPath(config());
    const std::filesystem::path file = Launcher::replayFile(config());

    cfg.set_replay_open(profile().replayOpen);
    cfg.set_replay_mode(demo.mode);
    cfg.set_replay_file(Convert::text(demo.file));
    cfg.set_replay_playback(demo.playback);
    cfg.set_replay_longtics(demo.longtics);
    cfg.set_replay_solo_net(demo.soloNet);
    cfg.set_replay_set(demo != ReplaySettings());

    cfg.set_replay_records(speaks.records);
    cfg.set_replay_timed(speaks.timed);
    cfg.set_replay_fast(speaks.fast);
    cfg.set_replay_has_complevel(speaks.complevel != Launcher::Complevels::none);

    // Only what this port reads. The number rides beside the name.
    const std::vector<int> offered = Launcher::complevels(speaks.complevel);
    std::vector<std::string> complevels;
    std::vector<std::string> numbers;
    complevels.reserve(offered.size());
    numbers.reserve(offered.size());

    for (const int level : offered) {
        complevels.emplace_back(complevelName(level));
        numbers.emplace_back(level < 0 ? "" : std::to_string(level));
    }

    cfg.set_replay_complevels(Convert::strings(complevels));
    cfg.set_replay_complevel_numbers(Convert::strings(numbers));
    cfg.set_replay_complevel(complevelIndex(offered, demo.compatibility));
    cfg.set_replay_has_longtics(speaks.longtics);
    cfg.set_replay_has_solo_net(speaks.soloNet);

    if (!_replaysRead || _replaysFrom != folder.string()) {
        _replaysFrom = folder.string();
        _replaysRead = true;
        _replays = Launcher::replays(config());
    }

    const auto at = std::ranges::find(_replays, file.filename().string());

    cfg.set_replay_folder(Convert::fromPath(folder));
    cfg.set_replay_files(Convert::strings(_replays));
    cfg.set_replay_path(Convert::fromPath(file));
    cfg.set_replay_index(at == _replays.end() || file.parent_path() != folder
                         ? -1
                         : static_cast<int>(at - _replays.begin()));

    // Said before the launch rather than after it: a demo is written over
    // without a word by every port there is.
    std::error_code asked;

    cfg.set_replay_overwrites(demo.mode == 1 && !file.empty()
                              && std::filesystem::exists(file, asked));
    cfg.set_replay_trouble(Convert::text(Launcher::replayTrouble(config())));

    scheduleSave();
}

void ConfigBridge::pushSave() {
    const auto &cfg = _window->global<ui::Cfg>();
    const Launcher::SaveSupport speaks = Launcher::saveSupport(config());
    const std::filesystem::path folder = Launcher::saveFolder(config());
    const std::filesystem::path file = Launcher::saveFile(config());

    cfg.set_save_open(profile().saveOpen);
    cfg.set_save_enabled(save().enabled);
    cfg.set_save_file(Convert::text(save().file));

    // Both halves: a port that cannot be told which save to load has nothing to
    // offer here, and neither has one whose saves are its own business.
    cfg.set_save_loads(speaks.names != Launcher::SaveNames::none && speaks.folder);
    cfg.set_save_slots(speaks.names == Launcher::SaveNames::slot);

    // The folder is the profile's, but what counts as a save in it is the
    // port's, so a port change reads it again.
    const std::string from = folder.string() + '\n' + Launcher::executable(config()).string();

    if (!_savesRead || _savesFrom != from) {
        _savesFrom = from;
        _savesRead = true;
        _saves = Launcher::saves(config());
    }

    std::vector<std::string> slots;
    slots.reserve(_saves.size());

    for (const std::string &name : _saves) {
        const int slot = speaks.names == Launcher::SaveNames::slot
            ? Launcher::saveSlot(name)
            : -1;

        slots.emplace_back(slot < 0 ? std::string() : "Slot " + std::to_string(slot));
    }

    const auto at = std::ranges::find(_saves, file.filename().string());

    cfg.set_save_folder(Convert::fromPath(folder));
    cfg.set_save_files(Convert::strings(_saves));
    cfg.set_save_slot_labels(Convert::strings(slots));
    cfg.set_save_path(Convert::fromPath(file));
    cfg.set_save_index(at == _saves.end() || file.parent_path() != folder
                       ? -1
                       : static_cast<int>(at - _saves.begin()));
    cfg.set_save_trouble(Convert::text(Launcher::saveTrouble(config())));

    scheduleSave();
}

void ConfigBridge::pushGeneral() {
    const auto &cfg = _window->global<ui::Cfg>();
    const GeneralSettings &general = config().general;

    cfg.set_game_port(Convert::text(general.gamePort));
    cfg.set_always_add(Convert::text(general.alwaysAdd));
    cfg.set_dosbox(Convert::text(general.dosbox));
    cfg.set_system_dosbox(Convert::fromPath(Detect::dosbox()));
    cfg.set_auto_close(general.autoClose);
    cfg.set_launch_zdl_immediately(general.launchZdlImmediately);
    cfg.set_show_paths(general.showPaths);
    cfg.set_start_view(Convert::text(general.startView == "games" ? "games" : "profiles"));
    cfg.set_profile_configs(general.profileConfigs);
    cfg.set_ignore_user_config(Session::get().userConfigIgnored());

    pushGameRev();
    scheduleSave();
}

// Opens the game and every file ticked on top of it, so it is only redone when
// one of those changed; nothing else touch() sees can alter the answer.
void ConfigBridge::pushMaps() {
    const Profile &active = profile();
    const NameEntry *game = config().findIwad(active.iwad);
    std::string mark = game == nullptr ? std::string() : game->file;

    for (const FileEntry &entry : active.files) {
        if (entry.enabled) {
            mark += '\n';
            mark += entry.file;
        }
    }

    if (_mapsKnown && mark == _mapsMark) {
        return;
    }

    _maps = Launcher::maps(config());
    _mapsMark = std::move(mark);
    _mapsKnown = true;

    _window->global<ui::Cfg>().set_maps(Convert::strings(_maps));
}

// A library launch depends on this much and no more, so the key only moves when
// one of them does and a keystroke elsewhere does not redo the shelf.
void ConfigBridge::pushGameRev() {
    std::string mark = config().general.gamePort + '\n' + config().general.alwaysAdd + '\n'
        + config().general.dosbox + '\n' + profile().port + '\n' + profile().id;

    for (const NameEntry &port : config().ports) {
        mark += '\n' + port.name + '\t' + port.file + (port.dosbox ? "\tdos" : "");
    }

    for (const NameEntry &game : config().iwads) {
        mark += '\n' + game.name + '\t' + game.file;
    }

    if (mark == _gameMark) {
        return;
    }

    _gameMark = std::move(mark);
    _window->global<ui::Cfg>().set_game_rev(++_gameRev);
}

// Building the line opens the game and, for a DOS port, walks its directory. It
// is only ever read, so it waits a moment rather than running once a keystroke.
void ConfigBridge::pushCommand() {
    _window->global<ui::Cfg>().set_rev(++_rev);
    _preview.start(slint::TimerMode::SingleShot, PREVIEW, [this] { showCommand(); });

    scheduleSave();
}

void ConfigBridge::showCommand() {
    const auto &cfg = _window->global<ui::Cfg>();

    _preview.stop();

    cfg.set_command_line(Convert::text(Launcher::commandLine(config())));
    cfg.set_command_trouble(Convert::text(Launcher::commandTrouble(config())));
}

void ConfigBridge::pushPath() {
    const auto &cfg = _window->global<ui::Cfg>();

    cfg.set_path(Convert::fromPath(Session::get().path()));
    cfg.set_user_config(Session::get().path() == Paths::get().configPath(Paths::USER));
}

bool ConfigBridge::start(const std::string &key, const std::string &title, const Config &what) {
    std::string error;
    Process::Id started = 0;

    // Whoever takes the output has to read it to the end, so it is only taken
    // when the profile asks for it.
    Process::Stream output = Process::NOTHING;

    // A DOS port prints into DOSBox's own window, and closing on launch takes the
    // log away: either leaves a pipe nobody drains.
    const bool capture = what.activeProfile().captureOutput
        && !Launcher::isDosPort(what)
        && !what.general.autoClose;
    const std::string line = Launcher::commandLine(what);

    if (!Launcher::launch(what, &started, capture ? &output : nullptr, &error)) {
        _notifier->error(error, "Nothing was launched");
        _runs->refused(key, title, error);

        return false;
    }

    _runs->began(key, title, line, started, output);

    if (launched) {
        launched();
    }

    return true;
}

void ConfigBridge::renamedIwad(const std::string &before, const std::string &after) {
    for (Profile &each : config().profiles) {
        if (each.iwad == before) {
            each.iwad = after;
        }
    }

    pushProfile();
    touch();
}

void ConfigBridge::renamedPort(const std::string &before, const std::string &after) {
    for (Profile &each : config().profiles) {
        if (each.port == before) {
            each.port = after;
        }
    }

    if (config().general.gamePort == before) {
        config().general.gamePort = after;

        pushGeneral();
    }

    pushProfile();
    touch();
}

std::string ConfigBridge::addPort(const std::string &file, const std::string &name,
                                  const bool dosbox) {
    if (file.empty()) {
        return {};
    }

    const std::string chosen = uniqueName(config().ports,
                                          name.empty() ? FileInfo::describePort(file) : name);

    config().ports.push_back(NameEntry{.name = chosen, .file = file, .dosbox = dosbox});

    pushLists();
    pushProfile();
    pushCommand();

    return chosen;
}

void ConfigBridge::updatePort(const int row, const std::string &name, const std::string &file,
                              const bool dosbox) {
    std::vector<NameEntry> &list = config().ports;

    if (row < 0 || std::cmp_greater_equal(row, list.size())) {
        return;
    }

    NameEntry &entry = list[static_cast<size_t>(row)];
    const std::string before = entry.name;
    const std::string after = uniqueName(list,
                                         name.empty() ? FileInfo::describePort(file) : name, row);

    entry.name = after;
    entry.file = file;
    entry.dosbox = dosbox;

    pushLists();

    // Profiles point at entries by name, so a rename has to be carried across.
    if (before != after) {
        renamedPort(before, after);
    }

    pushProfile();
    pushCommand();
}

void ConfigBridge::removePort(const int row) {
    std::vector<NameEntry> &list = config().ports;

    if (row < 0 || std::cmp_greater_equal(row, list.size())) {
        return;
    }

    list.erase(list.begin() + row);

    // A port since removed is the same as never having pointed it anywhere.
    if (const std::string &chosen = config().general.gamePort;
        !chosen.empty() && config().findPort(chosen) == nullptr) {
        config().general.gamePort.clear();

        pushGeneral();
    }

    pushLists();
    pushProfile();
    pushCommand();
}

namespace {

template<typename Item>
void moveTo(std::vector<Item> &list, const int from, const int to) {
    if (from == to || from < 0 || std::cmp_greater_equal(from, list.size())
        || to < 0 || std::cmp_greater_equal(to, list.size())) {
        return;
    }

    const auto first = list.begin();
    const auto at = first + from;
    const auto onto = first + to;

    if (to > from) {
        std::rotate(at, at + 1, onto + 1);
    } else {
        std::rotate(onto, at, at + 1);
    }
}

}

void ConfigBridge::bind() {
    const auto &cfg = _window->global<ui::Cfg>();

    // What the badges along the bottom of a profile's card say.
    cfg.on_profile_badges([](int, const int index) {
        std::vector<ui::BadgeSpec> badges;

        if (index < 0 || std::cmp_greater_equal(index, config().profiles.size())) {
            return std::shared_ptr<slint::Model<ui::BadgeSpec>>(
                std::make_shared<slint::VectorModel<ui::BadgeSpec>>(std::move(badges)));
        }

        const Profile &each = config().profiles[static_cast<size_t>(index)];
        const bool ready = !each.port.empty() || each.customCommand;
        int loaded = 0;

        for (const FileEntry &file : each.files) {
            if (file.enabled) {
                ++loaded;
            }
        }

        if (dosPortOf(each)) {
            badges.push_back(ui::BadgeSpec{.text = "DOS", .kind = "muted", .dot = true});
        }

        if (!ready) {
            badges.push_back(ui::BadgeSpec{.text = "No port", .kind = "warning", .dot = true});
        } else if (!each.files.empty()) {
            const size_t count = each.files.size();
            const std::string said = std::cmp_equal(loaded, count)
                ? std::to_string(count) + (count == 1 ? " file" : " files")
                : std::to_string(loaded) + " of " + std::to_string(count) + " loaded";

            badges.push_back(ui::BadgeSpec{
                .text = Convert::text(said),
                .kind = "muted",
                .dot = true,
            });
        }

        // No multiplayer setting reaches a DOS port's command line, so the card
        // does not claim that profile is in a game with anyone.
        if (const int role = netRoleOf(each.multiplayer); role != 0 && !dosPortOf(each)) {
            badges.push_back(ui::BadgeSpec{
                .text = role == 1 ? "Hosting" : "Multiplayer",
                .kind = "muted",
                .dot = true,
            });
        }

        if (each.replay.mode != 0) {
            badges.push_back(ui::BadgeSpec{
                .text = each.replay.mode == 1 ? "Recording" : "Replay",
                .kind = "muted",
                .dot = true,
            });
        }

        return std::shared_ptr<slint::Model<ui::BadgeSpec>>(
            std::make_shared<slint::VectorModel<ui::BadgeSpec>>(std::move(badges)));
    });

    cfg.on_art_key([](int) {
        const Profile &active = profile();

        return Convert::text(artKeyOf(active, config().findIwad(active.iwad)));
    });

    cfg.on_game_key([](const slint::SharedString &iwad) {
        return Convert::text(gameKeyOf(Convert::plain(iwad)));
    });

    cfg.on_game_command_line([](int, const slint::SharedString &iwad) {
        return Convert::text(Launcher::commandLine(oneGame(Convert::plain(iwad))));
    });

    cfg.on_zdl_file_name([] { return Convert::text(zdlFileName(profile().name)); });

    cfg.on_index_of([](const std::shared_ptr<slint::Model<slint::SharedString>> &list,
                       const slint::SharedString &wanted) {
        for (size_t row = 0; row < list->row_count(); row++) {
            if (*list->row_data(row) == wanted) {
                return static_cast<int>(row);
            }
        }

        return -1;
    });

    cfg.on_describe_iwad([](const slint::SharedString &file) {
        return Convert::text(FileInfo::describeIwad(Convert::toPath(file)));
    });

    cfg.on_describe_port([](const slint::SharedString &file) {
        return Convert::text(FileInfo::describePort(Convert::toPath(file)));
    });

    cfg.on_set_filter([this](const slint::SharedString &value) {
        _filter = Convert::plain(value);

        _window->global<ui::Cfg>().set_filter(value);
        pushShelf();
    });

    // The active profile.

    cfg.on_set_profile_index([this](const int index) {
        const std::vector<Profile> &profiles = config().profiles;

        if (index < 0 || std::cmp_greater_equal(index, profiles.size())
            || profiles[static_cast<size_t>(index)].id == config().activeProfileId) {
            return;
        }

        config().setActiveProfile(profiles[static_cast<size_t>(index)].id);
        reload();
    });

    cfg.on_set_iwad([this](const slint::SharedString &value) {
        if (Convert::plain(value) == profile().iwad) {
            return;
        }

        profile().iwad = Convert::plain(value);

        pushProfile();

        // A different IWAD is a different set of maps to warp to.
        touch();
    });

    cfg.on_set_port([this](const slint::SharedString &value) {
        if (Convert::plain(value) == profile().port) {
            return;
        }

        profile().port = Convert::plain(value);

        pushProfile();
        pushCommand();
    });

    cfg.on_set_skill([this](const int value) {
        profile().skill = value;

        pushProfile();
        pushCommand();
    });

    cfg.on_set_monsters([this](const int value) {
        profile().monsters = value;

        pushProfile();
        pushCommand();
    });

    cfg.on_set_warp([this](const slint::SharedString &value) {
        profile().warp = Convert::plain(value);

        pushProfile();
        pushCommand();
    });

    cfg.on_set_extra([this](const slint::SharedString &value) {
        profile().extra = Convert::plain(value);

        pushProfile();
        pushCommand();
    });

    cfg.on_set_multiplayer_open([this](const bool value) {
        profile().dialogOpen = value;

        pushProfile();
    });

    cfg.on_set_shared_config([this](const bool value) {
        profile().sharedConfig = value;

        pushProfile();
        pushCommand();
    });

    cfg.on_set_command_override([this](const bool value) {
        // Taken over for the first time, it starts as what ZDL would have run: a
        // line to edit rather than a blank one.
        if (value && profile().command.empty()) {
            profile().command = Launcher::commandTemplate(config());
        }

        profile().customCommand = value;

        pushProfile();
        pushCommand();
        pushProfiles();
    });

    cfg.on_set_command([this](const slint::SharedString &value) {
        profile().command = Convert::plain(value);

        pushProfile();
        pushCommand();
    });

    cfg.on_set_dos_fullscreen([this](const bool value) {
        profile().dosFullscreen = value;

        pushProfile();
        pushCommand();
    });

    cfg.on_set_capture_output([this](const bool value) {
        profile().captureOutput = value;

        pushProfile();
    });

    // The multiplayer panel.

    cfg.on_set_net_role([this](const int value) {
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

            // The count is what separates the two: a host needs one, a joiner
            // must not have one.
            mp.players = value == 1 ? std::max(mp.players, 2) : 0;
        }

        pushMultiplayer();
        pushCommand();
        pushProfiles();
    });

    cfg.on_set_game_type([this](const int value) {
        multiplayer().gameType = value;

        pushMultiplayer();
        pushCommand();
        pushProfiles();
    });

    cfg.on_set_players([this](const int value) {
        multiplayer().players = value;

        pushMultiplayer();
        pushCommand();
        pushProfiles();
    });

    cfg.on_set_host([this](const slint::SharedString &value) {
        multiplayer().host = Convert::plain(value);

        pushMultiplayer();
        pushCommand();
    });

    cfg.on_set_net_port([this](const slint::SharedString &value) {
        multiplayer().port = Convert::plain(value);

        pushMultiplayer();
        pushCommand();
    });

    cfg.on_set_frag_limit([this](const slint::SharedString &value) {
        multiplayer().fragLimit = Convert::plain(value);

        pushMultiplayer();
        pushCommand();
    });

    cfg.on_set_time_limit([this](const slint::SharedString &value) {
        multiplayer().timeLimit = Convert::plain(value);

        pushMultiplayer();
        pushCommand();
    });

    cfg.on_set_dmflags([this](const slint::SharedString &value) {
        multiplayer().dmflags = Convert::plain(value);

        pushMultiplayer();
        pushCommand();
    });

    cfg.on_set_dmflags2([this](const slint::SharedString &value) {
        multiplayer().dmflags2 = Convert::plain(value);

        pushMultiplayer();
        pushCommand();
    });

    cfg.on_set_extratic([this](const int value) {
        multiplayer().extratic = value;

        pushMultiplayer();
        pushCommand();
    });

    cfg.on_set_netmode([this](const int value) {
        multiplayer().netmode = value;

        pushMultiplayer();
        pushCommand();
    });

    cfg.on_set_dup([this](const int value) {
        multiplayer().dup = value;

        pushMultiplayer();
        pushCommand();
    });

    cfg.on_set_savegame([this](const slint::SharedString &value) {
        multiplayer().savegame = Convert::plain(value);

        pushMultiplayer();
        pushCommand();
    });

    // The replay panel.

    cfg.on_set_replay_open([this](const bool value) {
        profile().replayOpen = value;

        pushProfile();
    });

    cfg.on_set_replay_mode([this](const int value) {
        ReplaySettings &demo = replay();

        if (value == demo.mode) {
            return;
        }

        demo.mode = value;

        if (value == 2) {
            // A demo the last run wrote is not in the list yet, and turning the
            // panel to playing one back is where that matters.
            _replaysRead = false;

            // Nothing named yet, so the newest one is what is reached for.
            if (demo.file.empty()) {
                if (const std::vector<std::string> found = Launcher::replays(config());
                    !found.empty()) {
                    demo.file = found.front();
                }
            }
        }

        pushReplay();
        pushCommand();
        pushProfiles();
    });

    cfg.on_set_replay_file([this](const slint::SharedString &value) {
        replay().file = Convert::plain(value);

        pushReplay();
        pushCommand();
    });

    cfg.on_set_replay_index([this](const int index) {
        // Against the list the picker is showing, which is what was clicked.
        replay().file = index >= 0 && std::cmp_less(index, _replays.size())
            ? _replays[static_cast<size_t>(index)]
            : std::string();

        pushReplay();
        pushCommand();
    });

    cfg.on_set_replay_playback([this](const int value) {
        replay().playback = value;

        pushReplay();
        pushCommand();
    });

    cfg.on_set_replay_complevel([this](const int index) {
        replay().compatibility = complevelAt(
            Launcher::complevels(Launcher::demoSupport(config()).complevel), index);

        pushReplay();
        pushCommand();
    });

    cfg.on_set_replay_longtics([this](const bool value) {
        replay().longtics = value;

        pushReplay();
        pushCommand();
    });

    cfg.on_set_replay_solo_net([this](const bool value) {
        replay().soloNet = value;

        pushReplay();
        pushCommand();
    });

    cfg.on_refresh_replays([this] {
        _replaysRead = false;

        pushReplay();
    });

    // The saves panel.

    cfg.on_set_save_open([this](const bool value) {
        profile().saveOpen = value;

        pushProfile();
    });

    cfg.on_set_save_enabled([this](const bool value) {
        SaveSettings &picked = save();

        if (value == picked.enabled) {
            return;
        }

        picked.enabled = value;

        if (value) {
            // A game saved by the last run is not in the list yet, and switching
            // this on is where that matters.
            _savesRead = false;

            // Nothing named yet, so the newest one is what is reached for.
            if (picked.file.empty()) {
                if (const std::vector<std::string> found = Launcher::saves(config());
                    !found.empty()) {
                    picked.file = found.front();
                }
            }
        }

        pushSave();
        pushCommand();
    });

    cfg.on_set_save_index([this](const int index) {
        // Against the list the picker is showing, which is what was clicked.
        save().file = index >= 0 && std::cmp_less(index, _saves.size())
            ? _saves[static_cast<size_t>(index)]
            : std::string();

        pushSave();
        pushCommand();
    });

    cfg.on_refresh_saves([this] {
        _savesRead = false;

        pushSave();
    });

    // Settings that outlive any one profile.

    cfg.on_set_game_port([this](const slint::SharedString &value) {
        config().general.gamePort = Convert::plain(value);

        pushGeneral();
        pushShelf();
    });

    cfg.on_set_always_add([this](const slint::SharedString &value) {
        config().general.alwaysAdd = Convert::plain(value);

        pushGeneral();
        pushCommand();
    });

    cfg.on_set_dosbox([this](const slint::SharedString &value) {
        config().general.dosbox = Convert::plain(value);

        pushGeneral();

        // The front of the command line for every DOS port there is.
        pushCommand();
    });

    cfg.on_set_auto_close([this](const bool value) {
        config().general.autoClose = value;

        pushGeneral();
    });

    cfg.on_set_launch_zdl_immediately([this](const bool value) {
        config().general.launchZdlImmediately = value;

        pushGeneral();
    });

    cfg.on_set_show_paths([this](const bool value) {
        config().general.showPaths = value;

        pushGeneral();
    });

    cfg.on_set_start_view([this](const slint::SharedString &value) {
        config().general.startView = value == "games" ? "games" : "profiles";

        pushGeneral();
    });

    cfg.on_set_profile_configs([this](const bool value) {
        config().general.profileConfigs = value;

        pushGeneral();

        // The port's config file is part of every profile's command line.
        pushProfile();
        pushCommand();
    });

    cfg.on_set_ignore_user_config([this](const bool value) {
        std::string error;

        // The flag lives in the user config, so unless that is the open one this
        // writes another file there and then.
        if (!Session::get().setUserConfigIgnored(value, &error)) {
            _notifier->error("Could not write the user config: " + error);

            return;
        }

        pushGeneral();
    });

    // Profiles.

    cfg.on_move_profile([this](const int from, const int to) {
        moveTo(config().profiles, from, to);

        pushProfiles();
        pushProfile();
    });

    cfg.on_add_profile([this](const slint::SharedString &name) {
        config().setActiveProfile(config().addProfile(Convert::plain(name)));
        reload();
    });

    cfg.on_duplicate_profile([this] {
        // There is a stand-in profile to read while the list is empty, but it is
        // nobody's and copying it would make a profile out of nothing.
        if (config().profiles.empty()) {
            return;
        }

        config().setActiveProfile(config().duplicateActiveProfile(profile().name));
        reload();
    });

    cfg.on_rename_profile([this](const slint::SharedString &name) {
        const std::string wanted = Convert::plain(name);

        if (wanted.empty()) {
            return;
        }

        // uniqueProfileName compares against this profile too, so an unchanged
        // name must not turn into "name (2)".
        Profile &active = profile();

        active.name = Text::iequals(active.name, wanted)
            ? Text::trim(wanted)
            : config().uniqueProfileName(wanted);

        pushProfiles();
        pushProfile();
    });

    cfg.on_remove_profile([this] {
        config().removeProfile(config().activeProfileId);
        reload();
    });

    // Clearing, in the three sizes the old ZDL menu offered.

    cfg.on_clear_multiplayer([this] {
        multiplayer() = MultiplayerSettings();

        pushMultiplayer();
        pushCommand();
        pushProfiles();
    });

    cfg.on_clear_replay([this] {
        replay() = ReplaySettings();

        pushReplay();
        pushCommand();
        pushProfiles();
    });

    cfg.on_clear_profile([this] {
        profile().clearSettings();
        reload();
    });

    cfg.on_clear_everything([this] {
        config().reset();
        reload();

        if (replaced) {
            replaced(false);
        }
    });

    // The file this is all kept in.

    cfg.on_save_as([this](const slint::SharedString &path) {
        std::string error;

        if (!Session::get().saveAs(Convert::toPath(path), &error)) {
            _notifier->error("Could not save to " + Convert::plain(path) + ": " + error);

            return;
        }

        pushPath();
        _notifier->success("Saved to " + Convert::plain(path) + ".");
    });

    cfg.on_load([this](const slint::SharedString &path) {
        std::string error;

        // What the config being left behind still owes goes to it, not to the
        // file about to take its place.
        flush();

        if (!Session::get().load(Convert::toPath(path), &error)) {
            _notifier->error("Could not read " + Convert::plain(path) + ": " + error);

            return;
        }

        reload();

        if (replaced) {
            replaced(true);
        }

        _notifier->success("Loaded " + Convert::plain(path) + ".");
    });

    cfg.on_adopt_as_user_config([this] {
        std::string error;

        if (!Session::get().adoptAsUserConfig(&error)) {
            _notifier->error("Could not write the user config: " + error);

            return;
        }

        pushPath();
        _notifier->success("This config is now the one ZDL4 opens by default.");
    });

    cfg.on_load_zdl([this](const slint::SharedString &path) {
        Profile loaded;

        if (!Import::loadZdlFile(Convert::toPath(path), loaded)) {
            _notifier->error("Could not read " + Convert::plain(path) + " as a .zdl file.");

            return;
        }

        loaded.name = config().uniqueProfileName(loaded.name);
        config().profiles.push_back(loaded);

        // A .zdl carries no config file name for the profile it becomes.
        config().ensureConfigFiles();
        config().setActiveProfile(loaded.id);
        reload();

        _notifier->success("Added " + loaded.name + " from " + Convert::plain(path) + ".");
    });

    cfg.on_save_zdl([this](const slint::SharedString &path) {
        if (!Import::saveZdlFile(Convert::toPath(path), profile())) {
            _notifier->error("Could not write " + Convert::plain(path) + ".");

            return;
        }

        _notifier->success("Saved " + profile().name + " to " + Convert::plain(path) + ".");
    });

    // Launching.

    cfg.on_launch([this] {
        if (config().profiles.empty()) {
            return;
        }

        start(profileKeyOf(config().activeProfileId), profile().name, config());
    });

    cfg.on_launch_at([this](const int index) {
        const std::vector<Profile> &profiles = config().profiles;

        if (index < 0 || std::cmp_greater_equal(index, profiles.size())) {
            return;
        }

        // Everything below works on the active profile, so the pressed card
        // becomes it first.
        if (profiles[static_cast<size_t>(index)].id != config().activeProfileId) {
            config().setActiveProfile(profiles[static_cast<size_t>(index)].id);
            reload();
        }

        start(profileKeyOf(config().activeProfileId), profile().name, config());
    });

    cfg.on_launch_game([this](const slint::SharedString &iwad) {
        const std::string name = Convert::plain(iwad);

        start(gameKeyOf(name), name, oneGame(name));
    });

    // The three lists.

    cfg.on_add_files([this](const std::shared_ptr<slint::Model<slint::SharedString>> &paths) {
        for (size_t row = 0; row < paths->row_count(); row++) {
            profile().files.push_back(FileEntry{
                .file = Convert::plain(*paths->row_data(row)),
                .enabled = true,
            });
        }

        pushLists();
        touch();
    });

    cfg.on_remove_file([this](const int row) {
        std::vector<FileEntry> &files = profile().files;

        if (row < 0 || std::cmp_greater_equal(row, files.size())) {
            return;
        }

        files.erase(files.begin() + row);

        pushLists();
        touch();
    });

    cfg.on_clear_files([this] {
        profile().files.clear();

        pushLists();
        touch();
    });

    cfg.on_move_file([this](const int from, const int to) {
        moveTo(profile().files, from, to);

        pushLists();
        touch();
    });

    cfg.on_set_file_enabled([this](const int row, const bool enabled) {
        std::vector<FileEntry> &files = profile().files;

        if (row < 0 || std::cmp_greater_equal(row, files.size())) {
            return;
        }

        files[static_cast<size_t>(row)].enabled = enabled;

        pushLists();
        touch();
    });

    cfg.on_add_iwads([this](const std::shared_ptr<slint::Model<slint::SharedString>> &paths) {
        for (size_t row = 0; row < paths->row_count(); row++) {
            const std::string file = Convert::plain(*paths->row_data(row));

            if (file.empty()) {
                continue;
            }

            if (folder(file)) {
                _notifier->warning("A game has to be a file: a source port cannot be pointed "
                                   "at a folder as one. " + file + " was left out.");

                continue;
            }

            config().iwads.push_back(NameEntry{
                .name = uniqueName(config().iwads, FileInfo::describeIwad(file)),
                .file = file,
            });
        }

        pushLists();
        touch();
    });

    cfg.on_update_iwad([this](const int row, const slint::SharedString &name,
                              const slint::SharedString &file) {
        std::vector<NameEntry> &list = config().iwads;

        if (row < 0 || std::cmp_greater_equal(row, list.size())) {
            return;
        }

        NameEntry &entry = list[static_cast<size_t>(row)];
        const std::string chosen = Convert::plain(file);

        // Only when the file itself moved: an entry already pointing at a
        // folder loads from a config as it stands, and renaming one is no
        // reason to refuse it.
        if (chosen != entry.file && folder(Convert::toPath(file))) {
            _notifier->warning("A game has to be a file: a source port cannot be pointed at a "
                               "folder as one. " + entry.name + " is unchanged.");
            pushLists();

            return;
        }

        const std::string wanted = Convert::plain(name);
        const std::string before = entry.name;
        const std::string after = uniqueName(list, wanted.empty()
            ? FileInfo::describeIwad(Convert::toPath(file))
            : wanted, row);

        entry.name = after;
        entry.file = chosen;

        pushLists();

        if (before != after) {
            renamedIwad(before, after);
        }

        touch();
    });

    cfg.on_remove_iwad([this](const int row) {
        std::vector<NameEntry> &list = config().iwads;

        if (row < 0 || std::cmp_greater_equal(row, list.size())) {
            return;
        }

        list.erase(list.begin() + row);

        pushLists();
        touch();
    });

    cfg.on_move_iwad([this](const int from, const int to) {
        moveTo(config().iwads, from, to);

        pushLists();
        touch();
    });

    cfg.on_add_port([this](const slint::SharedString &file, const bool dosbox) {
        addPort(Convert::plain(file), {}, dosbox);
    });

    cfg.on_update_port([this](const int row, const slint::SharedString &name,
                              const slint::SharedString &file, const bool dosbox) {
        updatePort(row, Convert::plain(name), Convert::plain(file), dosbox);
    });

    cfg.on_move_port([this](const int from, const int to) {
        moveTo(config().ports, from, to);

        pushLists();
    });
}
