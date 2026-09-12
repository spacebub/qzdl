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

#include <utility>

#include "core/util/Text.h"
#include "core/wad/FileInfo.h"
#include "gui/dialogs/EntryDialog.h"
#include "gui/toolkit/controls/Button.h"

namespace dialogs {

using namespace toolkit;

EntryDialog::EntryDialog(const std::string &title, std::string kind,
                       std::vector<std::string> filters, std::string remember,
                       std::string name, std::string file, const bool dosOffered,
                       const bool dosbox, FilePicker &picker,
                       std::function<void(const std::string &, const std::string &, bool)>
                           accepted)
    : _picker(picker), _accepted(std::move(accepted)), _kind(std::move(kind)),
      _filters(std::move(filters)), _remember(std::move(remember)), _title(title),
      _filePath(std::move(file)), _named(std::move(name)), _dosbox(dosOffered && dosbox) {
    wanted = 520.0;

    Box *column = card()->append(Box::column());

    column->pad(22.0)->spacing(16.0);

    heading(column, title);

    _file = column->append(std::make_unique<Field>("File", [this](const std::string &value) {
        _filePath = value;

        _accept->setEnabled(!Text::trim(_filePath).empty());
    }));

    _file->mono()->icon(Glyphs::Glyph::Folder, "Browse", [this] {
        _picker.open("entry-file", _title, _filters, false, false, false, _remember);
    });

    _file->setText(_filePath);

    _name = column->append(std::make_unique<Field>("Name", [this](const std::string &value) {
        _named = value;
    }));

    _name->placeholder("Taken from the file")
        ->note("What profiles and .zdl files call this one.");

    _name->accepted = [this] { commit(); };
    _name->setText(_named);

    _dos = column->append(std::make_unique<Toggle>("Runs under DOSBox",
                                                   [this](const bool value) {
        _dosbox = value;

        _dos->setChecked(value);
    }));

    _dos->hint = "A DOS program. It is started inside DOSBox, with every directory the "
                 "launch names mounted as a drive of its own";

    _dos->setVisible(dosOffered);
    _dos->setChecked(_dosbox);

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
    _accept->setEnabled(!Text::trim(_filePath).empty());
}

void EntryDialog::opened() {
    _name->takeFocus();
}

void EntryDialog::setFile(const std::string &path) {
    _filePath = path;

    _file->setText(_filePath);

    // Only when nothing has been typed, so a chosen name survives.
    if (Text::trim(_named).empty() && !_filePath.empty()) {
        _named = _kind == "iwad" ? FileInfo::describeIwad(_filePath)
                                 : FileInfo::describePort(_filePath);

        _name->setText(_named);
    }

    _accept->setEnabled(!Text::trim(_filePath).empty());
}

void EntryDialog::commit() {
    if (Text::trim(_filePath).empty()) {
        return;
    }

    // Copied out: accepting takes the dialog down, and the callable with it.
    const auto fire = _accepted;
    const std::string name = Text::trim(_named);
    const std::string file = Text::trim(_filePath);
    const bool dos = _dosbox;
    const std::function<void()> shut = dismissed;

    if (shut) {
        shut();
    }

    if (fire) {
        fire(name, file, dos);
    }
}

}
