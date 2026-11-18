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

#include <filesystem>
#include <fstream>
#include <string>

#include "support/FlatpakApp.h"

namespace tests {

namespace fs = std::filesystem;

FlatpakApp
deployFlatpak(const fs::path &root, const std::string &app, const std::string &branch, const std::string &commit) {
    const fs::path deployed = root / "app" / app;
    const fs::path onBranch = deployed / "x86_64" / branch;
    const fs::path exported = root / "exports" / "bin" / app;

    fs::create_directories(onBranch / commit / "export" / "bin");
    fs::create_directories(exported.parent_path());
    std::ofstream(onBranch / commit / "export" / "bin" / app) << "#!/bin/sh\n";

    for (const fs::path &link : {deployed / "current", onBranch / "active", exported}) {
        fs::remove(link);
    }

    fs::create_directory_symlink(fs::path("x86_64") / branch, deployed / "current");
    fs::create_directory_symlink(commit, onBranch / "active");
    fs::create_symlink(fs::path("../../app") / app / "current" / "active" / "export" / "bin" / app, exported);

    return {.exported = exported, .commit = onBranch / commit};
}

void nameRelease(const FlatpakApp &deployed, const std::string &version) {
    const fs::path metainfo = deployed.commit / "files" / "share" / "metainfo"
                              / (deployed.exported.filename().string() + ".metainfo.xml");

    fs::create_directories(metainfo.parent_path());
    std::ofstream(metainfo) << R"(<releases><release version=")" << version << R"("/></releases>)";
}

}
