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

#include <cstdlib>

#include "core/Env.h"

std::string Env::get(const char *name) {
#ifdef _MSC_VER
    // The same read, into a copy of its own rather than into the block the
    // environment keeps: hence the free once it has been taken.
    char *value = nullptr;
    size_t length = 0;

    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) {
        return {};
    }

    std::string held(value);

    std::free(value);

    return held;
#else
    // NOLINTNEXTLINE(concurrency-mt-unsafe) -- nothing here ever writes the environment.
    const char *value = std::getenv(name);

    return value != nullptr ? std::string(value) : std::string();
#endif
}
