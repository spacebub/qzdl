/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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
#include <string>
#include <system_error>

#include "ttk/draw/Glyphs.h"
#include "ttk/draw/Theme.h"
#include "ttk/toolkit/layout/Rule.h"
#include "ttk/util/Desktop.h"

#include "core/config/Profile.h"
#include "gui/components/GeneralPanel.h"
#include "gui/services/Filters.h"
#include "gui/state/State.h"

using namespace ttk;

namespace components {

GeneralPanel::GeneralPanel(Reach *reach)
    : CollapsiblePanel("General", [reach](const bool open) { reach->config.panels().setGeneralOpen(open); }),
      _reach(reach) {
    Box *body = CollapsiblePanel::body();

    body->append(std::make_unique<Rule>());

    const auto group = [body](const char *caption) {
        Box *made = body->append(Box::column());

        made->spacing(6.0)->cross(Box::Place::Start);
        made->append(std::make_unique<Label>(caption))->section();

        return made;
    };

    const auto switches = [](Box *into) {
        Box *row = into->append(Box::row());

        row->spacing(16.0);

        return row;
    };

    _game = group("Game");

    Box *rules = switches(_game);

    _pistolStart = rules->append(std::make_unique<Toggle>(
            "Pistol start", [this](const bool on) { _reach->config.profile().setPistolStart(on); }));

    _cheats = rules->append(std::make_unique<Toggle>(
            "Allow cheats", [this](const bool on) { _reach->config.profile().setCheats(on); }));

    _cheats->hint = "Turns on sv_cheats, which a netgame or the hardest skill needs before any cheat works";

    _levelstat = rules->append(std::make_unique<Toggle>(
            "Level stats", [this](const bool on) { _reach->config.profile().setLevelstat(on); }));

    _levelstat->hint = "Writes a levelstat.txt with the time taken on each map, which is what a run is submitted with";

    _skipAutoload = rules->append(std::make_unique<Toggle>(
            "Skip autoload", [this](const bool on) { _reach->config.profile().setSkipAutoload(on); }));

    _skipAutoload->hint = "Starts without the files the port loads on its own from autoload folders and config lists";

    _sound = group("Sound");

    Box *sound = switches(_sound);

    _music = sound->append(
            std::make_unique<Toggle>("Music", [this](const bool on) { _reach->config.profile().setMusic(on); }));

    _music->hint = "Off starts the port with no music";

    _soundEffects = sound->append(std::make_unique<Toggle>(
            "Sound effects", [this](const bool on) { _reach->config.profile().setSoundEffects(on); }));

    _soundEffects->hint = "Off starts the port with no sound effects";

    _display = group("Display");

    _mode = _display->append(std::make_unique<MultistateSwitch>(
            [this](const int value) { _reach->config.profile().setDisplay(static_cast<Display>(value)); }));

    _mode->set_options({
            {.value = static_cast<int>(Display::Port), .label = "Port"},
            {.value = static_cast<int>(Display::Window), .label = "Window"},
            {.value = static_cast<int>(Display::Fullscreen), .label = "Fullscreen"},
    });

    _screenshots = group("Screenshots");

    _shots = _screenshots->append(std::make_unique<MultistateSwitch>([this](const int value) {
        const auto picked = static_cast<Screenshots>(value);

        _reach->config.profile().setScreenshots(picked);

        // Custom with nowhere to go yet asks for the folder at once.
        if (picked == Screenshots::Custom && State::get().cfg.screenshotPath.empty()) {
            pickFolder();
        }
    }));

    _shots->set_options({
            {.value = static_cast<int>(Screenshots::Port), .label = "Port"},
            {.value = static_cast<int>(Screenshots::Profile), .label = "Profile"},
            {.value = static_cast<int>(Screenshots::Custom), .label = "Custom"},
    });

    _where = _screenshots->append(std::make_unique<Fact>("Directory", ""));
    _where->path()->on_click("Show in file explorer", [] {
        const std::string &folder = State::get().cfg.screenshotPath;
        std::error_code made;

        if (folder.empty()) {
            return;
        }

        std::filesystem::create_directories(std::filesystem::path(folder), made);
        Desktop::open(folder);
    });

    _folder = _screenshots->append(std::make_unique<Field>(
            "Directory", [this](const std::string &value) { _reach->config.profile().setScreenshotFolder(value); }));

    _folder->placeholder("No folder picked yet, so the port picks one")->mono();
    _folder->icon(Glyphs::Glyph::Folder, "Browse", [this] { pickFolder(); });
    _folder->minWidth = 0.0;
    _folder->fixedWidth = 520.0;
}

void GeneralPanel::pickFolder() const {
    _reach->files.open("Select a screenshots folder",
                       {},
                       true,
                       false,
                       false,
                       LastDir::GENERAL,
                       Picked::first([this](const std::string &path) {
                           _reach->config.profile().setScreenshotFolder(path);
                           _reach->touch();
                       }));
}

void GeneralPanel::sync() {
    const State::Cfg &cfg = State::get().cfg;

    set_visible(cfg.hasPistolStart || cfg.hasCheats || cfg.hasLevelstat || cfg.hasNoAutoload || cfg.hasMusic
                || cfg.hasSoundEffects || cfg.hasDisplay || cfg.hasShotdir);

    if (!visible()) {
        return;
    }


    set_open(cfg.generalOpen);
    set_said("", false);

    _game->set_visible(cfg.hasPistolStart || cfg.hasCheats || cfg.hasLevelstat || cfg.hasNoAutoload);
    _pistolStart->set_visible(cfg.hasPistolStart);
    _pistolStart->set_checked(cfg.pistolStart);
    _pistolStart->hint = cfg.pistolStartSwitch
                                 ? "Starts every map with only the pistol. Left out while a demo is recorded or "
                                   "played and in a netgame, since the port stops on it there"
                                 : "Starts every map with only the pistol";
    _cheats->set_visible(cfg.hasCheats);
    _cheats->set_checked(cfg.cheats);
    _levelstat->set_visible(cfg.hasLevelstat);
    _levelstat->set_checked(cfg.levelstat);
    _skipAutoload->set_visible(cfg.hasNoAutoload);
    _skipAutoload->set_checked(cfg.skipAutoload);

    _sound->set_visible(cfg.hasMusic || cfg.hasSoundEffects);
    _music->set_visible(cfg.hasMusic);
    _music->set_checked(cfg.music);
    _soundEffects->set_visible(cfg.hasSoundEffects);
    _soundEffects->set_checked(cfg.soundEffects);

    _display->set_visible(cfg.hasDisplay);
    _mode->set_current(static_cast<int>(cfg.display));
    _mode->hint = cfg.displayKept ? "Window or fullscreen for this profile. The port keeps the choice in its settings, "
                                    "and Port runs it as it last ran"
                                  : "Window or fullscreen for this profile. Port runs it as the port is set";

    _screenshots->set_visible(cfg.hasShotdir);
    _shots->set_current(static_cast<int>(cfg.screenshots));

    // The port keeps its own under Port, so there is no directory to show.
    _where->set_visible(cfg.screenshots == Screenshots::Profile);
    _where->set_value(cfg.screenshotPath);
    _folder->set_visible(cfg.screenshots == Screenshots::Custom);

    if (_folder->text() != cfg.screenshotFolder) {
        _folder->set_text(cfg.screenshotFolder);
    }
}

}
