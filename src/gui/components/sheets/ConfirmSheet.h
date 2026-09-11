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

#include "gui/toolkit/overlays/Sheet.h"

namespace components {

// A question with two answers, one of which may be the dangerous one.
class ConfirmSheet : public toolkit::Sheet {
public:
    ConfirmSheet(const std::string &title, const std::string &said, const std::string &accept,
                 bool danger, std::function<void()> accepted);

private:
    std::function<void()> _accepted;
};

}
