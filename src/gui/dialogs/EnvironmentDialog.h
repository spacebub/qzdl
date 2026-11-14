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

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/layout/Box.h"
#include "ttk/toolkit/layout/Scroll.h"
#include "ttk/toolkit/overlays/Dialog.h"

#include "core/config/Profile.h"

namespace dialogs {

// The variables a profile sets for its launches, edited as name and value rows.
class EnvironmentDialog : public ttk::Dialog {
public:
    EnvironmentDialog(std::vector<EnvVariable> variables, std::function<void(std::vector<EnvVariable>)> accepted);

    void opened() override;

    bool advance(double now) override;

    void arrange(ttk::Typeface &type) override;

private:
    void rebuild();

    // Save stays off while a name could not be set.
    void check() const;

    void commit() const;

    std::vector<EnvVariable> _variables;
    std::function<void(std::vector<EnvVariable>)> _accepted;

    std::vector<ttk::Field *> _names;

    std::optional<size_t> _drop;
    bool _revealLast = false;

    ttk::Scroll *_scroll = nullptr;
    ttk::Box *_rows = nullptr;
    ttk::Label *_empty = nullptr;
    ttk::Label *_problem = nullptr;
    ttk::Button *_accept = nullptr;
};

}
