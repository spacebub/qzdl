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
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/config/Config.h"
#include "core/launch/Port.h"

namespace Detect {

struct Found {
    // The catalog port it is.
    Port::Port port{};

    std::string name;
    std::filesystem::path file;
    bool dos{false};
};

[[nodiscard]] bool same(const std::filesystem::path &left, const std::filesystem::path &right);

[[nodiscard]] std::filesystem::path onPath(std::string_view name);

// Looked up once per run.
[[nodiscard]] const std::filesystem::path &dosbox();

// Walks the disk on every call. Excludes what ZDL unpacked into its own directory.
[[nodiscard]] std::vector<Found> ports();

// The engine a found port is listed as, set to the port it is, marked detected and holding the release its install
// names.
[[nodiscard]] Engine engine(const Found &found);

// The engines to add for the found ports whose file no listed engine names. Reads the disk.
[[nodiscard]] std::vector<Engine> unlisted(std::span<const Engine> listed, std::span<const Found> found);

// The fresh engines that no listed engine names, where fresh came from unlisted against compared. The disk is read
// only for listed engines that compared lacks.
[[nodiscard]] std::vector<Engine>
stillUnlisted(std::span<const Engine> listed, std::span<const Engine> compared, std::span<const Engine> fresh);

// The release the install of an engine names: the newest in the metainfo of a Flatpak export that is neither
// managed nor run in DOSBox. Empty for every other engine and for metainfo text that holds no release. Reads the disk.
[[nodiscard]] std::string installedVersion(const Engine &engine);

struct Installed {
    std::string file;
    std::string version;
};

// The releases the installs of the engines name, for the ones that name a release. Reads the disk.
[[nodiscard]] std::vector<Installed> installedVersions(std::span<const Engine> engines);

// Stores each read release in the engines that still follow that install. Text that holds no release is never
// stored. True when a stored version changed.
[[nodiscard]] bool storeVersions(std::span<Engine> engines, std::span<const Installed> read);

}
