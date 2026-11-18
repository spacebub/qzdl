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
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ttk/dialogs/FilePicker.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/Select.h"
#include "ttk/toolkit/controls/Toggle.h"
#include "ttk/toolkit/layout/Box.h"
#include "ttk/toolkit/overlays/Dialog.h"

#include "core/config/Config.h"
#include "core/launch/Dialect.h"
#include "core/launch/KnownPorts.h"
#include "core/util/FileVersion.h"

namespace dialogs {

// A name and a file, for a game or an engine already on this machine. An engine also
// takes its DOSBox flag, the port picked for it and its release version.
class EntryDialog : public ttk::Dialog {
public:
    EntryDialog(const std::string &title,
                GameEntry game,
                ttk::FilePicker &files,
                std::function<void(const GameEntry &)> accepted);

    // A new engine with release versions cannot be saved before one is picked. An edit can. `read` gives the version
    // text of the file, from which an engine without a version is offered one.
    EntryDialog(const std::string &title,
                Engine engine,
                bool adding,
                ttk::FilePicker &files,
                std::function<void(const Engine &)> accepted,
                Dialect::VersionReader read = FileVersion::of);

    // The file the picker came back with.
    void setFile(const std::string &path);

    void opened() override;

private:
    // A game is edited as an engine with the port rows hidden.
    EntryDialog(const std::string &title, bool forEngine, Engine engine, bool adding, ttk::FilePicker &files);

    void commit() const;

    [[nodiscard]] bool saveable() const;

    // Fills what the file tells and shows the rows that describe it. A new entry starts with
    // only the file, so picking one is all there is to do.
    void fill();

    void showRows();

    // The ports that fit the DOSBox flag, after an option that leaves the port to the file name.
    // A picked one that no longer fits goes back to the file name.
    void offerPorts();

    // The release versions of the port, showing the one the stored version falls in. A version without a
    // release is cleared, and so is one stored for other ranges.
    void offerVersions();

    // The file may name a release. An engine that needs a version takes it.
    void suggestVersion();

    // A Flatpak export names the release it installs. The engine takes it in place of a picked one. The install is read
    // again only for another file.
    void readInstall();

    ttk::FilePicker &_files;

    // The picker keeps the browse callback past this dialog: it calls in only while this lives.
    std::shared_ptr<bool> _alive = std::make_shared<bool>(true);

    std::function<void(const Engine &)> _accepted;

    bool _forEngine;
    bool _adding;
    std::string _title;

    ttk::Field *_file = nullptr;
    ttk::Field *_name = nullptr;
    ttk::Toggle *_dos = nullptr;
    ttk::Select *_port = nullptr;
    ttk::Box *_versions = nullptr;
    ttk::Select *_version = nullptr;
    ttk::Label *_versionHint = nullptr;
    ttk::Box *_fromInstall = nullptr;
    ttk::Label *_installLine = nullptr;
    ttk::Button *_accept = nullptr;

    Engine _entry;
    std::span<const Port::Port> _offered;

    // The ranges the stored version belongs to.
    std::span<const Dialect::VersionRange> _ranges;

    bool _revealed = false;

    Dialect::VersionReader _read = FileVersion::of;

    // Set by hand, so a new file no longer decides them.
    bool _nameTouched = false;
    bool _versionTouched = false;
    bool _dosTouched = false;

    // The file an unpicked version belongs to: the one opened with, or the last one a version was suggested for.
    std::string _versionFile;

    // The release the install of the file names. Empty for anything but a Flatpak export that names one.
    std::string _installedVersion;

    // The file _installedVersion was read for. Unset when the next read must go to disk.
    std::optional<std::string> _installFile;
};

}
