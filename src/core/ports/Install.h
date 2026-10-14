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

    // Lowercase hex. Empty when the release lists none.
    std::string sha256;
};

// url is empty when no asset fits the port's build.
[[nodiscard]] Release parseRelease(const std::string &body, const Catalog::Port &port);

// Only https is ever fetched.
[[nodiscard]] bool secure(std::string_view url);

[[nodiscard]] std::string fileNameOf(std::string_view url);

// The asset name with anything unsafe dropped, else the port's id.
[[nodiscard]] std::string downloadName(const Catalog::Port &port, std::string_view asset);

// What the origin said the download is. A zero size or an empty hash is not held to.
struct Promise {
    long long size{0};
    std::string sha256;
};

struct Placed {
    std::filesystem::path program;
    std::string trouble;

    // A message of its own. Empty when the row suffices.
    std::string headline;

    // The download was not what was promised, and is gone.
    bool rejected{false};
};

// Runs on any thread.
[[nodiscard]] Placed place(const std::filesystem::path &archive, const Catalog::Port &port);

// Holds a finished download to the promise, moves it from a .part name onto the
// shelf when it has one, then places it. Runs on any thread.
[[nodiscard]] Placed accept(const std::filesystem::path &from,
                            const std::filesystem::path &into,
                            const Promise &promise,
                            const Catalog::Port &port);

[[nodiscard]] std::string stampedVersion(const Catalog::Port &port);
void stamp(const Catalog::Port &port, const std::string &version);

[[nodiscard]] long long shelfBytes();
void clearShelf();

}
