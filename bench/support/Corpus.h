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
#include <vector>

namespace bench {

// Game files built once into the sandbox, so the readers are measured against
// real bytes rather than whatever happens to be installed.
namespace Corpus {

// IWAD: a 320x200 TITLEPIC, a PLAYPAL, IWADINFO and 32 maps.
const std::filesystem::path &iwad();

const std::filesystem::path &pwad(int maps);

// Stored-method zip: IWADINFO, a PNG title and 32 maps.
const std::filesystem::path &pk3();

// The legacy .zdl ini a config is imported from.
const std::filesystem::path &ini();

const std::filesystem::path &json(int profiles, int files);
const std::filesystem::path &json(int engines, int profiles, int addons, bool grouped = false);

// Small PWADs, for the paths a file list holds.
const std::vector<std::string> &addons(int count);

// A folder of `count` small PWADs and PK3s for the file browser, spread over the folder and `levels` of subfolders
// below it, four to a level and five on the third. A few text files beside them are not add-ons.
struct Tree {
    std::filesystem::path root;

    // The add-on files, in the order written.
    std::vector<std::string> files;
};

const Tree &tree(int count, int levels);

// GZDoom saves of the corpus iwad, written into `folder`.
void saves(const std::filesystem::path &folder, int count);

}

}
