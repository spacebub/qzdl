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
#include <utility>
#include <vector>

#include "gui/components/Parts.h"
#include "gui/components/RunPanel.h"
#include "gui/draw/Glyphs.h"
#include "gui/draw/Theme.h"
#include "gui/state/State.h"
#include "gui/toolkit/layout/Box.h"
#include "gui/toolkit/layout/Pair.h"
#include "gui/toolkit/layout/Rule.h"
#include "gui/util/Desktop.h"

namespace {

constexpr std::array<std::string_view, 5> SKILLS = {"V. Easy", "Easy", "Medium", "Hard",
                                                   "V. Hard"};
constexpr std::array<std::string_view, 4> MONSTERS = {"No monsters", "Fast", "Respawn",
                                                      "Fast & respawn"};

int indexOf(const std::vector<std::string> &list, const std::string_view wanted) {
    const auto found = std::ranges::find(list, wanted);

    return found == list.end() ? -1 : static_cast<int>(std::distance(list.begin(), found));
}

}

namespace components {

using namespace toolkit;

RunPanel::RunPanel(Reach *reach) : _reach(reach) {
    Box *into = append(Box::column());

    into->pad(16.0)->spacing(14.0);

    panelTitle(into, "The run");

    _addPort = into->append(std::make_unique<Button>("Add a source port", [this] {
        _reach->go(State::Page::Engines);
    }));

    _addPort->glyph(Glyphs::Glyph::Plus)->tooltip("Ports are set up on the Engines page");

    _port = into->append(std::make_unique<Select>("Source port", [this](const int index) {
        const std::vector<std::string> &names = State::get().cfg.portNames;

        _reach->config.profile().setPort(
            index < 0 || std::cmp_greater_equal(index, names.size())
                ? std::string()
                : names[static_cast<size_t>(index)]);
    }));

    _port->placeholder("None selected")->clearable()
        ->tooltip("What actually runs. Add ports on the Engines page.");

    _addGame = into->append(std::make_unique<Button>("Add a game", [this] {
        State::get().nav.shelf = State::Shelf::Games;

        _reach->go(State::Page::Library);
    }));

    _addGame->glyph(Glyphs::Glyph::Plus)->tooltip("Games are added on the library's games shelf");

    _iwad = into->append(std::make_unique<Select>("Game", [this](const int index) {
        const std::vector<std::string> &names = State::get().cfg.iwadNames;

        _reach->config.profile().setIwad(
            index < 0 || std::cmp_greater_equal(index, names.size())
                ? std::string()
                : names[static_cast<size_t>(index)]);
    }));

    _iwad->placeholder("None selected")->clearable()
        ->tooltip("The IWAD itself. Add games from the library.");

    _map = into->append(std::make_unique<Select>("Map", [this](const int index) {
        const std::vector<std::string> &maps = State::get().cfg.maps;

        _reach->config.profile().setWarp(
            index < 0 || std::cmp_greater_equal(index, maps.size())
                ? std::string()
                : maps[static_cast<size_t>(index)]);
    }));

    _map->clearable()->tooltip("Read out of the game and everything loaded on top of it");

    // Half the row each, whatever the panel has come down to: a width taken off
    // the page's own runs the second one off the edge as soon as it is narrower.
    Box *pair = into->append(std::make_unique<Pair>(0.0));

    pair->spacing(12.0);

    _skill = pair->append(std::make_unique<Select>("Skill", [this](const int index) {
        _reach->config.profile().setSkill(index + 1);
    }));

    _skill->clearable();
    _skill->setOptions(std::vector<std::string>(SKILLS.begin(), SKILLS.end()));

    _monsters = pair->append(std::make_unique<Select>("Monsters", [this](const int index) {
        _reach->config.profile().setMonsters(index + 1);
    }));

    _monsters->clearable();
    _monsters->setOptions(std::vector<std::string>(MONSTERS.begin(), MONSTERS.end()));

    into->append(std::make_unique<Rule>());

    _capture = into->append(std::make_unique<Toggle>("Log the game's output",
                                                     [this](const bool on) {
        _reach->config.profile().setCaptureOutput(on);
    }));

    _fullscreen = into->append(std::make_unique<Toggle>("Full screen", [this](const bool on) {
        _reach->config.profile().setDosFullscreen(on);
    }));

    _fullscreen->hint = "Give DOSBox the whole screen rather than a window";

    _levelstat = into->append(std::make_unique<Toggle>("Level stats", [this](const bool on) {
        _reach->config.profile().setLevelstat(on);
    }));

    _levelstat->hint = "Writes a levelstat.txt with the time taken on each map, which is what a "
                       "run is submitted with";

    _sharedConfig = into->append(std::make_unique<Toggle>("Use the port's config",
                                                          [this](const bool on) {
        _reach->config.profile().setSharedConfig(on);
    }));

    _sharedConfig->hint = "Launch on the settings the source port keeps for itself, shared with "
                          "everything else that uses them";

    _directory = into->append(std::make_unique<Fact>("Directory", ""));
    _directory->path()->onClick("Show in file explorer", [] {
        const std::string where = State::get().cfg.profileDirectory;
        std::error_code made;

        std::filesystem::create_directories(std::filesystem::path(where), made);

        Desktop::open(where);
    });
}

void RunPanel::sync() const {
    const State::Cfg &cfg = State::get().cfg;

    _addPort->setVisible(cfg.ports.empty());
    _port->setVisible(!cfg.ports.empty());

    if (_port->visible()) {
        _port->setOptions(cfg.portNames);
        _port->setBadges(cfg.portBadges);
        _port->setCurrent(indexOf(cfg.portNames, cfg.port));
    }

    _addGame->setVisible(cfg.iwads.empty());
    _iwad->setVisible(!cfg.iwads.empty());

    if (_iwad->visible()) {
        _iwad->setOptions(cfg.iwadNames);
        _iwad->setCurrent(indexOf(cfg.iwadNames, cfg.iwad));
    }

    _map->setOptions(cfg.maps);
    _map->setCurrent(indexOf(cfg.maps, cfg.warp));

    _skill->setCurrent(cfg.skill - 1);
    _monsters->setCurrent(cfg.monsters - 1);

    // A DOS port prints into DOSBox's own window.
    _capture->setVisible(!cfg.dosPort);
    _capture->setChecked(cfg.captureOutput && !cfg.autoClose);
    _capture->setEnabled(!cfg.autoClose);
    _capture->hint = cfg.autoClose
        ? "Nothing to record while ZDL4 closes on launch: the log goes with the window. Turn "
          "that off in Settings."
        : "Takes what the source port prints into a log along the bottom of the window. Off, "
          "nothing is piped at all.";

    _fullscreen->setVisible(cfg.dosPort);
    _fullscreen->setChecked(cfg.dosFullscreen);

    _levelstat->setVisible(cfg.hasLevelstat);
    _levelstat->setChecked(cfg.levelstat);

    _sharedConfig->setVisible(cfg.profileConfigs && !cfg.dosPort);
    _sharedConfig->setChecked(cfg.sharedConfig);

    _directory->setValue(cfg.profileDirectory);
}

}
