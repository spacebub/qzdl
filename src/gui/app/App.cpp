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
#include <cstdio>

#include "core/config/Schema.h"
#include "core/config/Session.h"
#include "gui/app/App.h"
#include "gui/components/Frame.h"
#include "gui/components/LogDock.h"
#include "gui/components/TitleBar.h"
#include "gui/components/Toasts.h"
#include "gui/components/sheets/AboutSheet.h"
#include "gui/components/sheets/CommandSheet.h"
#include "gui/components/sheets/ConfirmSheet.h"
#include "gui/components/sheets/CopyConfigSheet.h"
#include "gui/components/sheets/EntrySheet.h"
#include "gui/components/sheets/PickSheet.h"
#include "gui/components/sheets/PromptSheet.h"
#include "gui/components/sheets/SheetLayer.h"
#include "gui/pages/Engines.h"
#include "gui/pages/Library.h"
#include "gui/pages/Profile.h"
#include "gui/pages/Settings.h"
#include "gui/toolkit/overlays/Tips.h"
#include "qzdl_git_revision.h"

namespace {

// A window wider than this has no surface to draw on.
constexpr int LARGEST_WINDOW = 16384;

std::vector<std::string> listOf(const std::initializer_list<const char *> names) {
    return {names.begin(), names.end()};
}

}

const std::vector<std::string> &App::wadFilters() {
    static const std::vector<std::string> held = listOf({
        "*.wad", "*.pwad", "*.iwad", "*.pk3", "*.pk7", "*.pkz", "*.pke", "*.ipk3", "*.ipk7",
        "*.zip", "*.7z", "*.deh", "*.bex", "*.lmp", "*.cfg",
    });

    return held;
}

const std::vector<std::string> &App::portFilters() {
#ifdef _WIN32
    static const std::vector<std::string> held = listOf({"*.exe"});
#else
    static const std::vector<std::string> held = listOf({"*"});
#endif

    return held;
}

const std::vector<std::string> &App::zdlFilters() {
    static const std::vector<std::string> held = listOf({"*.zdl"});

    return held;
}

const std::vector<std::string> &App::configFilters() {
    static const std::vector<std::string> held = listOf({"*.json", "*.ini"});

    return held;
}

const std::vector<std::string> &App::saveFilters() {
    static const std::vector<std::string> held =
        listOf({"*.zds", "*.dsg", "*.esg", "*.sav", "*.save"});

    return held;
}

const std::vector<std::string> &App::replayFilters() {
    static const std::vector<std::string> held = listOf({"*.lmp"});

    return held;
}

App::App()
    : _art(&_shell),
      _runs(&_shell),
      _config(&_shell, &_notifier, &_runs),
      _picker(&_notifier,
              [this](const std::string &action, const std::vector<std::string> &paths,
                     const bool option) { picked(action, paths, option); }),
      _engines(&_shell, &_notifier, &_config) {
    IwadArt::prune();

    // A new title screen makes the cards re-ask.
    _art.arrived = [this] {
        State::get().sys.artRev = _art.revision();

        touch();
    };

    _config.replaced = [this](const bool detect) {
        _engines.relist();

        if (detect) {
            _engines.discover();
        }
    };

    // Hiding the window ends the loop, so the config is written first.
    _config.launched = [this] {
        if (Session::get().config().general.autoClose) {
            persist();
            _shell.stop();
        }
    };

    State::get().changed = [this] { _dirty = true; };
}

App::~App() = default;

bool App::start() {
    if (!_shell.start(1180, 760)) {
        return false;
    }

    State::System &sys = State::get().sys;

    sys.version = QZDL_VERSION;

    if (*QZDL_GIT_REVISION != '\0') {
        sys.version += " (" QZDL_GIT_REVISION ")";
    }

    sys.runtime = "Blend2D";

#ifdef _WIN32
    sys.windows = true;
#else
    sys.windows = false;
#endif

    sys.gpu = Session::get().config().general.hardwareRendering;

    sys.wadFilters = wadFilters();
    sys.portFilters = portFilters();
    sys.zdlFilters = zdlFilters();
    sys.configFilters = configFilters();
    sys.saveFilters = saveFilters();
    sys.replayFilters = replayFilters();

    const std::string saved = Session::get().config().general.theme;

    Theme::setMode(saved == ThemeMode::LIGHT || saved == ThemeMode::DARK ? saved
                                                                        : ThemeMode::SYSTEM);

    State::get().nav.shelf = Session::get().config().general.startView == StartView::GAMES
        ? "games"
        : "profiles";

    build();
    restoreGeometry();

    Shell::setOutline(Theme::of().borderStrong);

    return true;
}

void App::build() {
    toolkit::Root &root = _shell.ui();

    // Built first, then handed to the frame that places them.
    auto bar = std::make_unique<components::TitleBar>(this);
    auto pages = std::make_unique<toolkit::Widget>();
    auto logs = std::make_unique<components::LogDock>(this);

    _bar = bar.get();
    _pages = pages.get();
    _logs = logs.get();

    components::Frame *frame = root.content()->append(
        std::make_unique<components::Frame>(_bar, _pages, _logs));

    frame->add(std::move(bar));
    frame->add(std::move(pages));
    frame->add(std::move(logs));

    _library = _pages->append(std::make_unique<pages::LibraryPage>(this));

    _sheets = root.layer(toolkit::Root::SHEETS)->append(std::make_unique<components::SheetLayer>());

    _sheets->closed = [this](toolkit::Sheet *gone) {
        if (gone == _entry) {
            _entry = nullptr;
        }

        if (gone == _pick) {
            _pick = nullptr;
        }
    };

    _toasts = root.layer(toolkit::Root::NOTICES)
                  ->append(std::make_unique<components::Toasts>([this](const int id) {
                      _notifier.dismiss(id);
                  }));

    _tips = root.layer(toolkit::Root::TIPS)->append(std::make_unique<toolkit::Tips>());

    _shell.draggable = [this](const double x, const double y) {
        return !_sheets->covered() && !_shell.ui().hasDismiss() && _bar->draggable(x, y);
    };

    _shell.closing = [this] { persist(); };

    _shell.back = [this] { back(); };
    _shell.forward = [this] { forward(); };

    _shell.shortcut = [this](const toolkit::Key &pressed) { return shortcut(pressed); };

    _shell.shadeChanged = [this] {
        Shell::setOutline(Theme::of().borderStrong);
        _shell.ui().damageAll();

        touch();
    };

    _shell.resized = [this](const double, const double) { _shell.ui().relayout(); };
}

void App::run() {
    _engines.relist();

    // Once per turn of the loop rather than on a timer: with nothing in flight the
    // loop blocks, and an idle window costs nothing.
    _shell.settle = [this] {
        if (_dirty) {
            _dirty = false;

            sync();
        }

        const toolkit::Widget *over = _shell.ui().hovered();
        const double x = _shell.ui().pointerX();
        const double y = _shell.ui().pointerY();

        if (over != nullptr && !over->hint.empty()) {
            _tips->point(over->hint, over->box(), x, y, Shell::now());
        } else {
            _tips->point({}, BLRect{}, x, y, Shell::now());
        }

        _toasts->setMessages(_notifier.messages());
    };

    _shell.run();
}

void App::sync() {
    if (const bool wants = State::get().pick.open; wants != (_pick != nullptr)) {
        if (wants) {
            _pick = _sheets->show(std::make_unique<components::PickSheet>(_picker));
        } else if (_sheets->top() == _pick) {
            _sheets->dismiss();
        } else {
            _pick = nullptr;
        }
    }

    _sheets->sync();

    const std::string &page = State::get().sys.page;

    _bar->sync();
    _logs->sync();

    _library->setVisible(page == "library");
    _library->sync();

    if (page == "profile") {
        _sawProfile = true;
    } else if (page == "engines") {
        _sawEngines = true;
    } else if (page == "settings") {
        _sawSettings = true;
    }

    if (_sawProfile && _profile == nullptr) {
        _profile = _pages->append(std::make_unique<pages::ProfilePage>(this));
    }

    if (_sawEngines && _enginesView == nullptr) {
        _enginesView = _pages->append(std::make_unique<pages::EnginesPage>(this));
    }

    if (_sawSettings && _settings == nullptr) {
        _settings = _pages->append(std::make_unique<pages::SettingsPage>(this));
    }

    if (_profile != nullptr) {
        _profile->setVisible(page == "profile");
        _profile->sync();
    }

    if (_enginesView != nullptr) {
        _enginesView->setVisible(page == "engines");
        _enginesView->sync();
    }

    if (_settings != nullptr) {
        _settings->setVisible(page == "settings");
        _settings->sync();
    }
}

void App::touch() {
    _dirty = true;
}

void App::go(const std::string &page) {
    State::System &sys = State::get().sys;

    if (page == sys.page) {
        return;
    }

    _history.push_back(sys.page);

    if (_history.size() > HISTORY) {
        _history.erase(_history.begin());
    }

    _ahead.clear();

    sys.page = page;

    touch();
}

void App::back() {
    if (_history.empty()) {
        return;
    }

    State::System &sys = State::get().sys;

    _ahead.push_back(sys.page);
    sys.page = _history.back();
    _history.pop_back();

    touch();
}

void App::forward() {
    if (_ahead.empty()) {
        return;
    }

    State::System &sys = State::get().sys;

    _history.push_back(sys.page);
    sys.page = _ahead.back();
    _ahead.pop_back();

    touch();
}

// --- the sheets ----------------------------------------------------------------

void App::ask(const std::string &title, const std::string &body,
              const std::string &accept, const bool danger, std::function<void()> accepted) {
    _sheets->show(std::make_unique<components::ConfirmSheet>(title, body, accept, danger,
                                                     [this, accepted = std::move(accepted)] {
        if (accepted) {
            accepted();
        }

        touch();
    }));
}

void App::prompt(const std::string &title, const std::string &label, const std::string &value,
                 const std::string &accept,
                 std::function<void(const std::string &)> accepted) {
    _sheets->show(std::make_unique<components::PromptSheet>(
        title, label, value, accept,
        [this, accepted = std::move(accepted)](const std::string &typed) {
            if (accepted) {
                accepted(typed);
            }

            touch();
        }));
}

void App::edit(const std::string &title, const std::string &kind,
               const std::vector<std::string> &filters, const std::string &remember,
               const std::string &name, const std::string &file, const bool offerDos,
               const bool dosbox,
               std::function<void(const std::string &, const std::string &, bool)> accepted) {
    auto made = std::make_unique<components::EntrySheet>(
        title, kind, filters, remember, name, file, offerDos, dosbox, _picker,
        [this, accepted = std::move(accepted)](const std::string &named,
                                               const std::string &path, const bool dos) {
            if (accepted) {
                accepted(named, path, dos);
            }

            touch();
        });

    _entry = made.get();

    _sheets->show(std::move(made));
}

void App::showAbout() {
    _sheets->show(std::make_unique<components::AboutSheet>());
}

void App::showCommand() {
    _sheets->show(std::make_unique<components::CommandSheet>([this] {
        _notifier.success("The command line is on the clipboard.");
    }));
}

void App::copyConfig() {
    ProfileBridge::pushConfigDonors();

    _sheets->show(std::make_unique<components::CopyConfigSheet>([this](const std::string &id) {
        _config.profile().copyEngineConfig(id);

        touch();
    }));
}

bool App::covered() const {
    return _sheets != nullptr && _sheets->covered();
}

void App::dismissTop() {
    if (_sheets != nullptr) {
        _sheets->close();
    }
}

void App::cycleShade() {
    const std::string next = Theme::nextMode();

    Theme::setMode(next);

    Session::get().config().general.theme = next;
    Session::get().save();

    Shell::setOutline(Theme::of().borderStrong);
    _shell.ui().damageAll();

    touch();
}

bool App::shortcut(const toolkit::Key &pressed) {
    if (pressed.code == toolkit::Code::Escape) {
        if (covered()) {
            dismissTop();

            return true;
        }

        return false;
    }

    if (pressed.code == toolkit::Code::Return && !covered()
        && State::get().sys.page != "settings" && State::get().sys.page != "engines") {
        _config.profile().launch();

        return true;
    }

    if (pressed.code == toolkit::Code::F1) {
        showAbout();

        touch();

        return true;
    }

    if (pressed.alt && pressed.code == toolkit::Code::Left) {
        back();

        return true;
    }

    if (pressed.alt && pressed.code == toolkit::Code::Right) {
        forward();

        return true;
    }

    if (pressed.code == toolkit::Code::Tab) {
        _shell.ui().focusNext(pressed.shift);

        return true;
    }

    return false;
}

void App::picked(const std::string &action, const std::vector<std::string> &paths,
                 const bool option) {
    if (paths.empty()) {
        return;
    }

    const std::string &first = paths.front();

    if (action == "add-iwads") {
        _config.lists().addIwads(paths);
    } else if (action == "add-files") {
        _config.lists().addFiles(paths);
    } else if (action == "add-port") {
        _config.lists().addPort(first, {}, option);
    } else if (action == "entry-file") {
        if (_entry != nullptr) {
            _entry->setFile(first);
        }
    } else if (action == "dosbox") {
        _config.settings().setDosbox(first);
    } else if (action == "savegame") {
        _config.panels().setSavegame(first);
    } else if (action == "replay") {
        _config.panels().setReplayFile(first);
    } else if (action == "save-zdl") {
        _config.profile().saveZdl(first);
    } else if (action == "load-config") {
        _config.settings().load(first);
    } else if (action == "save-config") {
        _config.settings().saveAs(first);
    } else if (action == "load-zdl") {
        _config.profile().loadZdl(first);
    }

    touch();
}

// --- the window ----------------------------------------------------------------

void App::restoreGeometry() {
    const WindowGeometry &saved = Session::get().config().general.window;

    const int width = saved.hasSize && saved.width > 0
        ? std::clamp(saved.width, 720, LARGEST_WINDOW)
        : 0;
    const int height = saved.hasSize && saved.height > 0
        ? std::clamp(saved.height, 520, LARGEST_WINDOW)
        : 0;

    _shell.setGeometry(saved.hasPosition ? saved.x : -1, saved.hasPosition ? saved.y : -1, width,
                       height);
}

void App::rememberGeometry() {
    WindowGeometry &window = Session::get().config().general.window;

    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    _shell.geometry(x, y, width, height);

    if (width <= 0 || height <= 0) {
        return;
    }

    window.hasPosition = true;
    window.x = x;
    window.y = y;
    window.hasSize = true;
    window.width = width;
    window.height = height;
}

void App::persist() {
    rememberGeometry();
    _config.flush();

    std::string error;

    if (!Session::get().save(&error)) {
        // NOLINTNEXTLINE(cert-err33-c,modernize-use-std-print)
        std::fprintf(stderr, "Could not save the config: %s\n", error.c_str());
    }
}
