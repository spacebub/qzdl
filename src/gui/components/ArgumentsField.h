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

#include "ttk/toolkit/controls/TextBox.h"
#include "ttk/toolkit/layout/Box.h"

namespace components {

// The prefix and the extra arguments in one frame, drawn as a field is.
class ArgumentsField : public ttk::Box {
public:
    using Edited = std::function<void(const std::string &)>;

    ArgumentsField(std::string caption, Edited prefix, Edited extra);

    // Leaves a box alone while it already holds the text, so typing is not undone.
    void set(const std::string &prefix, const std::string &extra) const;

    [[nodiscard]] ttk::TextBox *prefix() const { return _prefix; }
    [[nodiscard]] ttk::TextBox *extra() const { return _extra; }

    void arrange(ttk::Typeface &type) override;
    void paint(const ttk::Painter &painter) override;

private:
    ttk::Box *_row = nullptr;
    ttk::TextBox *_prefix = nullptr;
    ttk::TextBox *_extra = nullptr;

    BLRect _frame{};
    double _divider = 0.0;
};

}
