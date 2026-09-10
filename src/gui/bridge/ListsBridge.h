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

#include <memory>
#include <string>
#include <vector>

#include "gui/bridge/Bridge.h"

// The active profile's files, and the IWAD and port lists.
class ListsBridge : public Bridge {
public:
    using Bridge::Bridge;

    void bind();

    void push();

    std::string addPort(const std::string &file, const std::string &name, bool dosbox);
    void updatePort(int row, const std::string &name, const std::string &file, bool dosbox);
    void removePort(int row);
    [[nodiscard]] static const std::vector<NameEntry> &ports();

    [[nodiscard]] static ui::NameRow rowOf(const std::vector<NameEntry> &list, int index,
                                           bool ports);

private:
    // Profiles reference IWADs and ports by name.
    void renamedIwad(const std::string &before, const std::string &after);
    void renamedPort(const std::string &before, const std::string &after);

    [[nodiscard]] static std::string uniqueName(const std::vector<NameEntry> &list,
                                                const std::string &base, int ignoring = -1);

    // Updated in place; see Models::reconcile.
    std::shared_ptr<slint::VectorModel<ui::FileRow>> _files
        = std::make_shared<slint::VectorModel<ui::FileRow>>();
    std::shared_ptr<slint::VectorModel<ui::NameRow>> _iwads
        = std::make_shared<slint::VectorModel<ui::NameRow>>();
    std::shared_ptr<slint::VectorModel<ui::NameRow>> _ports
        = std::make_shared<slint::VectorModel<ui::NameRow>>();
};
