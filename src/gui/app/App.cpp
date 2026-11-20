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
#include <cstdio>
#include <filesystem>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "ttk/dialogs/ConfirmDialog.h"
#include "ttk/dialogs/DialogLayer.h"
#include "ttk/dialogs/FilePickerDialog.h"
#include "ttk/dialogs/PromptDialog.h"
#include "ttk/notices/Toasts.h"
#include "ttk/toolkit/controls/StatusIndicator.h"
#include "ttk/toolkit/overlays/Tips.h"

#include "core/config/Schema.h"
#include "core/config/Session.h"
#include "core/launch/Dialect.h"
#include "core/launch/Flags.h"
#include "core/ports/Detect.h"
#include "core/wad/Readmes.h"
#include "gui/app/App.h"
#include "gui/app/Views.h"
#include "gui/components/Frame.h"
#include "gui/components/LogDock.h"
#include "gui/components/TitleBar.h"
#include "gui/dialogs/AboutDialog.h"
#include "gui/dialogs/CommandDialog.h"
#include "gui/dialogs/CopyConfigDialog.h"
#include "gui/dialogs/EditCommandDialog.h"
#include "gui/dialogs/EntryDialog.h"
#include "gui/dialogs/EnvironmentDialog.h"
#include "gui/dialogs/FlagsDialog.h"
#include "gui/dialogs/ReadmeDialog.h"
#include "gui/draw/Hues.h"
#include "gui/draw/Mark.h"
#include "gui/pages/EnginesPage.h"
#include "gui/pages/LibraryPage.h"
#include "gui/pages/ProfilePage.h"
#include "gui/pages/SettingsPage.h"
#include "gui/services/Filters.h"
#include "qzdl_git_revision.h"

using namespace ttk;

namespace {

std::string &lastDir(const std::string &key) {
    // clang-format off
    static constexpr std::array slots{
        std::pair{&LastDir::WAD, &LastDirs::wad},       std::pair{&LastDir::SRC, &LastDirs::src},
        std::pair{&LastDir::SAVE, &LastDirs::save},     std::pair{&LastDir::ZDL, &LastDirs::zdl},
        std::pair{&LastDir::CONFIG, &LastDirs::config}, std::pair{&LastDir::REPLAY, &LastDirs::replay},
    };
    // clang-format on

    LastDirs &dirs = Session::get().config().general.lastDirs;

    for (const auto &[named, held] : slots) {
        if (*named == key) {
            return dirs.*held;
        }
    }

    return dirs.general;
}

// A window wider than this has no surface to draw on.
constexpr int LARGEST_WINDOW = 16384;

const char *getConfigThemeLiteral(const Theme::Mode mode) {
    if (mode == Theme::Mode::Dark) {
        return ThemeMode::DARK;
    }

    if (mode == Theme::Mode::Light) {
        return ThemeMode::LIGHT;
    }

    if (mode == Theme::Mode::System) {
        return ThemeMode::SYSTEM;
    }

    return ThemeMode::SYSTEM;
}

Theme::Mode getModeFromConfigLiteral(const std::string &theme) {
    if (theme == ThemeMode::DARK) {
        return Theme::Mode::Dark;
    }

    if (theme == ThemeMode::LIGHT) {
        return Theme::Mode::Light;
    }

    if (theme == ThemeMode::SYSTEM) {
        return Theme::Mode::System;
    }

    return Theme::Mode::System;
}
}

App::App()
    : _art(&_shell),
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
              // Wired by wireReach(), once the window can answer them.
              .touch = {},
              .go = {},
              .cycleShade = {},
              .ask = {},
              .prompt = {},
              .editGame = {},
              .editEngine = {},
              .editEnvironment = {},
              .editCommand = {},
              .editFlags = {},
              .showAbout = {},
              .showCommand = {},
              .copyConfig = {},
              .showReadmes = {},
      } {
    _notifier.changed = [] { State::get().touch(); };
    _files.changed = [] { State::get().touch(); };

    _files.set_memory(FilePicker::Memory{
            .directory = [](const std::string &key) { return lastDir(key); },
            .remember =
                    [](const std::string &key, const std::string &path) {
                        if (!path.empty()) {
                            lastDir(key) = path;
                        }
                    },
            .hidden = [] { return Session::get().config().general.showHidden; },
            .showHidden = [](const bool shown) { Session::get().config().general.showHidden = shown; },
    });

    StatusIndicator::set_tones(&Hues::statusTones);

    IwadArt::prune();

    wireReach();
    wireServices();
    wireConfig();
}

void App::wireReach() {
    _reach.touch = [this] { touch(); };
    _reach.go = [this](const State::Page page) { go(page); };
    _reach.cycleShade = [this] { cycleShade(); };

    _reach.ask = [this](const std::string &title,
                        const std::string &body,
                        const std::string &accept,
                        const bool danger,
                        std::function<void()> accepted) { ask(title, body, accept, danger, std::move(accepted)); };

    _reach.prompt = [this](const std::string &title,
                           const std::string &label,
                           const std::string &value,
                           const std::string &accept,
                           std::function<void(const std::string &)> accepted) {
        prompt(title, label, value, accept, std::move(accepted));
    };

    _reach.editGame =
            [this](const std::string &title, const GameEntry &game, std::function<void(const GameEntry &)> accepted) {
                editGame(title, game, std::move(accepted));
            };

    _reach.editEngine = [this](const std::string &title,
                               const Engine &engine,
                               const bool adding,
                               std::function<void(const Engine &)> accepted) {
        _dialogs->show(std::make_unique<dialogs::EntryDialog>(
                title, engine, adding, _files, [this, then = std::move(accepted)](const Engine &edited) {
                    if (then) {
                        then(edited);
                    }

                    touch();
                }));
    };

    _reach.editEnvironment = [this](const std::vector<EnvVariable> &variables,
                                    std::function<void(std::vector<EnvVariable>)> accepted) {
        _dialogs->show(std::make_unique<dialogs::EnvironmentDialog>(
                variables, [this, then = std::move(accepted)](std::vector<EnvVariable> edited) {
                    if (then) {
                        then(std::move(edited));
                    }

                    touch();
                }));
    };

    _reach.editCommand = [this] {
        auto accepted = [this](const std::string &edited) {
            _config.profile().setCommand(edited);

            touch();
        };

        _dialogs->show(std::make_unique<dialogs::EditCommandDialog>(State::get().cfg.command, std::move(accepted)));
    };

    _reach.editFlags = [this](const Flags::Family family) {
        const Config &config = Session::get().config();
        auto accepted = [this](const Flags::Edits &edits) {
            _config.panels().setFlags(edits);

            touch();
        };

        _dialogs->show(std::make_unique<dialogs::FlagsDialog>(
                family, Dialect::of(config), config.activeProfile().flags, std::move(accepted)));
    };

    _reach.showAbout = [this] { showAbout(); };
    _reach.showCommand = [this] { showCommand(); };
    _reach.copyConfig = [this] { copyConfig(); };
    _reach.showReadmes = [this](const std::string &file) { showReadmes(file); };
}

void App::wireServices() {
    // Downloads fetches and unpacks. What that means for the engine list is decided here.
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

    // A new title screen makes the cards re-ask.
    _art.arrived = [this] {
        State::get().sys.artRev = _art.revision();

        touch();
    };
}

void App::wireConfig() {
    _config.replaced = [this](const bool detect) {
        _downloads.relist();
        _downloads.opened(detect);
    };

    _config.launched = [this] {
        if (Session::get().config().general.autoClose) {
            _shell.stop();
        }
    };

    State::get().changed = [this] {
        _dirty = true;

        _shell.nudge();
    };
}

App::~App() = default;

bool App::start() {
    _shell.set_icon(Mark::of(256));

    if (!_shell.start("ZDL4", 1180, 760)) {
        return false;
    }

    describeRuntime();
    applySavedSettings();

    build();
    wireShell();

    restoreGeometry();

    if (const std::string &said = Session::get().warning(); !said.empty()) {
        _notifier.error(said);
    }

    _config.settings().showFormatNote();

    Shell::set_outline(Theme::palette().borderStrong);

    return true;
}

void App::describeRuntime() {
    State::System &sys = State::get().sys;

    sys.version = QZDL_VERSION;

    if constexpr (*QZDL_GIT_REVISION != '\0') {
        sys.version += " (" QZDL_GIT_REVISION ")";
    }

    sys.runtime = "tinytk " QZDL_TINYTK_VERSION;

#ifdef _WIN32
    sys.windows = true;
#else
    sys.windows = false;
#endif
}

void App::applySavedSettings() {
    const GeneralSettings &general = Session::get().config().general;

    Theme::set_mode(getModeFromConfigLiteral(general.theme));

    State::get().nav.shelf = general.startView == StartView::Games ? State::Shelf::Games : State::Shelf::Profiles;
}

void App::build() {
    ttk::Root &root = _shell.ui();

    // Built first, then handed to the frame that places them.
    auto bar = std::make_unique<components::TitleBar>(&_reach);
    auto pages = std::make_unique<ttk::Widget>();
    auto logs = std::make_unique<components::LogDock>(&_reach);

    _bar = bar.get();
    _pages = pages.get();
    _logs = logs.get();

    components::Frame *frame = root.content()->append(std::make_unique<components::Frame>(_bar, _pages, _logs));

    frame->add(std::move(bar));
    frame->add(std::move(pages));
    frame->add(std::move(logs));

    _library = _pages->append(std::make_unique<pages::LibraryPage>(&_reach));

    _dialogs = root.layer(ttk::Root::DIALOGS)->append(std::make_unique<ttk::DialogLayer>());

    _dialogs->closed = [this](const ttk::Dialog *gone) {
        if (gone == _pick) {
            _pick = nullptr;
        }
    };

    _toasts = root.layer(ttk::Root::NOTICES)->append(std::make_unique<ttk::Toasts>([this](const int id) {
        _notifier.dismiss(id);
    }));

    _tips = root.layer(ttk::Root::TIPS)->append(std::make_unique<ttk::Tips>());
}

void App::wireShell() {
    _shell.draggable = [this](const double x, const double y) {
        return !_dialogs->covered() && !_shell.ui().has_dismiss() && _bar->draggable(x, y);
    };

    _shell.back = [this] { back(); };
    _shell.forward = [this] { forward(); };

    _shell.shortcut = [this](const ttk::Key &pressed) { return shortcut(pressed); };

    _shell.shadeChanged = [this] {
        Shell::set_outline(Theme::palette().borderStrong);
        _shell.ui().damage_all();

        touch();
    };

    _shell.resized = [this](const double, const double) { _shell.ui().relayout(); };
}

void App::run() {
    _downloads.relist();

    // Once per turn of the loop rather than on a timer: with nothing in flight the
    // loop blocks, and an idle window costs nothing.
    _shell.settle = [this] {
        if (_dirty) {
            _dirty = false;

            sync();
        }

        const ttk::Widget *over = _shell.ui().hovered();
        const double x = _shell.ui().pointer_x();
        const double y = _shell.ui().pointer_y();

        if (over != nullptr && !over->hint.empty()) {
            _tips->point(over->hint, over->box(), x, y, Shell::now());
        } else {
            _tips->point({}, BLRect{}, x, y, Shell::now());
        }

        _toasts->set_messages(_notifier.messages());
    };

    _shell.run();

    // Every way out of the loop ends here, the title bar's close button included.
    persist();
}

void App::sync() {
    if (const bool wants = _files.state().open; wants != (_pick != nullptr)) {
        if (wants) {
            _pick = _dialogs->show(std::make_unique<ttk::FilePickerDialog>(_files));
        } else if (_dialogs->top() == _pick) {
            _dialogs->dismiss();
        } else {
            _pick = nullptr;
        }
    }

    const State::Page page = State::get().sys.page;

    if (page == State::Page::Profile) {
        _sawProfile = true;
    } else if (page == State::Page::Engines) {
        _sawEngines = true;
    } else if (page == State::Page::Settings) {
        _sawSettings = true;
    }

    if (_sawProfile && _profile == nullptr) {
        _profile = _pages->append(std::make_unique<pages::ProfilePage>(&_reach));
    }

    if (_sawEngines && _enginesView == nullptr) {
        _enginesView = _pages->append(std::make_unique<pages::EnginesPage>(&_reach));
    }

    if (_sawSettings && _settings == nullptr) {
        _settings = _pages->append(std::make_unique<pages::SettingsPage>(&_reach));
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

void App::touch() {
    _dirty = true;

    _shell.nudge();
}

void App::go(const State::Page page) {
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

void App::ask(const std::string &title,
              const std::string &body,
              const std::string &accept,
              const bool danger,
              std::function<void()> accepted) {
    _dialogs->show(
            std::make_unique<ttk::ConfirmDialog>(title, body, accept, danger, [this, then = std::move(accepted)] {
                if (then) {
                    then();
                }

                touch();
            }));
}

void App::prompt(const std::string &title,
                 const std::string &label,
                 const std::string &value,
                 const std::string &accept,
                 std::function<void(const std::string &)> accepted) {
    _dialogs->show(std::make_unique<ttk::PromptDialog>(
            title, label, value, accept, [this, then = std::move(accepted)](const std::string &typed) {
                if (then) {
                    then(typed);
                }

                touch();
            }));
}

void App::editGame(const std::string &title, const GameEntry &game, std::function<void(const GameEntry &)> accepted) {
    _dialogs->show(std::make_unique<dialogs::EntryDialog>(
            title, game, _files, [this, then = std::move(accepted)](const GameEntry &edited) {
                if (then) {
                    then(edited);
                }

                touch();
            }));
}

void App::showAbout() const {
    _dialogs->show(std::make_unique<dialogs::AboutDialog>());
}

void App::showCommand() {
    _dialogs->show(std::make_unique<dialogs::CommandDialog>(
            [this] { _notifier.success("The command line is on the clipboard."); }));
}

void App::showReadmes(const std::string &file) {
    const std::string name = std::filesystem::path(file).filename().string();
    std::vector<Readmes::File> texts = Readmes::of(file);

    if (texts.empty()) {
        _notifier.info("No text files came with " + name + ".");

        return;
    }

    _dialogs->show(std::make_unique<dialogs::ReadmeDialog>(name, std::move(texts)));
}

void App::copyConfig() {
    ProfileBridge::pushConfigDonors();

    _dialogs->show(std::make_unique<dialogs::CopyConfigDialog>([this](const std::string &id) {
        _config.profile().copyEngineConfig(id);

        touch();
    }));
}

bool App::covered() const {
    return _dialogs != nullptr && _dialogs->covered();
}

void App::dismissTop() const {
    if (_dialogs != nullptr) {
        _dialogs->close();
    }
}

void App::cycleShade() {
    const Theme::Mode next = Theme::cycle_mode();

    Session::get().config().general.theme = getConfigThemeLiteral(next);
    Session::get().save();

    Shell::set_outline(Theme::palette().borderStrong);
    _shell.ui().damage_all();

    touch();
}

bool App::shortcut(const ttk::Key &pressed) {
    if (pressed.code == ttk::Code::Escape) {
        if (covered()) {
            dismissTop();

            return true;
        }

        return false;
    }

    if (pressed.code == ttk::Code::Return && !covered() && State::get().sys.page != State::Page::Settings
        && State::get().sys.page != State::Page::Engines) {
        _config.profile().launch();

        return true;
    }

    if (pressed.code == ttk::Code::F1 && !covered()) {
        showAbout();

        touch();

        return true;
    }

    if (pressed.alt && pressed.code == ttk::Code::Left) {
        back();

        return true;
    }

    if (pressed.alt && pressed.code == ttk::Code::Right) {
        forward();

        return true;
    }

    return false;
}

void App::restoreGeometry() {
    const WindowGeometry &saved = Session::get().config().general.window;

    const int width = saved.hasSize && saved.width > 0 ? std::clamp(saved.width, 720, LARGEST_WINDOW) : 0;
    const int height = saved.hasSize && saved.height > 0 ? std::clamp(saved.height, 520, LARGEST_WINDOW) : 0;

    std::optional<SDL_Point> at;

    if (saved.hasPosition) {
        at = SDL_Point{saved.x, saved.y};
    }

    _shell.set_geometry(at, width, height, saved.maximized);
}

void App::rememberGeometry() const {
    WindowGeometry &window = Session::get().config().general.window;

    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    _shell.geometry(x, y, width, height);

    if (width <= 0 || height <= 0) {
        return;
    }

    window.maximized = _shell.maximized();
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
