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
#include "ttk/toolkit/controls/Select.h"
#include "ttk/toolkit/controls/Toggle.h"
#include "ttk/toolkit/overlays/Dialog.h"

#include "core/config/Config.h"
#include "core/launch/Dialect.h"

namespace dialogs {

// A name and a file, for a game or a source port already on this machine. A port also
// takes its DOSBox flag and dialect.
class EntryDialog : public ttk::Dialog {
public:
    EntryDialog(const std::string &title,
                GameEntry game,
                ttk::FilePicker &files,
                std::function<void(const GameEntry &)> accepted);

    EntryDialog(const std::string &title,
                PortEntry port,
                ttk::FilePicker &files,
                std::function<void(const PortEntry &)> accepted);

    // The file the picker came back with.
    void setFile(const std::string &path);

    void opened() override;

private:
    // A game is edited as a port with the port rows hidden.
    EntryDialog(const std::string &title, bool port, PortEntry entry, ttk::FilePicker &files);

    void commit() const;

    // Fills what the file tells and shows the rows that describe it. A new entry starts with
    // only the file, so picking one is all there is to do.
    void fill();

    void showRows();

    // The dialects that fit the DOSBox flag, showing the one a launch speaks. A picked one
    // that no longer fits goes back to the file name.
    void offerDialects();

    [[nodiscard]] bool managed() const;

    ttk::FilePicker &_files;

    // The picker keeps the browse callback past this dialog: it calls in only while this lives.
    std::shared_ptr<bool> _alive = std::make_shared<bool>(true);

    std::function<void(const PortEntry &)> _accepted;

    bool _port;
    std::string _title;

    ttk::Field *_file = nullptr;
    ttk::Field *_name = nullptr;
    ttk::Toggle *_dos = nullptr;
    ttk::Select *_dialect = nullptr;
    ttk::Button *_accept = nullptr;

    PortEntry _entry;
    std::vector<const Dialect::Choice *> _offered;

    bool _revealed = false;

    // Set by hand, so a new file no longer decides them.
    bool _nameTouched = false;
    bool _dosTouched = false;
};

}
