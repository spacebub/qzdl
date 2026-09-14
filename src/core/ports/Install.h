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
#include <string>
#include <string_view>

#include "core/ports/Catalog.h"

namespace Install {

struct Release {
    // The tag from its first digit on.
    std::string version;
    std::string asset;
    std::string url;
    long long size{0};
};

// url is empty when no asset fits the port's build.
[[nodiscard]] Release parseRelease(const std::string &body, const Catalog::Port &port);

[[nodiscard]] std::string fileNameOf(std::string_view url);

// The asset name with anything unsafe dropped, else the port's id.
[[nodiscard]] std::string downloadName(const Catalog::Port &port, std::string_view asset);

struct Placed {
    std::filesystem::path program;
    std::string trouble;

    // A message of its own; empty when the row suffices.
    std::string headline;
};

// Runs on any thread.
[[nodiscard]] Placed place(const std::filesystem::path &archive, const Catalog::Port &port);

[[nodiscard]] std::string stampedVersion(const Catalog::Port &port);
void stamp(const Catalog::Port &port, const std::string &version);

[[nodiscard]] long long shelfBytes();
void clearShelf();

}
