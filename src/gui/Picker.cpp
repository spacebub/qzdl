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
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "core/Paths.h"
#include "core/Session.h"
#include "core/Text.h"
#include "gui/Convert.h"
#include "gui/Desktop.h"
#include "gui/Picker.h"

namespace {

// The remembered directory a kind of pick starts in.
std::string &directoryFor(const std::string &kind) {
    LastDirs &dirs = Session::get().config().general.lastDirs;

    if (kind == "wad") {
        return dirs.wad;
    }

    if (kind == "src") {
        return dirs.src;
    }

    if (kind == "save") {
        return dirs.save;
    }

    if (kind == "zdl") {
        return dirs.zdl;
    }

    if (kind == "config") {
        return dirs.config;
    }

    if (kind == "replay") {
        return dirs.replay;
    }

    return dirs.general;
}

// Windows hands back backslashes; everything past this point is forward slashes.
std::string forward(std::string path) {
    for (char &each : path) {
        if (each == '\\') {
            each = '/';
        }
    }

    return path;
}

std::vector<std::string> split(const std::string &path) {
    std::vector<std::string> parts;

    for (std::string &part : Text::split(path, '/')) {
        if (!part.empty()) {
            parts.push_back(std::move(part));
        }
    }

    return parts;
}

bool rooted(const std::string &path) {
    return path.starts_with("/");
}

// A typed path the way a shell would take it: "~" is home, and anything
// relative hangs off the working directory rather than staying unrooted.
std::string resolve(const std::string &typed) {
    std::string text = forward(typed);

    if (text == "~" || text.starts_with("~/")) {
        if (const std::filesystem::path home = Paths::homeDirectory(); !home.empty()) {
            text = home.generic_string() + text.substr(1);
        }
    }

    std::error_code code;
    std::filesystem::path path = std::filesystem::absolute(text, code);

    if (code) {
        return text;
    }

    std::error_code asked;

    if (const std::filesystem::path real = std::filesystem::weakly_canonical(path, asked);
        !asked) {
        path = real;
    } else {
        path = path.lexically_normal();
    }

    // A trailing slash leaves an empty name on the end, which the row along the
    // top and the Up button both then walk into rather than out of.
    if (path.filename().empty() && path.has_relative_path()) {
        path = path.parent_path();
    }

    return path.generic_string();
}

// Windows drops trailing dots and spaces, so a name ending in them would not be
// the name that lands. "." and ".." fall away with them, which is the point.
std::string tidy(std::string name) {
    while (!name.empty() && (name.back() == '.' || name.back() == ' ')) {
        name.pop_back();
    }

    return name;
}

// A leading dot everywhere, and the hidden attribute on Windows.
bool concealed(const std::filesystem::directory_entry &step) {
#ifdef _WIN32
    if (step.path().filename().string().starts_with(".")) {
        return true;
    }

    const DWORD marks = GetFileAttributesW(step.path().c_str());

    return marks != INVALID_FILE_ATTRIBUTES && (marks & FILE_ATTRIBUTE_HIDDEN) != 0;
#else
    return step.path().filename().string().starts_with(".");
#endif
}

}

Picker::Picker(const ui::Zdl *window, Notifier *notifier, Chosen chosen)
    : _window(window), _notifier(notifier), _chosen(std::move(chosen)) {
    const auto &pick = _window->global<ui::Pick>();

    pick.set_entries(_rows);

    const auto wantedFrom = [](const std::shared_ptr<slint::Model<slint::SharedString>> &filters) {
        std::vector<std::string> wanted;

        wanted.reserve(filters->row_count());

        for (size_t row = 0; row < filters->row_count(); row++) {
            wanted.push_back(Convert::plain(*filters->row_data(row)));
        }

        return wanted;
    };

    pick.on_start([this, wantedFrom](const slint::SharedString &action,
                                     const slint::SharedString &title,
                                     const std::shared_ptr<slint::Model<slint::SharedString>>
                                         &filters,
                                     const bool directories, const bool folders,
                                     const bool multiple, const slint::SharedString &remember,
                                     const slint::SharedString &option,
                                     const slint::SharedString &hint) {
        _saving = false;

        start(Convert::plain(action), Convert::plain(title), wantedFrom(filters), directories,
              folders, multiple, Convert::plain(remember), Convert::plain(option),
              Convert::plain(hint));
        suggest("");
    });

    // Files are listed and filtered as when opening; only what the button does
    // at the end of it differs.
    pick.on_start_save([this, wantedFrom](const slint::SharedString &action,
                                          const slint::SharedString &title,
                                          const std::shared_ptr<slint::Model<slint::SharedString>>
                                              &filters,
                                          const slint::SharedString &remember,
                                          const slint::SharedString &name) {
        _saving = true;

        start(Convert::plain(action), Convert::plain(title), wantedFrom(filters), false, false,
              false, Convert::plain(remember), "", "");
        suggest(Convert::plain(name));
    });

    pick.on_named([this](const slint::SharedString &name) {
        _saveName = Convert::plain(name);

        showTarget();
    });

    pick.on_save([this](const slint::SharedString &name, const bool replacing) {
        save(Convert::plain(name), replacing);
    });

    pick.on_go([this](const slint::SharedString &path) { go(Convert::plain(path)); });

    pick.on_up([this] {
        // Above a drive letter is the list of drives, not a folder.
        if (!rooted(_path) && _parts.size() <= 1) {
            showDrives();

            return;
        }

        go(Convert::plain(Convert::fromPath(
            std::filesystem::path(_path).parent_path())));
    });

    pick.on_up_to([this](const int index) {
        std::string wanted = rooted(_path) ? "/" : "";

        for (int part = 0; part <= index && std::cmp_less(part, _parts.size()); part++) {
            wanted += _parts[static_cast<size_t>(part)];

            if (part < index) {
                wanted += "/";
            }
        }

        // "C:" alone names the current directory on that drive, not its root.
        go(wanted.size() == 2 && wanted[1] == ':' ? wanted + "/" : wanted);
    });

    pick.on_show_drives([this] { showDrives(); });
    pick.on_show_hidden([this](const bool value) { showHidden(value); });
    pick.on_mark([this](const slint::SharedString &path) { mark(Convert::plain(path)); });

    pick.on_choose([this](const slint::SharedString &path) {
        choose({Convert::plain(path)});
    });

    pick.on_choose_marked([this] { choose(_marked); });
    pick.on_typed([this](const slint::SharedString &path) { typed(Convert::plain(path)); });
    pick.on_dismiss([this] { dismiss(); });
}

std::string Picker::startDirectory(const std::string &kind) {
    const std::string &remembered = directoryFor(kind);
    std::error_code code;

    if (!remembered.empty() && std::filesystem::is_directory(remembered, code)) {
        return Convert::plain(Convert::fromPath(remembered));
    }

    const std::filesystem::path home = Paths::homeDirectory();

    return Convert::plain(Convert::fromPath(
        home.empty() ? std::filesystem::current_path(code) : home));
}

void Picker::rememberDirectory(const std::string &kind, const std::string &path) {
    if (!path.empty()) {
        directoryFor(kind) = path;
    }
}

void Picker::start(const std::string &action, const std::string &title,
                   const std::vector<std::string> &filters, const bool directories,
                   const bool folders, const bool multiple, const std::string &remember,
                   const std::string &option, const std::string &optionHint) {
    const auto &pick = _window->global<ui::Pick>();

    _action = action;
    _remember = remember.empty() ? "general" : remember;
    _filters = filters.empty() ? std::vector<std::string>{"*"} : filters;
    _suffixes.clear();
    _anything = false;

    for (const std::string &filter : _filters) {
        if (filter == "*" || filter == "*.*") {
            _anything = true;
        } else if (filter.starts_with("*")) {
            // Every filter is "*.something", so the suffix is the whole test.
            _suffixes.push_back(Text::lower(filter.substr(1)));
        }
    }
    _directories = directories;
    _multiple = multiple;
    _marked.clear();

    pick.set_title(Convert::text(title));
    pick.set_directories(directories);
    pick.set_folders(folders);
    pick.set_multiple(multiple);
    pick.set_option(Convert::text(option));
    pick.set_option_hint(Convert::text(optionHint));
    pick.set_option_set(false);
    pick.set_hidden_shown(hidden());
    pick.set_editing(false);
    pick.set_saving(_saving);
    pick.set_open(true);

    go(startDirectory(_remember));
}

void Picker::go(const std::string &path) {
    // Gathering several, a mark survives walking out and back in. A single pick's
    // is about to be taken, so it does not outlive its directory.
    if (!_multiple) {
        _marked.clear();
    }

    _drives = false;
    _path = forward(path);
    _parts = split(_path);

    walk();
    push();
    showTarget();
}

bool Picker::hidden() {
    return Session::get().config().general.showHidden;
}

void Picker::showHidden(const bool value) {
    Session::get().config().general.showHidden = value;

    _window->global<ui::Pick>().set_hidden_shown(value);

    if (!_drives) {
        walk();
    }

    push();
}

void Picker::showDrives() {
    _drives = true;
    _entries.clear();

    for (const std::string &drive : Desktop::drives()) {
        _entries.push_back(Entry{
            .name = drive, .path = drive, .directory = true, .hidden = false, .key = drive});
    }

    push();
}

bool Picker::wanted(const std::filesystem::path &path) const {
    if (_anything) {
        return true;
    }

    return std::ranges::contains(_suffixes, Text::lower(path.extension().string()));
}

void Picker::walk() {
    _entries.clear();

    std::vector<Entry> directories;
    std::vector<Entry> files;
    std::error_code code;

    for (std::filesystem::directory_iterator step(_path, code), end;
         step != end && !code; step.increment(code)) {
        std::error_code asked;
        const std::filesystem::path &path = step->path();
        const bool aside = concealed(*step);

        if (aside && !hidden()) {
            continue;
        }

        std::string name = path.filename().string();
        std::string key = Text::lower(name);

        Entry entry{
            .name = std::move(name),
            .path = Convert::plain(Convert::fromPath(path)),
            .directory = step->is_directory(asked),
            .hidden = aside,
            .key = std::move(key),
        };

        if (entry.directory) {
            directories.push_back(std::move(entry));
        } else if (!_directories && wanted(path)) {
            files.push_back(std::move(entry));
        }
    }

    const auto byName = [](const Entry &left, const Entry &right) {
        return Text::naturalLess(left.key, right.key);
    };

    std::ranges::sort(directories, byName);
    std::ranges::sort(files, byName);

    _entries = std::move(directories);
    _entries.insert(_entries.end(), std::make_move_iterator(files.begin()),
                    std::make_move_iterator(files.end()));
}

// By path rather than by row, so a mark survives walking out and back in.
void Picker::mark(const std::string &path) {
    const auto found = std::ranges::find(_marked, path);
    const bool already = found != _marked.end();

    // One at a time: marking another moves the mark rather than adding to it.
    if (_multiple) {
        if (already) {
            _marked.erase(found);
        } else {
            _marked.push_back(path);
        }
    } else {
        _marked.clear();

        if (!already) {
            _marked.push_back(path);
        }
    }

    push();
}

void Picker::choose(const std::vector<std::string> &paths) {
    // Copies: dismiss() clears _marked, which is what choose_marked passes in.
    const std::string action = _action;
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    const std::vector<std::string> chosen = paths;
    const bool option = _window->global<ui::Pick>().get_option_set();

    rememberDirectory(_remember, _path);
    dismiss();

    if (_chosen && !chosen.empty()) {
        _chosen(action, chosen, option);
    }
}

std::string Picker::target(const std::string &name) const {
    // Only ever a name: a path typed in here would otherwise save somewhere
    // other than the directory on show.
    const std::string bare = tidy(
        Text::trim(std::filesystem::path(forward(Text::trim(name))).filename().string()));

    if (bare.empty()) {
        return {};
    }

    // A name with none of our suffixes on it is one the picker's own filter
    // would hide the next time around, so it takes the first filter's.
    const bool suffixed = _anything || _suffixes.empty()
        || std::ranges::any_of(_suffixes, [&bare](const std::string &suffix) {
               return Text::iendsWith(bare, suffix);
           });

    return Convert::plain(Convert::fromPath(
        std::filesystem::path(_path) / (suffixed ? bare : bare + _suffixes.front())));
}

void Picker::suggest(const std::string &name) {
    const auto &pick = _window->global<ui::Pick>();

    _saveName = name;

    pick.set_name(Convert::text(name));
    pick.set_name_stem(static_cast<int>(std::filesystem::path(name).stem().string().size()));
    pick.set_name_seed(pick.get_name_seed() + 1);

    showTarget();
}

void Picker::showTarget() {
    const auto &pick = _window->global<ui::Pick>();
    const std::string wanted = target(_saveName);
    std::error_code code;

    pick.set_target(Convert::text(wanted));
    pick.set_replacing(!wanted.empty() && std::filesystem::is_regular_file(wanted, code));
}

void Picker::save(const std::string &name, const bool replacing) {
    const std::string wanted = target(name);
    std::error_code code;

    if (wanted.empty()) {
        _notifier->warning("Give the file a name first.");

        return;
    }

    // Enter carries no red button with it, so it stops at what is already there.
    if (!replacing && std::filesystem::is_regular_file(wanted, code)) {
        _notifier->warning(std::filesystem::path(wanted).filename().string()
                           + " is already there. Use Replace to write over it.");

        return;
    }

    choose({wanted});
}

void Picker::typed(const std::string &path) {
    const auto &pick = _window->global<ui::Pick>();
    const std::string text = Text::trim(path);
    std::error_code code;

    if (text.empty()) {
        pick.set_editing(false);

        return;
    }

    const std::string wanted = resolve(text);

    if (std::filesystem::is_directory(wanted, code)) {
        pick.set_editing(false);
        go(wanted);

        return;
    }

    if (!_directories && std::filesystem::is_regular_file(wanted, code)) {
        pick.set_editing(false);

        const std::filesystem::path file(wanted);

        // Saving, a file typed in full names the directory and the name apart:
        // the button below is still what writes it, red where this one is.
        if (_saving) {
            go(Convert::plain(Convert::fromPath(file.parent_path())));
            suggest(file.filename().string());

            return;
        }

        choose({wanted});

        return;
    }

    _notifier->warning(wanted + " is not there.");
}

void Picker::dismiss() {
    const auto &pick = _window->global<ui::Pick>();

    // Typing a path is escaped on its own, before the sheet is.
    if (pick.get_editing()) {
        pick.set_editing(false);

        return;
    }

    pick.set_open(false);
    _multiple = false;
    _marked.clear();
}

void Picker::push() {
    const auto &pick = _window->global<ui::Pick>();
    std::vector<ui::DirEntry> rows;

    rows.reserve(_entries.size());

    for (const Entry &entry : _entries) {
        rows.push_back(ui::DirEntry{
            .name = Convert::text(entry.name),
            .path = Convert::text(entry.path),
            .directory = entry.directory,
            .hidden = entry.hidden,
            .marked = std::ranges::find(_marked, entry.path) != _marked.end(),
        });
    }

    // Counted rather than tallied as they are marked: the list is a handful of
    // paths, and a count kept alongside is one more thing to get wrong.
    int folders = 0;

    for (const std::string &path : _marked) {
        std::error_code code;

        if (std::filesystem::is_directory(path, code)) {
            ++folders;
        }
    }

    Models::reconcile(*_rows, rows);
    pick.set_marked_folders(folders);
    pick.set_path(Convert::text(_path));
    pick.set_parts(Convert::strings(_parts));
    pick.set_rooted(rooted(_path));
    pick.set_drives(_drives);
    pick.set_marked(static_cast<int>(_marked.size()));

    if (_drives) {
        pick.set_nothing(Convert::text("No drives found"));
    } else if (_directories) {
        pick.set_nothing(Convert::text("No directories here"));
    } else {
        pick.set_nothing(Convert::text("Nothing here matches " + Text::join(_filters, ", ")));
    }
}
