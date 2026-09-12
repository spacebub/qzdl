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

#include "gui/toolkit/controls/Field.h"
#include "gui/toolkit/overlays/Dialog.h"

namespace dialogs {

// One line asked for, with a name for what it is.
class PromptDialog : public toolkit::Dialog {
public:
    PromptDialog(const std::string &title, const std::string &label, std::string value,
                const std::string &accept, std::function<void(const std::string &)> accepted);

    void opened() override;

private:
    void commit();

    std::function<void(const std::string &)> _accepted;

    toolkit::Field *_field = nullptr;
    toolkit::Button *_accept = nullptr;

    std::string _value;
};

}
