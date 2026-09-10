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

#include "main.h"
#include "gui/Models.h"
#include "gui/Notifier.h"

// The file browser's state. The sheet draws it; nothing about walking a
// filesystem is written in .slint.
class Picker {
public:
    using Chosen = std::function<void(const std::string &action,
                                      const std::vector<std::string> &paths, bool option)>;

    Picker(const ui::Zdl *window, Notifier *notifier, Chosen chosen);

    // Where a kind of pick starts: wherever it was last left.
    [[nodiscard]] static std::string startDirectory(const std::string &kind);
    static void rememberDirectory(const std::string &kind, const std::string &path);

private:
    void start(const std::string &action, const std::string &title,
               const std::vector<std::string> &filters, bool directories, bool folders,
               bool multiple, const std::string &remember, const std::string &option,
               const std::string &optionHint);

    // Takes the directory that is open with the name typed under it. Writing
    // over a file that is already there is only done when it is meant.
    void save(const std::string &name, bool replacing);

    // What save() would write for a name: the open directory with the name
    // under it, carrying the first filter's suffix when it has none of them.
    // Empty when the name is no name at all.
    [[nodiscard]] std::string target(const std::string &name) const;

    // Puts a name in the sheet's field, stem selected.
    void suggest(const std::string &name);

    // Pushes out what the name in the field comes to, and what it would hit.
    void showTarget();

    void go(const std::string &path);
    void showDrives();
    void showHidden(bool value);
    void mark(const std::string &path);
    void choose(const std::vector<std::string> &paths);
    void typed(const std::string &path);
    void dismiss();

    void walk();

    void push();

    [[nodiscard]] bool wanted(const std::filesystem::path &path) const;

    // Whether concealed entries are listed; remembered across picks and runs.
    [[nodiscard]] static bool hidden();

    const ui::Zdl *_window;
    Notifier *_notifier;
    Chosen _chosen;

    // Handed over once and changed in place: marking a file is one row.
    std::shared_ptr<slint::VectorModel<ui::DirEntry>> _rows
        = std::make_shared<slint::VectorModel<ui::DirEntry>>();

    std::string _action;
    std::string _remember;
    std::vector<std::string> _filters;

    // The filters as bare suffixes, taken apart once rather than per file.
    std::vector<std::string> _suffixes;
    bool _anything{false};
    bool _directories{false};
    bool _multiple{false};

    // Set before start(), which pushes it out to the sheet.
    bool _saving{false};

    // What the sheet's name field holds, reported as it is typed.
    std::string _saveName;

    std::string _path;
    std::vector<std::string> _parts;
    bool _drives{false};

    std::vector<std::string> _marked;

    struct Entry {
        std::string name;
        std::string path;
        bool directory{false};
        bool hidden{false};

        // Lowered once: the sort would otherwise lower both sides of every compare.
        std::string key;
    };

    std::vector<Entry> _entries;
};
