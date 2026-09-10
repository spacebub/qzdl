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
#include <string_view>

#include "qzdl_git_revision.h"
#include "core/Detect.h"
#include "core/Paths.h"
#include "core/Session.h"
#include "core/Text.h"
#include "gui/App.h"
#include "gui/Convert.h"
#include "gui/Desktop.h"

#ifdef _WIN32
#include "gui/WindowChrome.h"
#endif

namespace {

// The pickers match on the extension alone, so these are suffixes, not names.
constexpr std::array WAD_FILTERS = std::to_array<std::string_view>({
    "*.wad", "*.pwad", "*.iwad", "*.pk3", "*.pk7", "*.pkz", "*.pke", "*.ipk3", "*.ipk7",
    "*.zip", "*.7z", "*.deh", "*.bex", "*.lmp", "*.cfg",
});

#ifdef _WIN32
constexpr std::array PORT_FILTERS = std::to_array<std::string_view>({"*.exe"});
#else
constexpr std::array PORT_FILTERS = std::to_array<std::string_view>({"*"});
#endif

constexpr std::array ZDL_FILTERS = std::to_array<std::string_view>({"*.zdl"});

// Only what Session::load can read; a source port's own .cfg is not.
constexpr std::array CONFIG_FILTERS = std::to_array<std::string_view>({"*.json", "*.ini"});

constexpr std::array SAVE_FILTERS =
    std::to_array<std::string_view>({"*.zds", "*.dsg", "*.esg", "*.sav", "*.save"});

// A demo is a lump whatever recorded it, and has been since 1993.
constexpr std::array REPLAY_FILTERS = std::to_array<std::string_view>({"*.lmp"});

// Worked out once: every path on the page goes through this, on every resize frame.
const std::string &homePrefix() {
    static const std::string home =
        Convert::plain(Convert::fromPath(Paths::homeDirectory())) + "/";

    return home;
}

std::string prettyPath(const std::string &path) {
    const std::string &home = homePrefix();

    return home.size() > 1 && path.starts_with(home) ? "~" + path.substr(home.size() - 1) : path;
}

// Dropped a whole leading directory at a time: a half-cut name is not a path.
std::string fitPath(const std::string &path, const int room) {
    std::string pretty = prettyPath(path);

    if (room <= 0 || std::cmp_less_equal(pretty.size(), room)) {
        return pretty;
    }

    for (size_t at = pretty.find('/'); at != std::string::npos; at = pretty.find('/', at + 1)) {
        // The ellipsis is one character wide in the face this is set in.
        if (std::string candidate = "…/" + pretty.substr(at + 1);
            std::cmp_less_equal(candidate.size() - 2, room - 1)) {
            return candidate;
        }
    }

    return pretty;
}

// The one control for it is a single button, so the three modes are a ring.
std::string nextTheme(const std::string &mode) {
    if (mode == "system") {
        return "light";
    }

    if (mode == "light") {
        return "dark";
    }

    return "system";
}

}

App::App()
    : _window(ui::Zdl::create()),
      _notifier(&*_window),
      _runs(&*_window),
      _config(&*_window, &_notifier, &_runs),
      _picker(&*_window, &_notifier,
              [this](const std::string &action, const std::vector<std::string> &paths,
                     const bool option) { picked(action, paths, option); }),
      _engines(&*_window, &_notifier, &_config) {
    IwadArt::prune();

    // Art arrives off-thread; moving the key is what has the cards ask again.
    _art.arrived = [this] { _window->global<ui::Sys>().set_art_rev(_art.revision()); };

    bindSystem();
    bindTheme();

    // A config that arrived without engines still has whatever ZDL fetched on disk.
    _config.replaced = [this](const bool detect) {
        _engines.relist();

        if (detect) {
            _engines.discover();
        }
    };

    // Hiding the last window ends the event loop, so the pending autosave has to
    // be written before that.
    _config.launched = [this] {
        if (Session::get().config().general.autoClose) {
            persist();
            _window->window().hide();
        }
    };

    _window->window().on_close_requested([this] {
        persist();

        return slint::CloseRequestResponse::HideWindow;
    });
}

void App::run() {
    restoreGeometry();
    _window->show();

#ifdef _WIN32
    // Queued for the loop below: the backend only makes the native window once
    // the loop runs, so there is nothing to take over before that.
    WindowChrome::apply(_window->window());

    const slint::Color edge = _window->global<ui::Theme>().get_border_strong();

    WindowChrome::outline(edge.red(), edge.green(), edge.blue());
#endif

    slint::run_event_loop();
    _window->hide();
}

void App::bindSystem() {
    const auto &sys = _window->global<ui::Sys>();

    // A release carries no revision, so it reads as the version alone.
    std::string version = QZDL_VERSION;

    if (*QZDL_GIT_REVISION != '\0') {
        version += " (" QZDL_GIT_REVISION ")";
    }

    sys.set_version(Convert::text(version));
    sys.set_runtime(Convert::text(std::string("Slint ") + SLINT_VERSION_STRING));

#ifdef _WIN32
    sys.set_windows(true);
#else
    sys.set_windows(false);
#endif

    sys.set_wad_filters(Convert::strings(WAD_FILTERS));
    sys.set_port_filters(Convert::strings(PORT_FILTERS));
    sys.set_zdl_filters(Convert::strings(ZDL_FILTERS));
    sys.set_config_filters(Convert::strings(CONFIG_FILTERS));
    sys.set_save_filters(Convert::strings(SAVE_FILTERS));
    sys.set_replay_filters(Convert::strings(REPLAY_FILTERS));

    sys.on_go([this](const slint::SharedString &page) { go(Convert::plain(page)); });
    sys.on_back([this] { back(); });
    sys.on_forward([this] { forward(); });

    sys.on_pretty_path([](const slint::SharedString &path) {
        return Convert::text(prettyPath(Convert::plain(path)));
    });

    sys.on_fit_path([](const slint::SharedString &path, const int room) {
        return Convert::text(fitPath(Convert::plain(path), room));
    });

    sys.on_trim([](const slint::SharedString &value) {
        return Convert::text(Text::trim(Convert::plain(value)));
    });

    sys.on_directory_of([](const slint::SharedString &path) {
        return Convert::fromPath(Convert::toPath(path).parent_path());
    });

    sys.on_file_name([](const slint::SharedString &path) {
        return Convert::fromPath(Convert::toPath(path).filename());
    });

    sys.on_is_file([](const slint::SharedString &path) {
        std::error_code code;

        return std::filesystem::is_regular_file(Convert::toPath(path), code);
    });

    sys.on_is_directory([](const slint::SharedString &path) {
        std::error_code code;

        return std::filesystem::is_directory(Convert::toPath(path), code);
    });

    // Asked of what is on disk rather than of the two strings: the same program
    // reached by another spelling is still the same program.
    sys.on_same_file([](const slint::SharedString &left, const slint::SharedString &right) {
        return Detect::same(Convert::toPath(left), Convert::toPath(right));
    });

    sys.on_art_for([this](int, const slint::SharedString &file) {
        return _art.of(Convert::plain(file));
    });

    sys.on_start_directory([](const slint::SharedString &kind) {
        return Convert::text(Picker::startDirectory(Convert::plain(kind)));
    });

    sys.on_remember_directory([this](const slint::SharedString &kind,
                                     const slint::SharedString &path) {
        Picker::rememberDirectory(Convert::plain(kind), Convert::plain(path));

        // The remembered directory is config, and nothing else will write it.
        _config.scheduleSave();
    });

    sys.on_reveal([this](const slint::SharedString &path) {
        std::string why;

        if (!Desktop::open(Convert::plain(path), &why)) {
            _notifier.warning(why.empty()
                ? "Nothing on this system offered to open it."
                : "Nothing on this system offered to open it: " + why + ".");
        }
    });

    sys.on_open_url([](const slint::SharedString &url) {
        Desktop::open(Convert::plain(url));
    });

    sys.on_begin_move([] {
#ifdef _WIN32
        return WindowChrome::beginMove();
#else
        return false;
#endif
    });

    sys.on_move_by([this](const float x, const float y) {
        const slint::PhysicalPosition at = _window->window().position();
        const float scale = _window->window().scale_factor();

        _window->window().set_position(slint::PhysicalPosition({
            .x = at.x + static_cast<int32_t>(x * scale),
            .y = at.y + static_cast<int32_t>(y * scale),
        }));
    });

    sys.on_outline([]([[maybe_unused]] const slint::Color edge) {
#ifdef _WIN32
        WindowChrome::outline(edge.red(), edge.green(), edge.blue());
#endif
    });
}

void App::bindTheme() {
    const auto &theme = _window->global<ui::Theme>();
    const std::string saved = Session::get().config().general.theme;

    theme.set_mono(Convert::text(Desktop::monospaceFamily()));
    theme.set_mode(Convert::text(saved == "light" || saved == "dark" ? saved : "system"));

    theme.on_cycle([this] {
        const auto &current = _window->global<ui::Theme>();
        const std::string next = nextTheme(Convert::plain(current.get_mode()));

        current.set_mode(Convert::text(next));

        // Written straight out: one button chooses it, with no Save beside it.
        Session::get().config().general.theme = next;
        Session::get().save();
    });
}

void App::go(const std::string &page) {
    const auto &sys = _window->global<ui::Sys>();

    if (page == Convert::plain(sys.get_page())) {
        return;
    }

    _history.push_back(Convert::plain(sys.get_page()));

    if (_history.size() > HISTORY) {
        _history.erase(_history.begin());
    }

    // Going somewhere new is the end of whatever was ahead.
    _ahead.clear();

    sys.set_page(Convert::text(page));
}

void App::back() {
    if (_history.empty()) {
        return;
    }

    const auto &sys = _window->global<ui::Sys>();

    _ahead.push_back(Convert::plain(sys.get_page()));
    sys.set_page(Convert::text(_history.back()));
    _history.pop_back();
}

void App::forward() {
    if (_ahead.empty()) {
        return;
    }

    const auto &sys = _window->global<ui::Sys>();

    _history.push_back(Convert::plain(sys.get_page()));
    sys.set_page(Convert::text(_ahead.back()));
    _ahead.pop_back();
}

// A size under the minimum or a position off every screen was saved against a
// layout that is gone; both are dropped and the desktop places the window.
void App::restoreGeometry() const {
    const WindowGeometry &saved = Session::get().config().general.window;

    if (saved.hasSize && saved.width > 0 && saved.height > 0) {
        _window->window().set_size(slint::LogicalSize({
            .width = static_cast<float>(std::max(saved.width, 720)),
            .height = static_cast<float>(std::max(saved.height, 520)),
        }));
    }

    if (saved.hasPosition && saved.x >= 0 && saved.y >= 0) {
        _window->window().set_position(slint::LogicalPosition({
            .x = static_cast<float>(saved.x),
            .y = static_cast<float>(saved.y),
        }));
    }
}

void App::rememberGeometry() const {
    WindowGeometry &window = Session::get().config().general.window;
    const float scale = _window->window().scale_factor();
    const slint::PhysicalPosition at = _window->window().position();
    const slint::PhysicalSize size = _window->window().size();

    window.hasPosition = true;
    window.x = static_cast<int>(static_cast<float>(at.x) / scale);
    window.y = static_cast<int>(static_cast<float>(at.y) / scale);
    window.hasSize = true;
    window.width = static_cast<int>(static_cast<float>(size.width) / scale);
    window.height = static_cast<int>(static_cast<float>(size.height) / scale);
}

void App::persist() {
    rememberGeometry();
    _config.flush();

    std::string error;

    if (!Session::get().save(&error)) {
        // An exception out of a Slint callback terminates the process, and
        // std::println can throw one.
        // NOLINTNEXTLINE(cert-err33-c,modernize-use-std-print)
        std::fprintf(stderr, "Could not save the config: %s\n", error.c_str());
    }
}

void App::picked(const std::string &action, const std::vector<std::string> &paths,
                 const bool option) {
    const auto &cfg = _window->global<ui::Cfg>();
    const std::string &first = paths.front();

    if (action == "add-iwads") {
        cfg.invoke_add_iwads(Convert::strings(paths));
    } else if (action == "add-files") {
        cfg.invoke_add_files(Convert::strings(paths));
    } else if (action == "add-port") {
        _config.addPort(first, {}, option);
    } else if (action == "entry-file") {
        _window->global<ui::Sheets>().set_entry_file(Convert::text(first));
    } else if (action == "dosbox") {
        cfg.invoke_set_dosbox(Convert::text(first));
    } else if (action == "savegame") {
        cfg.invoke_set_savegame(Convert::text(first));
    } else if (action == "replay") {
        cfg.invoke_set_replay_file(Convert::text(first));
    } else if (action == "save-zdl") {
        cfg.invoke_save_zdl(Convert::text(first));
    } else if (action == "load-config") {
        cfg.invoke_load(Convert::text(first));
    } else if (action == "save-config") {
        cfg.invoke_save_as(Convert::text(first));
    } else if (action == "load-zdl") {
        cfg.invoke_load_zdl(Convert::text(first));
    }
}
