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
#include <memory>
#include <string>
#include <vector>

#include "ttk/dialogs/FilePicker.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/Toggle.h"
#include "ttk/toolkit/overlays/Dialog.h"

namespace dialogs {

// A name and a file, for a game or a source port already on this machine.
class EntryDialog : public ttk::Dialog {
public:
    // What an empty name is guessed from.
    enum class Kind : std::uint8_t {
        Game,
        Port,
    };

    EntryDialog(const std::string &title, Kind kind, std::vector<std::string> filters,
               std::string remember, std::string name, std::string file,
               bool dosOffered, bool dosbox, ttk::FilePicker &files,
               std::function<void(const std::string &, const std::string &, bool)> accepted);

    // The file the picker came back with.
    void setFile(const std::string &path);

    void opened() override;

private:
    void commit() const;

    ttk::FilePicker &_files;

    // The picker keeps the browse callback past this dialog: it calls in only while this lives.
    std::shared_ptr<bool> _alive = std::make_shared<bool>(true);

    std::function<void(const std::string &, const std::string &, bool)> _accepted;

    Kind _kind;
    std::vector<std::string> _filters;
    std::string _remember;
    std::string _title;

    ttk::Field *_file = nullptr;
    ttk::Field *_name = nullptr;
    ttk::Toggle *_dos = nullptr;
    ttk::Button *_accept = nullptr;

    std::string _filePath;
    std::string _named;
    bool _dosbox = false;
};

}
