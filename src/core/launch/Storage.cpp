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

#include <algorithm>
#include <cctype>
#include <ranges>
#include <utility>

#include "core/config/Schema.h"
#include "core/launch/Dialect.h"
#include "core/launch/DosFiles.h"
#include "core/launch/Storage.h"
#include "core/system/Paths.h"
#include "core/util/Text.h"

namespace Storage {

std::filesystem::path configFile(const Profile &profile) {
    if (profile.config.empty()) {
        return {};
    }

    std::filesystem::path named(profile.config);

    if (named.is_absolute()) {
        return named;
    }

    const std::filesystem::path directory = Paths::dataDirectory();

    return directory.empty()
        ? std::filesystem::path()
        : directory / ConfigFile::PROFILES_DIR / named.stem() / named;
}

std::filesystem::path extraConfigFile(const std::filesystem::path &config) {
    return config.parent_path() / (config.stem().string() + "-extra" + config.extension().string());
}

std::filesystem::path configFile(const Config &config) {
    const Profile &profile = config.activeProfile();

    // A DOS port runs in the profile's own folder, so one that takes -config is always
    // handed the config there. The rest keep their own, wherever they write it.
    if (const Dialect::Port speaks = Dialect::of(config); speaks.dos) {
        return speaks.configFile ? portConfigFile(config, profile) : std::filesystem::path();
    }

    if (!config.general.profileConfigs || profile.sharedConfig) {
        return {};
    }

    return configFile(profile);
}

std::filesystem::path saveDirectory(const Config &config) {
    const std::filesystem::path own = configFile(config);

    return own.empty() ? std::filesystem::path() : own.parent_path() / "saves";
}

std::filesystem::path saveFolder(const Config &config) {
    return Dialect::of(config).save.empty() ? std::filesystem::path() : saveDirectory(config);
}

std::vector<std::string> saves(const Config &config) {
    const std::filesystem::path folder = saveFolder(config);

    if (folder.empty()) {
        return {};
    }

    const std::string_view extension = Dialect::of(config).saveExt;
    std::vector<std::pair<std::filesystem::file_time_type, std::string>> found;
    std::error_code code;

    for (std::filesystem::directory_iterator walk(folder, code), end; walk != end && !code;
         walk.increment(code)) {
        std::error_code asked;

        if (!walk->is_regular_file(asked)
            || !Text::iendsWith(walk->path().filename().string(), extension)) {
            continue;
        }

        found.emplace_back(walk->last_write_time(asked), walk->path().filename().string());
    }

    std::ranges::sort(found, [](const auto &left, const auto &right) {
        return left.first != right.first ? left.first > right.first
                                         : Text::naturalLess(left.second, right.second);
    });

    std::vector<std::string> names;
    names.reserve(found.size());

    for (auto &name: found | std::views::values) {
        names.push_back(std::move(name));
    }

    return names;
}

std::filesystem::path saveFile(const Config &config) {
    const SaveSettings &save = config.activeProfile().save;

    if (save.file.empty()) {
        return {};
    }

    std::filesystem::path named(save.file);

    if (named.is_absolute()) {
        return named;
    }

    const std::filesystem::path folder = saveFolder(config);

    return folder.empty() ? std::filesystem::path() : folder / named;
}

int saveSlot(const std::string &name) {
    const std::string stem = std::filesystem::path(name).stem().string();
    size_t at = stem.size();

    while (at > 0 && std::isdigit(static_cast<unsigned char>(stem[at - 1])) != 0) {
        at--;
    }

    // No port has that many slots.
    if (at == stem.size() || stem.size() - at > 4) {
        return -1;
    }

    return Text::toInt(stem.substr(at), -1);
}

std::string saveTrouble(const Config &config) {
    const std::filesystem::path file = saveFile(config);

    if (!config.activeProfile().save.enabled || file.empty()) {
        return {};
    }

    std::error_code code;

    if (!std::filesystem::is_regular_file(file, code)) {
        return "There is no save at " + file.string() + " any more.";
    }

    if (Dialect::of(config).loads == Dialect::SaveNames::slot
        && saveSlot(file.filename().string()) < 0) {
        return "This port loads a save by the slot it sits in, and there is no number in "
            + file.filename().string() + " to take one from.";
    }

    return {};
}

std::filesystem::path profileDirectory(const Profile &profile) {
    const std::filesystem::path own = configFile(profile);

    return own.empty() ? std::filesystem::path() : own.parent_path();
}

std::filesystem::path portConfigFile(const Config &config, const Profile &profile) {
    if (!Dialect::of(config, profile).dos) {
        return configFile(profile);
    }

    const std::filesystem::path own = profileDirectory(profile);

    return own.empty() ? std::filesystem::path() : own / ConfigFile::DOS_CFG;
}

std::filesystem::path runDirectory(const Config &config,
                                   const std::filesystem::path &portDirectory) {
    const std::filesystem::path own = profileDirectory(config.activeProfile());

    if (own.empty()) {
        return portDirectory;
    }

    std::error_code made;

    std::filesystem::create_directories(own, made);

    return std::filesystem::is_directory(own, made) ? own : portDirectory;
}

std::filesystem::path replayDirectory(const Profile &profile) {
    const std::filesystem::path own = profileDirectory(profile);

    return own.empty() ? std::filesystem::path() : own / "replays";
}

std::filesystem::path replayDirectory(const Config &config) {
    return replayDirectory(config.activeProfile());
}

std::vector<std::string> replays(const Config &config) {
    const std::filesystem::path folder = replayDirectory(config);
    std::vector<std::pair<std::filesystem::file_time_type, std::string>> found;

    if (folder.empty()) {
        return {};
    }

    std::error_code code;

    for (std::filesystem::directory_iterator walk(folder, code), end; walk != end && !code;
         walk.increment(code)) {
        std::error_code asked;

        if (!walk->is_regular_file(asked)
            || !Text::iendsWith(walk->path().filename().string(), ".lmp")) {
            continue;
        }

        found.emplace_back(walk->last_write_time(asked), walk->path().filename().string());
    }

    std::ranges::sort(found, [](const auto &left, const auto &right) {
        return left.first != right.first ? left.first > right.first
                                         : Text::naturalLess(left.second, right.second);
    });

    std::vector<std::string> names;
    names.reserve(found.size());

    for (auto &name: found | std::views::values) {
        names.push_back(std::move(name));
    }

    return names;
}

std::filesystem::path replayFile(const Config &config) {
    const ReplaySettings &replay = config.activeProfile().replay;

    if (replay.file.empty()) {
        return {};
    }

    std::filesystem::path named(replay.file);

    if (named.is_absolute()) {
        return named;
    }

    const std::filesystem::path folder = replayDirectory(config);

    if (folder.empty()) {
        return {};
    }

    // The vanilla line appends .lmp regardless, so the name always carries it.
    if (!Text::iendsWith(named.string(), ".lmp")) {
        named += ".lmp";
    }

    return folder / named;
}

std::string replayTrouble(const Config &config) {
    const ReplaySettings &demo = config.activeProfile().replay;
    const std::filesystem::path file = replayFile(config);

    if (demo.mode == 0 || file.empty()) {
        return {};
    }

    if (demo.mode == 1 && Dialect::of(config).dos
        && !DosFiles::spellable(file.filename().string())) {
        return "DOS cannot spell " + file.filename().string() + ", and eight characters "
            "and three is all it can spell, so there would be nothing by that name to "
            "record into.";
    }

    std::error_code code;

    if (demo.mode == 2 && !std::filesystem::is_regular_file(file, code)) {
        return "There is no demo at " + file.string() + " any more.";
    }

    return {};
}

}
