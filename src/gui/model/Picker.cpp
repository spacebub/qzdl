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

#include "core/config/Session.h"
#include "core/system/Paths.h"
#include "core/util/Text.h"
#include "gui/model/Picker.h"
#include "gui/util/Desktop.h"
#include "gui/util/Format.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

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

// Windows hands back backslashes.
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

// "~" is home; a relative path hangs off the working directory.
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

    // A trailing slash would leave an empty final name.
    if (path.filename().empty() && path.has_relative_path()) {
        path = path.parent_path();
    }

    return path.generic_string();
}

// Windows drops trailing dots and spaces.
std::string tidy(std::string name) {
    while (!name.empty() && (name.back() == '.' || name.back() == ' ')) {
        name.pop_back();
    }

    return name;
}

// A leading dot, or the hidden attribute on Windows.
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

Picker::Picker(Notifier *notifier, Chosen chosen)
    : _notifier(notifier), _chosen(std::move(chosen)) {}

void Picker::open(const std::string &action, const std::string &title,
                  const std::vector<std::string> &filters, const bool directories,
                  const bool folders, const bool multiple, const std::string &remember,
                  const std::string &option, const std::string &optionHint) {
    _saving = false;

    start(action, title, filters, directories, folders, multiple, remember, option, optionHint);
    suggest("");
}

void Picker::openSave(const std::string &action, const std::string &title,
                      const std::vector<std::string> &filters, const std::string &remember,
                      const std::string &name) {
    _saving = true;

    start(action, title, filters, false, false, false, remember, "", "");
    suggest(name);
}

void Picker::named(const std::string &name) {
    _saveName = name;

    showTarget();
}

void Picker::up() {
    // Above a drive letter is the list of drives, not a folder.
    if (!rooted(_path) && _parts.size() <= 1) {
        showDrives();

        return;
    }

    go(Format::fromPath(std::filesystem::path(_path).parent_path()));
}

void Picker::upTo(const int index) {
    std::string wanted = rooted(_path) ? "/" : "";

    for (int part = 0; part <= index && std::cmp_less(part, _parts.size()); part++) {
        wanted += _parts[static_cast<size_t>(part)];

        if (part < index) {
            wanted += "/";
        }
    }

    // "C:" alone names the current directory on that drive, not its root.
    go(wanted.size() == 2 && wanted[1] == ':' ? wanted + "/" : wanted);
}

void Picker::chooseMarked() {
    choose(_marked);
}

std::string Picker::startDirectory(const std::string &kind) {
    const std::string &remembered = directoryFor(kind);
    std::error_code code;

    if (!remembered.empty() && std::filesystem::is_directory(remembered, code)) {
        return Format::fromPath(remembered);
    }

    const std::filesystem::path home = Paths::homeDirectory();

    return Format::fromPath(home.empty() ? std::filesystem::current_path(code) : home);
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
    _action = action;
    _remember = remember.empty() ? "general" : remember;
    _filters = filters.empty() ? std::vector<std::string>{"*"} : filters;
    _suffixes.clear();
    _anything = false;

    for (const std::string &filter : _filters) {
        if (filter == "*" || filter == "*.*") {
            _anything = true;
        } else if (filter.starts_with("*")) {
            _suffixes.push_back(Text::lower(filter.substr(1)));
        }
    }
    _directories = directories;
    _multiple = multiple;
    _marked.clear();

    State::PickState &sheet = pick();

    sheet.title = title;
    sheet.directories = directories;
    sheet.folders = folders;
    sheet.multiple = multiple;
    sheet.option = option;
    sheet.optionHint = optionHint;
    sheet.optionSet = false;
    sheet.hiddenShown = hidden();
    sheet.editing = false;
    sheet.saving = _saving;
    sheet.open = true;

    go(startDirectory(_remember));
}

void Picker::go(const std::string &path) {
    // A multi-pick's marks survive navigation.
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

    pick().hiddenShown = value;

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
            .path = Format::fromPath(path),
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

void Picker::mark(const std::string &path) {
    const auto found = std::ranges::find(_marked, path);
    const bool already = found != _marked.end();

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
    const bool option = pick().optionSet;

    rememberDirectory(_remember, _path);
    dismiss();

    if (_chosen && !chosen.empty()) {
        _chosen(action, chosen, option);
    }
}

std::string Picker::target(const std::string &name) const {
    const std::string bare = tidy(
        Text::trim(std::filesystem::path(forward(Text::trim(name))).filename().string()));

    if (bare.empty()) {
        return {};
    }

    const bool suffixed = _anything || _suffixes.empty()
        || std::ranges::any_of(_suffixes, [&bare](const std::string &suffix) {
               return Text::iendsWith(bare, suffix);
           });

    return Format::fromPath(std::filesystem::path(_path)
                            / (suffixed ? bare : bare + _suffixes.front()));
}

void Picker::suggest(const std::string &name) {
    State::PickState &sheet = pick();

    _saveName = name;

    sheet.name = name;
    sheet.nameStem = static_cast<int>(std::filesystem::path(name).stem().string().size());
    sheet.nameSeed++;

    showTarget();
}

void Picker::showTarget() {
    State::PickState &sheet = pick();
    const std::string wanted = target(_saveName);
    std::error_code code;

    sheet.target = wanted;
    sheet.replacing = !wanted.empty() && std::filesystem::is_regular_file(wanted, code);

    State::get().touch();
}

void Picker::save(const std::string &name, const bool replacing) {
    const std::string wanted = target(name);
    std::error_code code;

    if (wanted.empty()) {
        _notifier->warning("Give the file a name first.");

        return;
    }

    if (!replacing && std::filesystem::is_regular_file(wanted, code)) {
        _notifier->warning(std::filesystem::path(wanted).filename().string()
                           + " is already there. Use Replace to write over it.");

        return;
    }

    choose({wanted});
}

void Picker::typed(const std::string &path) {
    State::PickState &sheet = pick();
    const std::string text = Text::trim(path);
    std::error_code code;

    if (text.empty()) {
        sheet.editing = false;

        State::get().touch();

        return;
    }

    const std::string wanted = resolve(text);

    if (std::filesystem::is_directory(wanted, code)) {
        sheet.editing = false;

        go(wanted);

        return;
    }

    if (!_directories && std::filesystem::is_regular_file(wanted, code)) {
        sheet.editing = false;

        const std::filesystem::path file(wanted);

        if (_saving) {
            go(Format::fromPath(file.parent_path()));
            suggest(file.filename().string());

            return;
        }

        choose({wanted});

        return;
    }

    _notifier->warning(wanted + " is not there.");
}

void Picker::dismiss() {
    State::PickState &sheet = pick();

    // Escape ends editing first, then closes the sheet.
    if (sheet.editing) {
        sheet.editing = false;

        State::get().touch();

        return;
    }

    sheet.open = false;
    _multiple = false;
    _marked.clear();

    State::get().touch();
}

void Picker::push() {
    State::PickState &sheet = pick();
    std::vector<State::DirEntry> rows;

    rows.reserve(_entries.size());

    for (const Entry &entry : _entries) {
        rows.push_back(State::DirEntry{
            .name = entry.name,
            .path = entry.path,
            .directory = entry.directory,
            .hidden = entry.hidden,
            .marked = std::ranges::find(_marked, entry.path) != _marked.end(),
        });
    }

    int folders = 0;

    for (const std::string &path : _marked) {
        std::error_code code;

        if (std::filesystem::is_directory(path, code)) {
            ++folders;
        }
    }

    sheet.entries = std::move(rows);
    sheet.markedFolders = folders;
    sheet.path = _path;
    sheet.parts = _parts;
    sheet.rooted = rooted(_path);
    sheet.drives = _drives;
    sheet.marked = static_cast<int>(_marked.size());

    if (_drives) {
        sheet.nothing = "No drives found";
    } else if (_directories) {
        sheet.nothing = "No directories here";
    } else {
        sheet.nothing = "Nothing here matches " + Text::join(_filters, ", ");
    }

    State::get().touch();
}
