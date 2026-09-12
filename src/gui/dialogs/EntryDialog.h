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

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "gui/services/FilePicker.h"
#include "gui/toolkit/controls/Field.h"
#include "gui/toolkit/controls/Toggle.h"
#include "gui/toolkit/overlays/Dialog.h"

namespace dialogs {

// A name and a file, for a game or a source port already on this machine.
class EntryDialog : public toolkit::Dialog {
public:
    // `kind` is "iwad" or "port", which is what a name is guessed from.
    EntryDialog(const std::string &title, std::string kind, std::vector<std::string> filters,
               std::string remember, std::string name, std::string file,
               bool dosOffered, bool dosbox, FilePicker &picker,
               std::function<void(const std::string &, const std::string &, bool)> accepted);

    // The file the picker came back with.
    void setFile(const std::string &path);

    void opened() override;

private:
    void commit();

    FilePicker &_picker;
    std::function<void(const std::string &, const std::string &, bool)> _accepted;

    std::string _kind;
    std::vector<std::string> _filters;
    std::string _remember;
    std::string _title;

    toolkit::Field *_file = nullptr;
    toolkit::Field *_name = nullptr;
    toolkit::Toggle *_dos = nullptr;
    toolkit::Button *_accept = nullptr;

    std::string _filePath;
    std::string _named;
    bool _dosbox = false;
};

}
