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
#include <filesystem>
#include <optional>
#include <utility>

#include "ttk/system/Text.h"
#include "ttk/toolkit/controls/Button.h"

#include "core/wad/FileInfo.h"
#include "gui/dialogs/EntryDialog.h"
#include "gui/services/Filters.h"

using namespace ttk;

namespace dialogs {

EntryDialog::EntryDialog(const std::string &title,
                         GameEntry game,
                         ttk::FilePicker &files,
                         std::function<void(const GameEntry &)> accepted)
    : EntryDialog(title, false, PortEntry{.name = std::move(game.name), .file = std::move(game.file)}, files) {
    _accepted = [accepted = std::move(accepted)](const PortEntry &edited) {
        if (accepted) {
            accepted(GameEntry{.name = edited.name, .file = edited.file});
        }
    };
}

EntryDialog::EntryDialog(const std::string &title,
                         PortEntry port,
                         ttk::FilePicker &files,
                         std::function<void(const PortEntry &)> accepted)
    : EntryDialog(title, true, std::move(port), files) {
    _accepted = std::move(accepted);
}

EntryDialog::EntryDialog(const std::string &title, const bool port, PortEntry entry, ttk::FilePicker &files)
    : _files(files),
      _port(port),
      _title(title),
      _entry(std::move(entry)),
      _revealed(!_entry.file.empty()),
      // A name that only says what the file would, as a drop fills it, still follows the file.
      _nameTouched(!_entry.name.empty()
                   && _entry.name
                              != (port ? FileInfo::describePort(_entry.file) : FileInfo::describeIwad(_entry.file))) {
    wanted = 520.0;

    Box *column = card()->append(Box::column());

    column->pad(22.0)->spacing(16.0);

    heading(column, title);

    _file = column->append(std::make_unique<Field>("File", [this](const std::string &value) {
        _entry.file = Text::trim(value);

        _accept->set_enabled(!_entry.file.empty());

        std::error_code code;

        // Only a path that names a file fills the rest, so the rows do not jump per key.
        if (std::filesystem::is_regular_file(_entry.file, code)) {
            fill();
        }

        offerDialects();
    }));

    _file->mono()->icon(Glyphs::Glyph::Folder, "Browse", [this] {
        _files.open(_title,
                    _port ? Filters::port() : Filters::game(),
                    false,
                    false,
                    false,
                    _port ? LastDir::SRC : LastDir::WAD,
                    Picked::first([this, alive = std::weak_ptr<bool>(_alive)](const std::string &path) {
                        if (!alive.expired()) {
                            setFile(path);
                        }
                    }));
    });

    _file->set_text(_entry.file);

    _name = column->append(
            std::make_unique<Field>("Name", [this](const std::string &value) {
                _entry.name = value;
                _nameTouched = true;
            }));

    _name->placeholder("Taken from the file")->note("What profiles and .zdl files call this one.");

    _name->accepted = [this] { commit(); };
    _name->set_text(_entry.name);

    _dialect = column->append(std::make_unique<Select>("Dialect", [this](const int index) {
        if (index < 0 || std::cmp_greater_equal(index, _offered.size())) {
            return;
        }

        PortEntry named = _entry;
        named.dialect.clear();

        // Picking what the file name already says leaves it to the name again.
        const Dialect::Choice &picked = *_offered[static_cast<size_t>(index)];

        _entry.dialect = picked.id == Dialect::of(named).id ? std::string() : std::string(picked.word);

        offerDialects();
    }));

    _dialect->hint = "The switches and files ZDL4 hands the port";
    _dialect->set_enabled(!managed());

    _dos = column->append(std::make_unique<Toggle>("Runs under DOSBox", [this](const bool value) {
        _entry.dosbox = value;
        _dosTouched = true;

        _dos->set_checked(value);

        offerDialects();
    }));

    _dos->hint = "A DOS program, or a batch file that starts one. It is run inside DOSBox "
                 "from its own folder, with every other directory the launch names mounted "
                 "as a drive of its own";

    _dos->set_checked(_entry.dosbox);

    Box *row = column->append(Box::row());

    row->spacing(8.0)->align(Box::Place::End);
    row->fixedHeight = Theme::control;

    row->append(std::make_unique<Button>("Cancel", [this] {
        if (dismissed) {
            dismissed();
        }
    }));

    _accept = row->append(std::make_unique<Button>("Save", [this] { commit(); }));
    _accept->kind(Button::Kind::Primary);
    _accept->set_enabled(!Text::trim(_entry.file).empty());

    showRows();
    offerDialects();
}

void EntryDialog::opened() {
    (_revealed ? _name : _file)->take_focus();
}

void EntryDialog::showRows() {
    _name->set_visible(_revealed);
    _dialect->set_visible(_port && _revealed);
    _dos->set_visible(_port && _revealed);
    _accept->set_visible(_revealed);
}

void EntryDialog::fill() {
    if (!_nameTouched && !_entry.file.empty()) {
        _entry.name = _port ? FileInfo::describePort(_entry.file) : FileInfo::describeIwad(_entry.file);

        _name->set_text(_entry.name);
    }

    if (const std::optional<bool> dos = Dialect::dosFromName(_entry.file); _port && dos && !_dosTouched) {
        _entry.dosbox = *dos;

        _dos->set_checked(*dos);
    }

    _revealed = true;

    showRows();
    offerDialects();
}

bool EntryDialog::managed() const {
    return !_entry.portId.empty();
}

void EntryDialog::offerDialects() {
    if (!_port) {
        return;
    }

    _offered.clear();

    for (const Dialect::Choice &each : Dialect::choices()) {
        if (each.dos == _entry.dosbox) {
            _offered.push_back(&each);
        }
    }

    if (std::ranges::find(_offered, _entry.dialect, &Dialect::Choice::word) == _offered.end()) {
        _entry.dialect.clear();
    }

    const Dialect::Id spoken = Dialect::of(_entry).id;

    // A native program can still speak a DOS dialect, such as Doom Legacy, and a launch uses it.
    if (std::ranges::find(_offered, spoken, &Dialect::Choice::id) == _offered.end()) {
        _offered.push_back(&Dialect::choice(spoken));
    }

    const auto found = std::ranges::find(_offered, spoken, &Dialect::Choice::id);

    std::vector<std::string> names;

    names.reserve(_offered.size());

    for (const Dialect::Choice *each : _offered) {
        names.emplace_back(each->name);
    }

    _dialect->set_options(std::move(names));
    _dialect->set_current(found == _offered.end() ? -1 : static_cast<int>(found - _offered.begin()));
}

void EntryDialog::setFile(const std::string &path) {
    _entry.file = path;

    _file->set_text(_entry.file);
    _accept->set_enabled(!Text::trim(_entry.file).empty());

    fill();
}

void EntryDialog::commit() const {
    if (Text::trim(_entry.file).empty()) {
        return;
    }

    // Copied out: accepting takes the dialog down, and the callable with it.
    const auto fire = _accepted;
    PortEntry entry = _entry;
    const std::function<void()> shut = dismissed;

    entry.name = Text::trim(entry.name);
    entry.file = Text::trim(entry.file);

    if (managed()) {
        entry.dialect.clear();
    }

    if (shut) {
        shut();
    }

    if (fire) {
        fire(entry);
    }
}

}
