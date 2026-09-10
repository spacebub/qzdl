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

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "main.h"
#include "gui/components/Notifier.h"

class Picker {
public:
    using Chosen = std::function<void(const std::string &action,
                                      const std::vector<std::string> &paths, bool option)>;

    Picker(const ui::Zdl *window, Notifier *notifier, Chosen chosen);

    [[nodiscard]] static std::string startDirectory(const std::string &kind);
    static void rememberDirectory(const std::string &kind, const std::string &path);

private:
    void start(const std::string &action, const std::string &title,
               const std::vector<std::string> &filters, bool directories, bool folders,
               bool multiple, const std::string &remember, const std::string &option,
               const std::string &optionHint);

    void save(const std::string &name, bool replacing);

    // The open directory with the name under it, given the first filter's suffix when it has none.
    [[nodiscard]] std::string target(const std::string &name) const;

    // Puts the name in the sheet's field with the stem selected.
    void suggest(const std::string &name);

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

    [[nodiscard]] static bool hidden();

    const ui::Zdl *_window;
    Notifier *_notifier;
    Chosen _chosen;

    // Changed in place, so marking a file updates one row.
    std::shared_ptr<slint::VectorModel<ui::DirEntry>> _rows
        = std::make_shared<slint::VectorModel<ui::DirEntry>>();

    std::string _action;
    std::string _remember;
    std::vector<std::string> _filters;

    std::vector<std::string> _suffixes;
    bool _anything{false};
    bool _directories{false};
    bool _multiple{false};

    // Set before start(), which pushes it to the sheet.
    bool _saving{false};

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

        // Lowercased once, for sorting.
        std::string key;
    };

    std::vector<Entry> _entries;
};
