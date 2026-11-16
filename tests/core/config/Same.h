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
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "core/config/Config.h"
#include "core/config/Profile.h"

// Profile and Config have no operator==, so a round trip is compared field by field.
namespace same {

inline void files(const std::vector<FileEntry> &got, const std::vector<FileEntry> &want) {
    ASSERT_EQ(got.size(), want.size());

    for (size_t at = 0; at < got.size(); ++at) {
        EXPECT_EQ(got[at].file, want[at].file) << "file " << at;
        EXPECT_EQ(got[at].enabled, want[at].enabled) << "file " << at;
    }
}

inline void profile(const Profile &got, const Profile &want) {
    SCOPED_TRACE("profile " + want.id);

    EXPECT_EQ(got.id, want.id);
    EXPECT_EQ(got.name, want.name);
    EXPECT_EQ(got.iwad, want.iwad);
    EXPECT_EQ(got.port, want.port);
    files(got.files, want.files);
    EXPECT_EQ(got.skill, want.skill);
    EXPECT_EQ(got.monsters, want.monsters);
    EXPECT_EQ(got.warp, want.warp);
    EXPECT_EQ(got.extra, want.extra);
    EXPECT_EQ(got.on(Switch::dialogOpen), want.on(Switch::dialogOpen));
    EXPECT_EQ(got.on(Switch::replayOpen), want.on(Switch::replayOpen));
    EXPECT_EQ(got.on(Switch::saveOpen), want.on(Switch::saveOpen));
    EXPECT_EQ(got.multiplayer, want.multiplayer);
    EXPECT_EQ(got.replay, want.replay);
    EXPECT_EQ(got.save, want.save);
    EXPECT_EQ(got.config, want.config);
    EXPECT_EQ(got.on(Switch::sharedConfig), want.on(Switch::sharedConfig));
    EXPECT_EQ(got.on(Switch::customCommand), want.on(Switch::customCommand));
    EXPECT_EQ(got.command, want.command);
    EXPECT_EQ(got.prefix, want.prefix);
    EXPECT_EQ(got.environment, want.environment);
    EXPECT_EQ(got.on(Switch::dosFullscreen), want.on(Switch::dosFullscreen));
    EXPECT_EQ(got.on(Switch::dosExit), want.on(Switch::dosExit));
    EXPECT_EQ(got.on(Switch::captureOutput), want.on(Switch::captureOutput));
    EXPECT_EQ(got.on(Switch::levelstat), want.on(Switch::levelstat));
    EXPECT_EQ(got.complevel, want.complevel);
    EXPECT_EQ(got.flags, want.flags);
    EXPECT_EQ(got.options, want.options);
    EXPECT_EQ(got.numbers, want.numbers);
    EXPECT_EQ(got.on(Switch::skipAutoload), want.on(Switch::skipAutoload));
    EXPECT_EQ(got.on(Switch::music), want.on(Switch::music));
    EXPECT_EQ(got.on(Switch::soundEffects), want.on(Switch::soundEffects));
    EXPECT_EQ(got.display, want.display);
    EXPECT_EQ(got.screenshots, want.screenshots);
    EXPECT_EQ(got.screenshotFolder, want.screenshotFolder);
    EXPECT_EQ(got.zdlFile, want.zdlFile);
    EXPECT_EQ(got.on(Switch::generalOpen), want.on(Switch::generalOpen));
    EXPECT_EQ(got.on(Switch::gameplayOpen), want.on(Switch::gameplayOpen));
}

inline void entries(const std::vector<GameEntry> &got, const std::vector<GameEntry> &want) {
    ASSERT_EQ(got.size(), want.size());

    for (size_t at = 0; at < got.size(); ++at) {
        EXPECT_EQ(got[at].name, want[at].name) << "entry " << at;
        EXPECT_EQ(got[at].file, want[at].file) << "entry " << at;
    }
}

inline void entries(const std::vector<PortEntry> &got, const std::vector<PortEntry> &want) {
    ASSERT_EQ(got.size(), want.size());

    for (size_t at = 0; at < got.size(); ++at) {
        EXPECT_EQ(got[at].name, want[at].name) << "entry " << at;
        EXPECT_EQ(got[at].file, want[at].file) << "entry " << at;
        EXPECT_EQ(got[at].dosbox, want[at].dosbox) << "entry " << at;
        EXPECT_EQ(got[at].portId, want[at].portId) << "entry " << at;
        EXPECT_EQ(got[at].dialect, want[at].dialect) << "entry " << at;
        EXPECT_EQ(got[at].version, want[at].version) << "entry " << at;
    }
}

inline void general(const GeneralSettings &got, const GeneralSettings &want) {
    EXPECT_EQ(got.alwaysAdd, want.alwaysAdd);
    EXPECT_EQ(got.dosbox, want.dosbox);
    EXPECT_EQ(got.detected, want.detected);
    EXPECT_EQ(got.autoClose, want.autoClose);
    EXPECT_EQ(got.launchZdlImmediately, want.launchZdlImmediately);
    EXPECT_EQ(got.showPaths, want.showPaths);
    EXPECT_EQ(got.noUserConf, want.noUserConf);
    EXPECT_EQ(got.moveAtNextStart, want.moveAtNextStart);
    EXPECT_EQ(got.showHidden, want.showHidden);
    EXPECT_EQ(got.profileConfigs, want.profileConfigs);
    EXPECT_EQ(got.startView, want.startView);
    EXPECT_EQ(got.gamePort, want.gamePort);
    EXPECT_EQ(got.theme, want.theme);
    EXPECT_EQ(got.isImported, want.isImported);
    EXPECT_EQ(got.importedFrom, want.importedFrom);
    EXPECT_EQ(got.importDate, want.importDate);

    EXPECT_EQ(got.window.width, want.window.width);
    EXPECT_EQ(got.window.height, want.window.height);
    EXPECT_EQ(got.window.x, want.window.x);
    EXPECT_EQ(got.window.y, want.window.y);
    EXPECT_EQ(got.window.hasSize, want.window.hasSize);
    EXPECT_EQ(got.window.hasPosition, want.window.hasPosition);
    EXPECT_EQ(got.window.maximized, want.window.maximized);

    EXPECT_EQ(got.lastDirs.general, want.lastDirs.general);
    EXPECT_EQ(got.lastDirs.wad, want.lastDirs.wad);
    EXPECT_EQ(got.lastDirs.src, want.lastDirs.src);
    EXPECT_EQ(got.lastDirs.save, want.lastDirs.save);
    EXPECT_EQ(got.lastDirs.zdl, want.lastDirs.zdl);
    EXPECT_EQ(got.lastDirs.config, want.lastDirs.config);
    EXPECT_EQ(got.lastDirs.replay, want.lastDirs.replay);
}

inline void config(const Config &got, const Config &want) {
    general(got.general, want.general);
    entries(got.iwads, want.iwads);
    entries(got.ports, want.ports);
    EXPECT_EQ(got.activeProfileId, want.activeProfileId);

    ASSERT_EQ(got.profiles.size(), want.profiles.size());

    for (size_t at = 0; at < got.profiles.size(); ++at) {
        profile(got.profiles[at], want.profiles[at]);
    }
}

inline std::string slurp(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);

    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

inline void spill(const std::filesystem::path &path, const std::string_view text) {
    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::binary | std::ios::trunc);

    file.write(text.data(), static_cast<std::streamsize>(text.size()));
}

}
