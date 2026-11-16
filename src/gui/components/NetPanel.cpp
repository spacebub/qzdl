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
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "ttk/draw/Glyphs.h"
#include "ttk/draw/Theme.h"
#include "ttk/toolkit/controls/Pill.h"
#include "ttk/toolkit/layout/Rule.h"
#include "ttk/toolkit/layout/Spacer.h"
#include "ttk/util/Desktop.h"

#include "core/config/Profile.h"
#include "core/launch/Netgame.h"
#include "gui/components/HostField.h"
#include "gui/components/NetPanel.h"
#include "gui/services/Filters.h"
#include "gui/state/State.h"

using namespace ttk;

namespace {

// -netmode takes a number, and -1 leaves it to the port.
constexpr int NETMODE_PORT = -1;

using Choice = MultistateSwitch::Choice;

struct Swatch {
    std::string_view name;
    std::string_view color;
};

// The team colors, which read well against every map.
constexpr std::array SWATCHES = {
        Swatch{.name = "Blue", .color = "#0000ff"},
        Swatch{.name = "Red", .color = "#ff0000"},
        Swatch{.name = "Green", .color = "#00ff00"},
        Swatch{.name = "Gold", .color = "#ffff00"},
        Swatch{.name = "Black", .color = "#000000"},
        Swatch{.name = "White", .color = "#ffffff"},
        Swatch{.name = "Orange", .color = "#ff8000"},
        Swatch{.name = "Purple", .color = "#ff00ff"},
};

// The ZDoom ports number their default teams in this order.
constexpr std::array TEAMS = {"Blue", "Red", "Green", "Gold", "Black", "White", "Orange", "Purple"};

constexpr int CUSTOM = static_cast<int>(SWATCHES.size());

std::vector<Choice> gameTypes(const bool teams, const bool deathmatch3) {
    std::vector<Choice> out = {
            {.value = static_cast<int>(GameType::Coop), .label = "Co-op"},
            {.value = static_cast<int>(GameType::Deathmatch), .label = "Deathmatch"},
            {.value = static_cast<int>(GameType::AltDeathmatch), .label = "Alt deathmatch"},
    };

    if (deathmatch3) {
        out.push_back({.value = static_cast<int>(GameType::Deathmatch3), .label = "Deathmatch 3.0"});
    }

    if (teams) {
        out.push_back({.value = static_cast<int>(GameType::TeamDeathmatch), .label = "Team deathmatch"});
    }

    return out;
}

// A swatch, Custom for any other color, or -1 for none.
int swatchOf(const std::string &color) {
    if (color.empty()) {
        return -1;
    }

    const std::string normal = Netgame::playerColor(color);

    for (size_t at = 0; at < SWATCHES.size(); at++) {
        if (SWATCHES[at].color == normal) {
            return static_cast<int>(at);
        }
    }

    return CUSTOM;
}

}

namespace components {

using Dialect::Feature;

NetPanel::NetPanel(Reach *reach)
    : SummaryPanel("Multiplayer", [reach](const bool open) { reach->config.panels().setMultiplayerOpen(open); }),
      _reach(reach) {
    _role = tools()->append(std::make_unique<MultistateSwitch>([this](const int value) {
        const auto role = static_cast<NetRole>(value);

        _reach->config.panels().setNetRole(role);
        _reach->config.panels().setMultiplayerOpen(role != NetRole::Alone);
    }));

    _role->set_options({
            {.value = static_cast<int>(NetRole::Alone), .label = "Off"},
            {.value = static_cast<int>(NetRole::Host), .label = "Host"},
            {.value = static_cast<int>(NetRole::Join), .label = "Join"},
    });

    // Last, so it sits against the far edge of the heading.
    _reset = tools()->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Refresh, [this] {
        _reach->ask("Reset the multiplayer settings?",
                    "The side this profile is on, the game it opens and every address, limit and "
                    "flag under it go back to their defaults. The rest of the profile is "
                    "untouched.",
                    "Reset",
                    true,
                    [this] { _reach->config.panels().clearMultiplayer(); });
    }));

    _reset->size(Theme::control)->outlined();
    _reset->fixedWidth = Theme::control;

    Box *body = CollapsiblePanel::body();

    body->append(std::make_unique<Rule>());

    _note = body->append(std::make_unique<Label>());
    _note->font(400, Theme::fontSmall)->tone(&Theme::Palette::faint)->wrap();

    // Hosting.
    _hosting = body->append(Box::row());
    _hosting->spacing(20.0)->cross(Box::Place::End);

    Box *type = _hosting->append(Box::column());

    type->spacing(6.0);
    type->append(std::make_unique<Label>("Game type"))->section();

    _gameType = type->append(std::make_unique<MultistateSwitch>(
            [this](const int value) { _reach->config.panels().setGameType(static_cast<GameType>(value)); }));

    _gameType->set_options(gameTypes(false, false));

    _players = _hosting->append(std::make_unique<Stepper>(
            "Players", [this](const int value) { _reach->config.panels().setPlayers(value); }));

    _players->range(1, 8)->tooltip("How many the game is opened for, this machine included");
    _players->fixedWidth = 170.0;

    _netPort = _hosting->append(std::make_unique<Field>(
            "Listen on port", [this](const std::string &value) { _reach->config.panels().setNetPort(value); }));

    _netPort->placeholder("Default")->mono();
    _netPort->fixedWidth = 160.0;

    Box *listing = _hosting->append(Box::column());

    listing->spacing(6.0);
    listing->append(std::make_unique<Label>("Listing"))->section();

    _listed = listing->append(
            std::make_unique<Toggle>("Public", [this](const bool on) { _reach->config.panels().setListed(on); }));

    _listed->hint = "Puts the game on the master server's list, where anyone can find it. Off "
                    "keeps it to whoever has the address";
    _listed->fixedHeight = Theme::control;

    _hosting->append(std::make_unique<Spacer>());

    // Joining.
    _joining = body->append(Box::row());
    _joining->spacing(12.0);

    _address = _joining->append(std::make_unique<HostField>(
            "Address of the game",
            [this](const std::string &value) { _reach->config.panels().setHost(value); },
            [this](const std::string &value) { _reach->config.panels().setNetPort(value); }));

    _address->stretch = 1.0;

    // This player, on either side.
    _player = body->append(Box::row());
    _player->spacing(12.0)->cross(Box::Place::End);

    _playerName = _player->append(std::make_unique<Field>(
            "Player name", [this](const std::string &value) { _reach->config.panels().setPlayerName(value); }));

    _playerName->placeholder("The port's own");
    _playerName->stretch = 1.0;
    _playerName->input()->hint = "The port keeps the name in its own settings once it is passed";

    _color = _player->append(std::make_unique<Select>("Color", [this](const int index) {
        _customColor = index == CUSTOM;

        if (index == CUSTOM) {
            const std::string now = State::get().cfg.playerColor;

            _reach->config.panels().setPlayerColor(now.empty() ? "#808080" : now);
        } else {
            _reach->config.panels().setPlayerColor(
                    index >= 0 && index < CUSTOM ? std::string(SWATCHES[static_cast<size_t>(index)].color) : "");
        }
    }));

    std::vector<std::string> swatches;

    swatches.reserve(SWATCHES.size() + 1);

    for (const Swatch &each : SWATCHES) {
        swatches.emplace_back(each.name);
    }

    swatches.emplace_back("Custom");

    _color->set_options(std::move(swatches));
    _color->placeholder("The port's own")
            ->clearable()
            ->tooltip("The port keeps the color in its own settings once it is passed");
    _color->fixedWidth = 170.0;

    _hex = _player->append(std::make_unique<Field>(
            "Hex", [this](const std::string &value) { _reach->config.panels().setPlayerColor(value); }));

    _hex->placeholder("#rrggbb")->mono();
    _hex->fixedWidth = 120.0;

    _team = _player->append(
            std::make_unique<Select>("Team", [this](const int index) { _reach->config.panels().setTeam(index); }));

    _team->set_options(std::vector<std::string>(TEAMS.begin(), TEAMS.end()));
    _team->placeholder("Auto")->clearable();
    _team->fixedWidth = 150.0;

    // The rules of a hosted game.
    _rules = body->append(Box::column());
    _rules->spacing(12.0);

    _rules->append(std::make_unique<Rule>());
    _rules->append(std::make_unique<Label>("Rules of the game"))->section();

    Box *limits = _rules->append(Box::row());

    limits->spacing(12.0)->cross(Box::Place::Start);

    _fragLimit = limits->append(std::make_unique<Field>(
            "Frag limit", [this](const std::string &value) { _reach->config.panels().setFragLimit(value); }));

    _fragLimit->placeholder("None")->mono();
    _fragLimit->stretch = 1.0;

    _timeLimit = limits->append(std::make_unique<Field>(
            "Time limit", [this](const std::string &value) { _reach->config.panels().setTimeLimit(value); }));

    _timeLimit->placeholder("None")->mono()->note("In minutes");
    _timeLimit->stretch = 1.0;

    _savegame = _rules->append(std::make_unique<Field>(
            "Start from a save", [this](const std::string &value) { _reach->config.panels().setSavegame(value); }));

    _savegame->placeholder("None · the game starts at its first map")->mono();

    _savegame->icon(Glyphs::Glyph::Folder, "Browse", [this] {
        // A save picked from anywhere else would never be found, so the folder is shown instead.
        if (const std::string &folder = State::get().cfg.netSaveFolder; !folder.empty()) {
            std::error_code code;
            std::filesystem::create_directories(folder, code);
            Desktop::open(folder);

            return;
        }

        _reach->files.open("Select a save game",
                           Filters::save(),
                           false,
                           false,
                           false,
                           LastDir::SAVE,
                           Picked::first([this](const std::string &path) {
                               _reach->config.panels().setSavegame(path);
                               _reach->touch();
                           }));
    });

    _saveClash = _rules->append(std::make_unique<Label>(""));

    _saveClash->font(400, Theme::fontSmall)->tone(&Theme::Palette::warning)->wrap();

    // The connection, which folds on its own.
    _tuning = body->append(Box::column());
    _tuning->spacing(12.0);

    _tuning->append(std::make_unique<Rule>());

    _tuningHead = _tuning->append(std::make_unique<DisclosureHeading>("Connection", [this] {
        State::get().nav.tuning = !State::get().nav.tuning;

        _reach->touch();
    }));

    Box *knobs = _tuningHead->body()->append(Box::row());

    knobs->spacing(20.0)->cross(Box::Place::End);

    Box *mode = knobs->append(Box::column());

    mode->spacing(6.0);
    mode->append(std::make_unique<Label>("Net mode"))->section();

    _netmode = mode->append(
            std::make_unique<MultistateSwitch>([this](const int value) { _reach->config.panels().setNetmode(value); }));

    _netmode->set_options({
            {.value = NETMODE_PORT, .label = "The port's own"},
            {.value = 0, .label = "Peer to peer"},
            {.value = 1, .label = "Client/server"},
    });

    _dup = knobs->append(std::make_unique<Stepper>("Duplicate tics",
                                                   [this](const int value) { _reach->config.panels().setDup(value); }));

    _dup->range(1, 9)->clearable(0, "Off")->tooltip(
            "Sends each tic more than once, which trades bandwidth for a connection that "
            "drops packets");
    _dup->fixedWidth = 170.0;

    Box *extra = knobs->append(Box::column());

    extra->spacing(6.0);
    extra->append(std::make_unique<Label>("Extra tic"))->section();

    _extratic = extra->append(std::make_unique<MultistateSwitch>(
            [this](const int value) { _reach->config.panels().setExtratic(value != 0); }));

    _extratic->set_options({{.value = 0, .label = "Off"}, {.value = 1, .label = "On"}});

    knobs->append(std::make_unique<Spacer>());
}

std::string NetPanel::summary() {
    const State::Cfg &cfg = State::get().cfg;
    const Dialect::Features &features = cfg.features;

    if (cfg.netRole == NetRole::Alone) {
        return "Off · this profile launches a single player game";
    }

    const bool unsupported = (cfg.netRole == NetRole::Host && !features.has(Feature::hosts))
                             || (cfg.netRole == NetRole::Join && !features.has(Feature::joins));

    if (unsupported) {
        return "not a side this port takes";
    }

    if (cfg.netRole == NetRole::Host) {
        return (features.has(Feature::players) ? "for " + std::to_string(cfg.players) + " players" : "open to others")
               + (cfg.netPort.empty() ? "" : ", on port " + cfg.netPort);
    }

    if (cfg.host.empty()) {
        return "no address yet";
    }

    return cfg.host + (cfg.netPort.empty() ? "" : ":" + cfg.netPort);
}

std::string NetPanel::tuningSummary() {
    const State::Cfg &cfg = State::get().cfg;
    const Dialect::Features &features = cfg.features;

    const int mode = features.has(Feature::netmode) ? cfg.netmode : NETMODE_PORT;
    const int dup = features.has(Feature::dup) ? cfg.dup : 0;
    const bool extra = features.has(Feature::extratic) && cfg.extratic;

    if (mode == NETMODE_PORT && dup <= 0 && !extra) {
        return "left to the port";
    }

    std::string said = mode == -1 ? "" : (mode == 0 ? "peer to peer" : "client/server");

    if (dup > 0) {
        said += (said.empty() ? "" : " · ") + std::to_string(dup) + "× tics";
    }

    if (extra) {
        said += (said.empty() ? "" : " · ") + std::string("extra tic");
    }

    return said;
}

void NetPanel::sync() {
    const State::Cfg &cfg = State::get().cfg;
    const Dialect::Features &features = cfg.features;
    const bool teams = features.has(Feature::teams);
    const bool deathmatch3 = features.has(Feature::deathmatch3);

    set_visible(features.has(Feature::hosts) || features.has(Feature::joins));

    if (!visible()) {
        return;
    }

    const bool unsupported = (cfg.netRole == NetRole::Host && !features.has(Feature::hosts))
                             || (cfg.netRole == NetRole::Join && !features.has(Feature::joins));
    const bool broken = unsupported || (cfg.netRole == NetRole::Join && cfg.host.empty());

    set_open(cfg.multiplayerOpen);
    summarise(summary(), broken);

    pill()->set_visible(cfg.netRole != NetRole::Alone);
    // clang-format off
    pill()->set_text(cfg.netRole == NetRole::Join               ? "Joining"
                    : cfg.gameType == GameType::Coop           ? "Co-op"
                    : cfg.gameType == GameType::Deathmatch     ? "Deathmatch"
                    : cfg.gameType == GameType::TeamDeathmatch ? (teams ? "Team deathmatch" : "Deathmatch")
                    : cfg.gameType == GameType::Deathmatch3    ? (deathmatch3 ? "Deathmatch 3.0" : "Deathmatch")
                                                               : "Alt deathmatch");
    // clang-format on
    pill()->kind(broken ? Pill::Kind::Warning : Pill::Kind::None);

    _role->set_current(static_cast<int>(cfg.netRole));
    _reset->set_enabled(cfg.multiplayerSet);
    _reset->tooltip(cfg.multiplayerSet ? "Put every multiplayer setting back to its default"
                                       : "Nothing here has been set");

    const bool hosting = cfg.netRole == NetRole::Host && features.has(Feature::hosts);
    const bool joining = cfg.netRole == NetRole::Join && features.has(Feature::joins);

    const bool apart = !cfg.netSaveFolder.empty();
    const bool profileSave = cfg.saveEnabled && !cfg.saveFile.empty();

    _hosting->set_visible(hosting);
    _joining->set_visible(joining);
    _rules->set_visible(hosting);

    if (cfg.netRole == NetRole::Alone) {
        _note->set_visible(true);
        _note->set_text("This profile starts a game for one. Host opens a game other machines "
                        "can connect to; Join connects to one somebody else is running.");
        _note->tone(&Theme::Palette::faint);
    } else if (unsupported) {
        _note->set_visible(true);
        _note->set_text(cfg.netRole == NetRole::Host ? "This port opens no game of its own: a server program beside it "
                                                       "does, and the port joins that. Nothing under here reaches the "
                                                       "launch."
                                                     : "This port has no way to join a game, so nothing under here "
                                                       "reaches the launch.");
        _note->tone(&Theme::Palette::warning);
    } else if (joining && apart && profileSave && !cfg.host.empty()) {
        _note->set_visible(true);
        _note->set_text("The save from the Saves panel is left out, since UZDoom reads netgame saves only from "
                        "the netgame save folder.");
        _note->tone(&Theme::Palette::warning);
    } else if (joining) {
        _note->set_visible(true);
        _note->set_text(cfg.host.empty() ? "Without an address there is nothing to join, and the profile "
                                           "launches a single player game."
                        : features.has(Feature::player) ? "How the game is played is the host's to decide. The name, "
                                                          "color and team below go along with this player."
                                                        : "How the game is played is the host's to decide, so "
                                                          "there is nothing else to set on this side.");
        _note->tone(cfg.host.empty() ? &Theme::Palette::warning : &Theme::Palette::faint);
    } else {
        _note->set_visible(false);
    }

    if (_teamsOffered != teams || _deathmatch3Offered != deathmatch3) {
        _teamsOffered = teams;
        _deathmatch3Offered = deathmatch3;
        _gameType->set_options(gameTypes(teams, deathmatch3));
    }

    // Without teams or Deathmatch 3.0 the port plays it as plain deathmatch, so it is shown as one.
    const bool plain = (cfg.gameType == GameType::TeamDeathmatch && !teams)
                       || (cfg.gameType == GameType::Deathmatch3 && !deathmatch3);
    const GameType shownType = plain ? GameType::Deathmatch : cfg.gameType;

    _gameType->set_current(static_cast<int>(shownType));

    const int swatch = swatchOf(cfg.playerColor);

    // Picking Custom keeps the hex field open, even while it is empty or holds a preset's color.
    if (_colorProfile != cfg.profileKey) {
        _colorProfile = cfg.profileKey;
        _customColor = swatch == CUSTOM;
    } else if (swatch == CUSTOM) {
        _customColor = true;
    }

    const int shown = _customColor ? CUSTOM : swatch;

    _player->set_visible(features.has(Feature::player) && (hosting || joining));

    if (_playerName->text() != cfg.playerName) {
        _playerName->set_text(cfg.playerName);
    }

    _color->set_current(shown);
    _hex->set_visible(shown == CUSTOM);

    if (shown == CUSTOM && _hex->text() != cfg.playerColor) {
        _hex->set_text(cfg.playerColor);
    }

    _team->set_visible(features.has(Feature::teamPick) && (joining || cfg.gameType == GameType::TeamDeathmatch));
    _team->set_current(cfg.team);
    _team->tooltip(joining ? "Counts only when the host plays team deathmatch" : "The side this player takes");

    _players->set_visible(features.has(Feature::players));
    _players->set_value(cfg.players);

    _listed->parent()->set_visible(features.has(Feature::listing));
    _listed->set_checked(cfg.listed);

    if (_netPort->text() != cfg.netPort) {
        _netPort->set_text(cfg.netPort);
    }

    _address->set(cfg.host, cfg.netPort);

    _fragLimit->set_visible(features.has(Feature::fragLimit));

    _savegame->set_visible(features.has(Feature::savegame));
    _savegame->set_icon_hint(apart ? "Open the netgame save folder" : "Browse");

    if (apart) {
        _savegame->note("Everyone who joins starts in this save. UZDoom reads netgame saves only from the netgame "
                        "save folder, which has one subfolder per game. Type the name of a save from the subfolder "
                        "for this game.");
    } else if (cfg.netSaveByName) {
        _savegame->note("Everyone who joins starts in this save. The port looks the name up in the save folder.");
    } else {
        _savegame->note("Everyone who joins starts in this save.");
    }

    _saveClash->set_text(apart ? "The save from the Saves panel is left out, since UZDoom reads netgame saves only "
                                 "from the netgame save folder."
                               : "The Saves panel names a save too. A port loads only one, and the save from the "
                                 "Saves panel wins.");
    _saveClash->set_visible(features.has(Feature::savegame) && profileSave && (apart || !cfg.savegame.empty()));

    if (_fragLimit->text() != cfg.fragLimit) {
        _fragLimit->set_text(cfg.fragLimit);
    }

    if (_timeLimit->text() != cfg.timeLimit) {
        _timeLimit->set_text(cfg.timeLimit);
    }

    if (_savegame->text() != cfg.savegame) {
        _savegame->set_text(cfg.savegame);
    }

    const bool tunable =
            cfg.netRole != NetRole::Alone && !unsupported
            && (features.has(Feature::extratic) || features.has(Feature::netmode) || features.has(Feature::dup));

    const bool turned = State::get().nav.tuning;

    _tuning->set_visible(tunable);

    _tuningHead->set_open(turned);
    _tuningHead->set_said(turned ? std::string() : tuningSummary());

    _netmode->parent()->set_visible(features.has(Feature::netmode));
    _netmode->set_current(std::clamp(cfg.netmode, NETMODE_PORT, 1));

    _dup->set_visible(features.has(Feature::dup));
    _dup->set_value(cfg.dup);

    _extratic->parent()->set_visible(features.has(Feature::extratic));
    _extratic->set_current(cfg.extratic ? 1 : 0);
}

}
