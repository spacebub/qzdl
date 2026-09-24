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
#include <thread>
#include <utility>

#include "ttk/toolkit/layout/Scroll.h"

#include "gui/app/Views.h"
#include "support/Rig.h"

namespace bench {

Rig &shared() {
    static Canvas canvas(1280, 800);
    static Rig made(canvas);

    return made;
}

Rig::Rig(Canvas &canvas)
    : _canvas(canvas),
      _art(&_shell),
      _runs(&_shell),
      _config(&_shell, &_notifier, &_runs),
      _files(&_notifier),
      _engines(&_shell, &_notifier),
      _reach{
          .shell = _shell,
          .config = _config,
          .notify = _notifier,
          .runs = _runs,
          .files = _files,
          .engines = _engines,
          .art = _art,
          .touch = [this] { _dirty = true; },
          .go = [this](const State::Page page) { go(page); },
          .cycleShade = [] {},
          .ask = [](const std::string &, const std::string &, const std::string &, bool,
                    std::function<void()> accepted) {
              if (accepted) {
                  accepted();
              }
          },
          .prompt = [](const std::string &, const std::string &, const std::string &,
                       const std::string &, const std::function<void(const std::string &)> &) {},
          .edit = [](const std::string &, dialogs::EntryDialog::Kind,
                     const std::vector<std::string> &,
                     const std::string &, const std::string &, const std::string &, bool, bool,
                     const std::function<void(const std::string &, const std::string &,
                                              bool)> &) {},
          .showAbout = [] {},
          .showCommand = [] {},
          .copyConfig = [] {},
      } {
    _notifier.changed = [] { State::get().touch(); };

    State::get().changed = [this] { _dirty = true; };

    // A run must not depend on the network, or spend the hour's GitHub limit on
    // whichever benchmark happens to sync the engines page first.
    _engines.setOffline(true);

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

    components::Frame *frame = root.content()->append(
        std::make_unique<components::Frame>(_bar, _pages, _logs));

    frame->add(std::move(bar));
    frame->add(std::move(pages));
    frame->add(std::move(logs));

    _library = _pages->append(std::make_unique<pages::LibraryPage>(&_reach));

    _dialogs = root.layer(ttk::Root::DIALOGS)->append(std::make_unique<ttk::DialogLayer>());

    _toasts = root.layer(ttk::Root::NOTICES)
                  ->append(std::make_unique<ttk::Toasts>([this](const int id) {
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
    State::get().sys.page = page;

    _dirty = true;
}

namespace {

void rewind(ttk::Widget *who) {
    if (auto *scroll = dynamic_cast<ttk::Scroll *>(who); scroll != nullptr) {
        scroll->scroll_to(0.0);
    }

    for (const ttk::Widget::Ptr &child : who->children()) {
        rewind(child.get());
    }
}

}

void Rig::forget() {
    _config.library().setFilter(std::string());

    State::get().nav.shelf = State::Shelf::Profiles;

    _canvas.ui().leave();

    rewind(_canvas.ui().content());

    // Every tween run down and every region dropped, so the next benchmark is not
    // measuring what the last one left mid-flight.
    _canvas.ui().set_now(_canvas.now());

    for (int at = 0; at < 4; ++at) {
        _canvas.ui().advance(_canvas.now());
    }

    _canvas.ui().take();
    _canvas.ui().take_shifts();
}

void Rig::ready() {
    // Title screens are read on a thread of their own, so without this a benchmark
    // times whichever cards happened to have one by the time it started.
    for (int at = 0; at < 400; ++at) {
        const int was = _art.revision();

        sync();
        _canvas.full();

        if (_art.revision() == was) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    sync();
    _canvas.full();

    _canvas.ui().take();
    _canvas.ui().take_shifts();
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

    views::syncAll(views::Tree{.bar = _bar,
                               .logs = _logs,
                               .dialogs = _dialogs,
                               .library = _library,
                               .profile = _profile,
                               .engines = _enginesView,
                               .settings = _settings},
                   page);
}

}
