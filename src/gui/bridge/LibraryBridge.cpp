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

#include <utility>

#include "core/launch/Command.h"
#include "core/util/Text.h"
#include "gui/Convert.h"
#include "gui/Models.h"
#include "gui/bridge/ConfigBridge.h"
#include "gui/bridge/LibraryBridge.h"

namespace {

// A port the list no longer has counts as unset.
std::string gamePortName(const Config &config) {
    const std::string &chosen = config.general.gamePort;

    return !chosen.empty() && config.findPort(chosen) != nullptr
        ? chosen
        : config.activeProfile().port;
}

// A minimal config for a library launch; copying the whole would carry every file list.
Config oneGame(const Config &config, const std::string &iwad) {
    const Profile &active = config.activeProfile();
    Config made;
    Profile target;

    made.general = config.general;
    made.iwads = config.iwads;
    made.ports = config.ports;

    // DOS staging needs an id even with no profile open.
    target.id = active.id.empty() ? "shelf" : active.id;
    target.name = active.name;
    target.config = active.config;
    target.port = gamePortName(config);
    target.iwad = iwad;
    target.sharedConfig = true;

    made.activeProfileId = target.id;
    made.profiles.push_back(std::move(target));

    return made;
}

bool contains(const std::string &value, const std::string &needle) {
    return Text::lower(value).contains(Text::lower(needle));
}

}

void LibraryBridge::pushShelf() const {
    std::vector<ui::ProfileCard> profiles;
    std::vector<ui::NameRow> games;

    for (size_t index = 0; index < config().profiles.size(); ++index) {
        if (_filter.empty() || contains(config().profiles[index].name, _filter)) {
            profiles.push_back(ProfileBridge::cardOf(static_cast<int>(index)));
        }
    }

    for (size_t index = 0; index < config().iwads.size(); ++index) {
        if (_filter.empty() || contains(config().iwads[index].name, _filter)) {
            games.push_back(ListsBridge::rowOf(config().iwads, static_cast<int>(index), false));
        }
    }

    Models::reconcile(*_shelfProfiles, profiles);
    Models::reconcile(*_shelfGames, games);
}

void LibraryBridge::pushGameRev() {
    std::string mark = config().general.gamePort + '\n' + config().general.alwaysAdd + '\n'
        + config().general.dosbox + '\n' + active().port + '\n' + active().id;

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
    cfg().set_game_rev(++_gameRev);
}

void LibraryBridge::bind() {
    const ui::Cfg &state = cfg();

    state.set_shelf_profiles(_shelfProfiles);
    state.set_shelf_games(_shelfGames);

    state.on_set_filter([this](const slint::SharedString &value) {
        _filter = Convert::plain(value);

        cfg().set_filter(value);
        pushShelf();
    });

    state.on_game_key([](const slint::SharedString &iwad) {
        return Convert::text(ConfigBridge::gameKey(Convert::plain(iwad)));
    });

    state.on_game_command_line([](int, const slint::SharedString &iwad) {
        return Convert::text(Command::line(oneGame(config(), Convert::plain(iwad))));
    });

    state.on_launch_game([this](const slint::SharedString &iwad) {
        const std::string name = Convert::plain(iwad);

        _hub->start(ConfigBridge::gameKey(name), name, oneGame(config(), name));
    });
}
