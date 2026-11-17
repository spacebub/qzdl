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
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "ttk/system/Text.h"
#include "ttk/toolkit/Root.h"
#include "ttk/toolkit/controls/Button.h"

#include "core/launch/Dialect.h"
#include "core/launch/KnownPorts.h"
#include "core/util/FileVersion.h"
#include "core/wad/FileInfo.h"
#include "gui/dialogs/EntryDialog.h"
#include "gui/services/Filters.h"

using namespace ttk;

namespace dialogs {

namespace {

constexpr std::string_view FROM_NAME = "From the file name";

}

EntryDialog::EntryDialog(const std::string &title,
                         GameEntry game,
                         ttk::FilePicker &files,
                         std::function<void(const GameEntry &)> accepted)
    : EntryDialog(title, false, Engine{.name = std::move(game.name), .file = std::move(game.file)}, false, files) {
    _accepted = [accepted = std::move(accepted)](const Engine &edited) {
        if (accepted) {
            accepted(GameEntry{.name = edited.name, .file = edited.file});
        }
    };
}

EntryDialog::EntryDialog(const std::string &title,
                         Engine engine,
                         const bool adding,
                         ttk::FilePicker &files,
                         std::function<void(const Engine &)> accepted,
                         const Dialect::VersionReader read)
    : EntryDialog(title, true, std::move(engine), adding, files) {
    _accepted = std::move(accepted);
    _read = read;

    suggestVersion();
}

EntryDialog::EntryDialog(
        const std::string &title, const bool forEngine, Engine engine, const bool adding, ttk::FilePicker &files)
    : _files(files),
      _forEngine(forEngine),
      _adding(adding),
      _title(title),
      _entry(std::move(engine)),
      _revealed(!_entry.file.empty()),
      // A name that only says what the file would, as a drop fills it, still follows the file.
      _nameTouched(
              !_entry.name.empty()
              && _entry.name
                         != (forEngine ? FileInfo::describePort(_entry.file) : FileInfo::describeIwad(_entry.file))),
      _versionFile(_entry.file) {
    wanted = 520.0;

    Box *column = card()->append(Box::column());

    column->pad(22.0)->spacing(16.0);

    heading(column, title);

    _file = column->append(std::make_unique<Field>("File", [this](const std::string &value) {
        _entry.file = Text::trim(value);

        std::error_code code;

        // Only a path that names a file fills the rest, so the rows do not jump per key.
        if (std::filesystem::is_regular_file(_entry.file, code)) {
            fill();
        }

        offerPorts();
        _accept->set_enabled(saveable());
    }));

    _file->mono()->icon(Glyphs::Glyph::Folder, "Browse", [this] {
        _files.open(_title,
                    _forEngine ? Filters::port() : Filters::game(),
                    false,
                    false,
                    false,
                    _forEngine ? LastDir::SRC : LastDir::WAD,
                    Picked::first([this, alive = std::weak_ptr<bool>(_alive)](const std::string &path) {
                        if (!alive.expired()) {
                            setFile(path);
                        }
                    }));
    });

    _file->set_text(_entry.file);

    _name = column->append(std::make_unique<Field>("Name", [this](const std::string &value) {
        _entry.name = value;
        _nameTouched = true;
    }));

    _name->placeholder("Taken from the file")->note("What profiles and .zdl files call this one.");

    _name->accepted = [this] { commit(); };
    _name->set_text(_entry.name);

    _port = column->append(std::make_unique<Select>("Port", [this](const int index) {
        if (index < 0 || std::cmp_greater(index, _offered.size())) {
            return;
        }

        // The first option leaves the port to the file name.
        _entry.port = index == 0 ? Port::Port::unknown : _offered[static_cast<size_t>(index) - 1];

        offerPorts();
        suggestVersion();
    }));

    _port->hint = "Which port this file is. ZDL4 picks the switches and files to match";

    _versions = column->append(Box::column());
    _versions->spacing(6.0);

    _version = _versions->append(std::make_unique<Select>("Version", [this](const int index) {
        const std::span<const Dialect::VersionRange> ranges = Dialect::versions(_entry);

        if (index < 0 || std::cmp_greater_equal(index, ranges.size())) {
            return;
        }

        _entry.version = std::string(ranges[static_cast<size_t>(index)].first);
        _versionTouched = true;

        offerVersions();
    }));

    _version->placeholder("None selected")->tooltip("The switches differ between releases of the port");

    _versionHint = _versions->append(
            std::make_unique<Label>("Until a version is picked, ZDL4 treats this port as the latest release."));
    _versionHint->font(400, Theme::fontSmall)->tone(&Theme::Palette::warning)->wrap();

    _dos = column->append(std::make_unique<Toggle>("Runs under DOSBox", [this](const bool value) {
        _entry.dosbox = value;
        _dosTouched = true;

        _dos->set_checked(value);

        offerPorts();
        suggestVersion();
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

    _ranges = Dialect::versions(_entry);

    showRows();
    offerPorts();
    _accept->set_enabled(saveable());
}

void EntryDialog::opened() {
    (_revealed ? _name : _file)->take_focus();
}

void EntryDialog::showRows() {
    _name->set_visible(_revealed);
    _port->set_visible(_forEngine && _revealed);
    _dos->set_visible(_forEngine && _revealed);
    _accept->set_visible(_revealed);
}

void EntryDialog::fill() {
    // An unpicked version belongs to the previous file.
    if (!_versionTouched && _entry.file != _versionFile) {
        _entry.version.clear();
        _versionFile = _entry.file;
    }

    if (!_nameTouched && !_entry.file.empty()) {
        _entry.name = _forEngine ? FileInfo::describePort(_entry.file) : FileInfo::describeIwad(_entry.file);

        _name->set_text(_entry.name);
    }

    if (const std::optional<bool> dos = Dialect::dosFromName(_entry.file); _forEngine && dos && !_dosTouched) {
        _entry.dosbox = *dos;

        _dos->set_checked(*dos);
    }

    _revealed = true;

    showRows();
    offerPorts();
    suggestVersion();
}

void EntryDialog::offerPorts() {
    if (!_forEngine) {
        return;
    }

    _offered = Dialect::picks(_entry.dosbox);

    const auto picked = _entry.port == Port::Port::unknown ? _offered.end() : std::ranges::find(_offered, _entry.port);

    if (picked == _offered.end()) {
        _entry.port = Port::Port::unknown;
    }

    std::vector<std::string> names{std::string(FROM_NAME)};

    names.reserve(_offered.size() + 1);

    for (const Port::Port each : _offered) {
        names.emplace_back(Port::known(each).name);
    }

    _port->set_options(std::move(names));
    _port->set_badges({std::string(Port::known(Dialect::fromName(_entry)).name)});
    _port->set_current(picked == _offered.end() ? 0 : static_cast<int>(picked - _offered.begin()) + 1);

    offerVersions();
}

void EntryDialog::offerVersions() {
    const std::span<const Dialect::VersionRange> ranges = Dialect::versions(_entry);

    if (ranges.data() != _ranges.data()) {
        _ranges = ranges;
        _entry.version.clear();
    }

    const Dialect::VersionRange *stored = Dialect::rangeOf(ranges, _entry.version);

    if (stored == nullptr) {
        _entry.version.clear();
    }

    std::vector<std::string> labels;

    labels.reserve(ranges.size());

    for (const Dialect::VersionRange &each : ranges) {
        labels.emplace_back(each.label);
    }

    _version->set_options(std::move(labels));
    _version->set_current(stored == nullptr ? -1 : static_cast<int>(stored - ranges.data()));

    _versions->set_visible(_revealed && !ranges.empty());
    _versionHint->set_visible(stored == nullptr);

    _accept->set_enabled(saveable());
}

void EntryDialog::suggestVersion() {
    if (!_forEngine) {
        return;
    }

    if (std::string found = Dialect::suggestedVersion(_entry, _read); !found.empty()) {
        _entry.version = std::move(found);
        _versionFile = _entry.file;

        offerVersions();
    }
}

bool EntryDialog::saveable() const {
    return !Text::trim(_entry.file).empty() && !(_adding && Dialect::needsVersion(_entry));
}

void EntryDialog::setFile(const std::string &path) {
    _entry.file = path;

    _file->set_text(_entry.file);
    _accept->set_enabled(saveable());

    fill();
}

void EntryDialog::commit() const {
    if (!saveable()) {
        if (root() != nullptr && !Text::trim(_entry.file).empty()) {
            root()->focus(_version);
        }

        return;
    }

    // Copied out: accepting takes the dialog down, and the callable with it.
    const auto fire = _accepted;
    Engine engine = _entry;
    const std::function<void()> shut = dismissed;

    engine.name = Text::trim(engine.name);
    engine.file = Text::trim(engine.file);

    if (shut) {
        shut();
    }

    if (fire) {
        fire(engine);
    }
}

}
