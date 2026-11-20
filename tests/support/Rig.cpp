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

#include <chrono>
#include <memory>
#include <span>
#include <thread>
#include <utility>

#include "core/config/Session.h"
#include "core/ports/Detect.h"
#include "gui/app/Views.h"
#include "support/Rig.h"

namespace tests {

Rig::Fresh::Fresh() {
    Session::get().config() = Config{};
    State::get() = State::All{};
}

Rig::Rig(Canvas &canvas)
    : _canvas(canvas),
      _art(&_shell),
      _runs(&_shell),
      _config(&_shell, &_notifier, &_runs),
      _files(&_notifier),
      _downloads(&_shell, &_notifier),
      _reach{
              .shell = _shell,
              .config = _config,
              .notify = _notifier,
              .runs = _runs,
              .files = _files,
              .downloads = _downloads,
              .art = _art,
              .touch = [this] { _dirty = true; },
              .go = [this](const State::Page page) { go(page); },
              .cycleShade = [this] { ++shadeCycles; },
              .ask =
                      [this](const std::string &title,
                             const std::string &body,
                             const std::string &accept,
                             const bool danger,
                             std::function<void()> accepted) {
                          asks.push_back({
                                  .title = title,
                                  .body = body,
                                  .accept = accept,
                                  .danger = danger,
                                  .accepted = std::move(accepted),
                          });
                      },
              .prompt =
                      [this](const std::string &title,
                             const std::string &label,
                             const std::string &value,
                             const std::string &accept,
                             std::function<void(const std::string &)> accepted) {
                          prompts.push_back({
                                  .title = title,
                                  .label = label,
                                  .value = value,
                                  .accept = accept,
                                  .accepted = std::move(accepted),
                          });
                      },
              .editGame =
                      [this](const std::string &title,
                             const GameEntry &game,
                             std::function<void(const GameEntry &)> accepted) {
                          gameEdits.push_back({.title = title, .game = game, .accepted = std::move(accepted)});
                      },
              .editEngine =
                      [this](const std::string &title,
                             const Engine &engine,
                             const bool adding,
                             std::function<void(const Engine &)> accepted) {
                          engineEdits.push_back({.title = title,
                                                 .engine = engine,
                                                 .adding = adding,
                                                 .accepted = std::move(accepted)});
                      },
              .editEnvironment =
                      [this](const std::vector<EnvVariable> &variables,
                             std::function<void(std::vector<EnvVariable>)> accepted) {
                          environmentEdits.push_back({.variables = variables, .accepted = std::move(accepted)});
                      },
              .editCommand = [this] { ++commandEdits; },
              .editFlags = [this](const Flags::Family family) { flagEdits.push_back(family); },
              .showAbout = [this] { ++aboutShown; },
              .showCommand = [this] { ++commandShown; },
              .copyConfig = [this] { ++copyConfigShown; },
      } {
    _notifier.changed = [] { State::get().touch(); };

    State::get().changed = [this] { _dirty = true; };

    // As App::wireServices and App::wireConfig, less what ends the loop.
    _downloads.addEngine = [this](const Engine &engine) { return _config.lists().addEngine(engine); };

    _downloads.updateEngine = [this](const int at, const std::string &name, const std::string &file, const bool dos) {
        Engine edited = Session::get().config().engines.at(static_cast<size_t>(at));

        edited.name = name;
        edited.file = file;
        edited.dosbox = dos;

        _config.lists().updateEngine(at, edited);
    };

    _downloads.removeEngine = [this](const int at) { _config.lists().removeEngine(at); };

    _downloads.scheduleSave = [this] { _config.scheduleSave(); };

    _downloads.storeVersions = [this](const std::span<const Detect::Installed> read) {
        _config.lists().storeVersions(read);
    };

    _art.arrived = [this] {
        State::get().sys.artRev = _art.revision();

        _dirty = true;
    };

    _config.replaced = [this](const bool detect) {
        _downloads.relist();
        _downloads.opened(detect);
    };

    State::System &sys = State::get().sys;

    sys.version = QZDL_VERSION;
    sys.runtime = "tinytk " QZDL_TINYTK_VERSION;

    // A case must not depend on the network, or spend the hour's GitHub limit.
    _downloads.setOffline(true);

    build();
}

Rig::~Rig() {
    State::get().changed = nullptr;
}

void Rig::build() {
    ttk::Root &root = _canvas.ui();

    auto bar = std::make_unique<components::TitleBar>(&_reach);
    auto pages = std::make_unique<ttk::Widget>();
    auto logs = std::make_unique<components::LogDock>(&_reach);

    _bar = bar.get();
    _pages = pages.get();
    _logs = logs.get();

    _frame = root.content()->append(std::make_unique<components::Frame>(_bar, _pages, _logs));

    _frame->add(std::move(bar));
    _frame->add(std::move(pages));
    _frame->add(std::move(logs));

    _library = _pages->append(std::make_unique<pages::LibraryPage>(&_reach));

    _dialogs = root.layer(ttk::Root::DIALOGS)->append(std::make_unique<ttk::DialogLayer>());

    _toasts = root.layer(ttk::Root::NOTICES)->append(std::make_unique<ttk::Toasts>([this](const int id) {
        _notifier.dismiss(id);
    }));

    _tips = root.layer(ttk::Root::TIPS)->append(std::make_unique<ttk::Tips>());
}

pages::ProfilePage &Rig::profile() {
    if (_profile == nullptr) {
        _profile = _pages->append(std::make_unique<pages::ProfilePage>(&_reach));
    }

    return *_profile;
}

pages::EnginesPage &Rig::enginesPage() {
    if (_enginesView == nullptr) {
        _enginesView = _pages->append(std::make_unique<pages::EnginesPage>(&_reach));
    }

    return *_enginesView;
}

pages::SettingsPage &Rig::settings() {
    if (_settings == nullptr) {
        _settings = _pages->append(std::make_unique<pages::SettingsPage>(&_reach));
    }

    return *_settings;
}

void Rig::go(const State::Page page) {
    visited.push_back(page);

    // As App::go, less the history.
    if (page == State::get().sys.page) {
        return;
    }

    State::get().sys.page = page;

    _dirty = true;
}

std::size_t Rig::turn(const double seconds) {
    _shell.advance(seconds);

    if (_dirty) {
        sync();
    }

    return _canvas.frameAt(_canvas.now() + seconds);
}

void Rig::ready() {
    // Title screens are read on a thread of their own and arrive as posts, as does
    // the start-up detection. Without this a case sees whichever had landed by the
    // time it looked.
    for (int at = 0; at < 400; ++at) {
        const int was = _art.revision();

        _shell.advance(0.0);
        sync();
        _canvas.full();

        // A read still on the art thread has posted nothing yet.
        if (_art.revision() == was && _shell.posted() == 0 && !_art.reading()) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    _shell.advance(0.0);
    sync();
    _canvas.full();
}

void Rig::sync() {
    _dirty = false;

    const State::Page page = State::get().sys.page;

    if (page == State::Page::Profile) {
        profile();
    } else if (page == State::Page::Engines) {
        enginesPage();
    } else if (page == State::Page::Settings) {
        settings();
    }

    views::syncAll(
            views::Tree{
                    .bar = _bar,
                    .logs = _logs,
                    .dialogs = _dialogs,
                    .library = _library,
                    .profile = _profile,
                    .engines = _enginesView,
                    .settings = _settings,
            },
            page);
}

}
