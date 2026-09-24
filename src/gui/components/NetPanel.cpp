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
#include <string>

#include "ttk/draw/Glyphs.h"
#include "ttk/draw/Theme.h"
#include "ttk/toolkit/controls/Pill.h"
#include "ttk/toolkit/layout/Rule.h"
#include "ttk/toolkit/layout/Spacer.h"

#include "core/config/Profile.h"
#include "gui/components/NetPanel.h"
#include "gui/services/Filters.h"
#include "gui/state/State.h"

using namespace ttk;

namespace {

// -netmode takes a number, and -1 leaves it to the port.
constexpr int NETMODE_PORT = -1;

}

namespace components {

NetPanel::NetPanel(Reach *reach)
    : CollapsiblePanel("Multiplayer",
                       [reach](const bool open) {
                           reach->config.panels().setMultiplayerOpen(open);
                       }),
      _reach(reach) {
    _role = tools()->append(std::make_unique<MultistateSwitch>([this](const int value) {
        const auto role = static_cast<NetRole>(value);

        _reach->config.panels().setNetRole(role);
        _reach->config.panels().setMultiplayerOpen(role != NetRole::Alone);
    }));

    _role->set_options({{.value = static_cast<int>(NetRole::Alone), .label = "Off"},
                       {.value = static_cast<int>(NetRole::Host), .label = "Host"},
                       {.value = static_cast<int>(NetRole::Join), .label = "Join"}});

    // Last, so it sits against the far edge of the heading.
    _reset = tools()->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Refresh, [this] {
        _reach->ask("Reset the multiplayer settings?",
                  "The side this profile is on, the game it opens and every address, limit and "
                  "flag under it go back to their defaults. The rest of the profile is "
                  "untouched.",
                  "Reset", true, [this] { _reach->config.panels().clearMultiplayer(); });
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

    _gameType = type->append(std::make_unique<MultistateSwitch>([this](const int value) {
        _reach->config.panels().setGameType(static_cast<GameType>(value));
    }));

    _gameType->set_options(
        {{.value = static_cast<int>(GameType::Coop), .label = "Co-op"},
         {.value = static_cast<int>(GameType::Deathmatch), .label = "Deathmatch"},
         {.value = static_cast<int>(GameType::AltDeathmatch), .label = "Alt deathmatch"}});

    _players = _hosting->append(std::make_unique<Stepper>("Players", [this](const int value) {
        _reach->config.panels().setPlayers(value);
    }));

    _players->range(1, 8)->tooltip("How many the game is opened for, this machine included");
    _players->fixedWidth = 170.0;

    _netPort = _hosting->append(std::make_unique<Field>("Listen on port",
                                                        [this](const std::string &value) {
        _reach->config.panels().setNetPort(value);
    }));

    _netPort->placeholder("Default")->mono();
    _netPort->fixedWidth = 160.0;

    Box *listing = _hosting->append(Box::column());

    listing->spacing(6.0);
    listing->append(std::make_unique<Label>("Listing"))->section();

    _listed = listing->append(std::make_unique<Toggle>("Public", [this](const bool on) {
        _reach->config.panels().setListed(on);
    }));

    _listed->hint = "Puts the game on the master server's list, where anyone can find it. Off "
                    "keeps it to whoever has the address";
    _listed->fixedHeight = Theme::control;

    _hosting->append(std::make_unique<Spacer>());

    // Joining. Start, so the note under the port leaves the boxes level.
    _joining = body->append(Box::row());
    _joining->spacing(12.0)->cross(Box::Place::Start);

    _host = _joining->append(std::make_unique<Field>("Address of the game",
                                                     [this](const std::string &value) {
        _reach->config.panels().setHost(value);
    }));

    _host->placeholder("A host name or an address")->mono();
    _host->stretch = 1.0;

    _joinPort = _joining->append(std::make_unique<Field>("Port",
                                                         [this](const std::string &value) {
        _reach->config.panels().setNetPort(value);
    }));

    _joinPort->placeholder("Default")->mono();
    _joinPort->fixedWidth = 160.0;

    // The rules of a hosted game.
    _rules = body->append(Box::column());
    _rules->spacing(12.0);

    _rules->append(std::make_unique<Rule>());
    _rules->append(std::make_unique<Label>("Rules of the game"))->section();

    Box *limits = _rules->append(Box::row());

    limits->spacing(12.0)->cross(Box::Place::Start);

    _fragLimit = limits->append(std::make_unique<Field>("Frag limit",
                                                        [this](const std::string &value) {
        _reach->config.panels().setFragLimit(value);
    }));

    _fragLimit->placeholder("None")->mono();
    _fragLimit->stretch = 1.0;

    _timeLimit = limits->append(std::make_unique<Field>("Time limit",
                                                        [this](const std::string &value) {
        _reach->config.panels().setTimeLimit(value);
    }));

    _timeLimit->placeholder("None")->mono()->note("In minutes");
    _timeLimit->stretch = 1.0;

    _dmflags = limits->append(std::make_unique<Field>("dmflags",
                                                      [this](const std::string &value) {
        _reach->config.panels().setDmflags(value);
    }));

    _dmflags->placeholder("None")->mono()->note("The port's own flag word");
    _dmflags->stretch = 1.0;

    _dmflags2 = limits->append(std::make_unique<Field>("dmflags2",
                                                       [this](const std::string &value) {
        _reach->config.panels().setDmflags2(value);
    }));

    _dmflags2->placeholder("None")->mono()->note("The second, where a port has one");
    _dmflags2->stretch = 1.0;

    _savegame = _rules->append(std::make_unique<Field>("Start from a save",
                                                       [this](const std::string &value) {
        _reach->config.panels().setSavegame(value);
    }));

    _savegame->placeholder("None · the game starts at its first map")->mono()
        ->note("Everyone joining drops into the host's saved game");

    _savegame->icon(Glyphs::Glyph::Folder, "Browse", [this] {
        _reach->files.open("Select a save game", Filters::save(), false, false, false, LastDir::SAVE,
                           Picked::first([this](const std::string &path) {
                               _reach->config.panels().setSavegame(path);
                               _reach->touch();
                           }));
    });

    _saveClash = _rules->append(std::make_unique<Label>(
        "The Saves panel names one too, and a port loads one save: that is the one it gets."));

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

    _netmode = mode->append(std::make_unique<MultistateSwitch>([this](const int value) {
        _reach->config.panels().setNetmode(value);
    }));

    _netmode->set_options({{.value = NETMODE_PORT, .label = "The port's own"},
                          {.value = 0, .label = "Peer to peer"},
                          {.value = 1, .label = "Client/server"}});

    _dup = knobs->append(std::make_unique<Stepper>("Duplicate tics", [this](const int value) {
        _reach->config.panels().setDup(value);
    }));

    _dup->range(1, 9)->clearable(0, "Off")
        ->tooltip("Sends each tic more than once, which trades bandwidth for a connection that "
              "drops packets");
    _dup->fixedWidth = 170.0;

    Box *extra = knobs->append(Box::column());

    extra->spacing(6.0);
    extra->append(std::make_unique<Label>("Extra tic"))->section();

    _extratic = extra->append(std::make_unique<MultistateSwitch>([this](const int value) {
        _reach->config.panels().setExtratic(value != 0);
    }));

    _extratic->set_options({{.value = 0, .label = "Off"}, {.value = 1, .label = "On"}});

    knobs->append(std::make_unique<Spacer>());
}

std::string NetPanel::summary() {
    const State::Cfg &cfg = State::get().cfg;

    if (cfg.netRole == NetRole::Alone) {
        return "Off · this profile launches a single player game";
    }

    const bool unsupported = (cfg.netRole == NetRole::Host && !cfg.netHosts)
        || (cfg.netRole == NetRole::Join && !cfg.netJoins);

    if (unsupported) {
        return "not a side this port takes";
    }

    if (cfg.netRole == NetRole::Host) {
        return (cfg.netPlayers ? "for " + std::to_string(cfg.players) + " players"
                               : "open to others")
            + (cfg.netPort.empty() ? "" : ", on port " + cfg.netPort);
    }

    if (cfg.host.empty()) {
        return "no address yet";
    }

    return cfg.host + (cfg.netPort.empty() ? "" : ":" + cfg.netPort);
}

std::string NetPanel::tuningSummary() {
    const State::Cfg &cfg = State::get().cfg;

    const int mode = cfg.hasNetmode ? cfg.netmode : NETMODE_PORT;
    const int dup = cfg.netDup ? cfg.dup : 0;
    const bool extra = cfg.netExtratic && cfg.extratic;

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

    set_visible(cfg.netHosts || cfg.netJoins);

    if (!visible()) {
        return;
    }

    const bool unsupported = (cfg.netRole == NetRole::Host && !cfg.netHosts)
        || (cfg.netRole == NetRole::Join && !cfg.netJoins);
    const bool broken = unsupported || (cfg.netRole == NetRole::Join && cfg.host.empty());

    set_open(cfg.multiplayerOpen);
    set_said(summary(), broken);

    pill()->set_visible(cfg.netRole != NetRole::Alone);
    pill()->set_text(cfg.netRole == NetRole::Join           ? "Joining"
                    : cfg.gameType == GameType::Coop       ? "Co-op"
                    : cfg.gameType == GameType::Deathmatch ? "Deathmatch"
                                                           : "Alt deathmatch");
    pill()->kind(broken ? Pill::Kind::Warning : Pill::Kind::None);

    _role->set_current(static_cast<int>(cfg.netRole));
    _reset->set_enabled(cfg.multiplayerSet);
    _reset->tooltip(cfg.multiplayerSet ? "Put every multiplayer setting back to its default"
                                      : "Nothing here has been set");

    const bool hosting = cfg.netRole == NetRole::Host && cfg.netHosts;
    const bool joining = cfg.netRole == NetRole::Join && cfg.netJoins;

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
        _note->set_text(cfg.netRole == NetRole::Host
                           ? "This port opens no game of its own: a server program beside it "
                             "does, and the port joins that. Nothing under here reaches the "
                             "launch."
                           : "This port has no way to join a game, so nothing under here "
                             "reaches the launch.");
        _note->tone(&Theme::Palette::warning);
    } else if (joining) {
        _note->set_visible(true);
        _note->set_text(cfg.host.empty()
                           ? "Without an address there is nothing to join, and the profile "
                             "launches a single player game."
                           : "How the game is played is the host's to decide, so there is "
                             "nothing else to set on this side.");
        _note->tone(cfg.host.empty() ? &Theme::Palette::warning : &Theme::Palette::faint);
    } else {
        _note->set_visible(false);
    }

    _gameType->set_current(static_cast<int>(cfg.gameType));

    _players->set_visible(cfg.netPlayers);
    _players->set_value(cfg.players);

    _listed->parent()->set_visible(cfg.netListing);
    _listed->set_checked(cfg.listed);

    if (_netPort->text() != cfg.netPort) {
        _netPort->set_text(cfg.netPort);
    }

    if (_host->text() != cfg.host) {
        _host->set_text(cfg.host);
    }

    if (_joinPort->text() != cfg.netPort) {
        _joinPort->set_text(cfg.netPort);
    }

    _fragLimit->set_visible(cfg.netFragLimit);
    _dmflags->set_visible(cfg.netFlags);
    _dmflags2->set_visible(cfg.netFlags);
    _savegame->set_visible(cfg.netSavegame);
    _saveClash->set_visible(cfg.netSavegame && !cfg.savegame.empty() && cfg.saveEnabled
                           && !cfg.saveFile.empty());

    if (_fragLimit->text() != cfg.fragLimit) {
        _fragLimit->set_text(cfg.fragLimit);
    }

    if (_timeLimit->text() != cfg.timeLimit) {
        _timeLimit->set_text(cfg.timeLimit);
    }

    if (_dmflags->text() != cfg.dmflags) {
        _dmflags->set_text(cfg.dmflags);
    }

    if (_dmflags2->text() != cfg.dmflags2) {
        _dmflags2->set_text(cfg.dmflags2);
    }

    if (_savegame->text() != cfg.savegame) {
        _savegame->set_text(cfg.savegame);
    }

    const bool tunable = cfg.netRole != NetRole::Alone && !unsupported
        && (cfg.netExtratic || cfg.hasNetmode || cfg.netDup);

    const bool turned = State::get().nav.tuning;

    _tuning->set_visible(tunable);

    _tuningHead->set_open(turned);
    _tuningHead->set_said(turned ? std::string() : tuningSummary());

    _netmode->parent()->set_visible(cfg.hasNetmode);
    _netmode->set_current(std::clamp(cfg.netmode, NETMODE_PORT, 1));

    _dup->set_visible(cfg.netDup);
    _dup->set_value(cfg.dup);

    _extratic->parent()->set_visible(cfg.netExtratic);
    _extratic->set_current(cfg.extratic ? 1 : 0);
}

}
