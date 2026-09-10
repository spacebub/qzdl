/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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
#include <string>

#include "core/config/Config.h"
#include "core/system/Process.h"

namespace Launcher {

[[nodiscard]] std::filesystem::path executable(const Config &config);

[[nodiscard]] bool isDosPort(const Config &config);

[[nodiscard]] std::filesystem::path dosbox(const Config &config);

bool launch(const Config &config,
            Process::Id *id = nullptr,
            Process::Stream *output = nullptr,
            std::string *error = nullptr);

}
