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
#include <iterator>
#include <ranges>
#include <system_error>
#include <utility>

#include "core/config/Schema.h"
#include "core/launch/Dialect.h"
#include "core/launch/DosFiles.h"
#include "core/launch/Storage.h"
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

    const std::filesystem::path folder = Config::profileFolder(named.stem().string());

    return folder.empty() ? std::filesystem::path() : folder / named;
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

bool ownsDirectory(const Config &config, const Profile &profile) {
    const std::filesystem::path named(profile.config);
    const std::string stem = named.stem().string();

    // "." or ".." would name the profiles folder or the data directory itself.
    if (named.empty() || named.is_absolute() || named.has_parent_path() || stem == "."
        || stem == "..") {
        return false;
    }

    const std::filesystem::path own = profileDirectory(profile);

    if (own.empty()) {
        return false;
    }

    // A hand-written config can point two profiles at one folder. Then it is neither's.
    // Case aside, since the filesystem may not tell the two apart either.
    return std::ranges::none_of(config.profiles, [&](const Profile &other) {
        return other.id != profile.id
            && Text::iequals(profileDirectory(other).string(), own.string());
    });
}

namespace {

// An absent source counts as done, and a target that is the source under another case
// is free.
std::error_code renameFile(const std::filesystem::path &was, const std::filesystem::path &now) {
    std::error_code asked;

    if (was == now || !std::filesystem::exists(was, asked)) {
        return {};
    }

    if (std::filesystem::exists(now, asked) && !std::filesystem::equivalent(was, now, asked)) {
        return std::make_error_code(std::errc::file_exists);
    }

    std::filesystem::rename(was, now, asked);

    return asked;
}

}

bool renameDirectory(Profile &profile, const std::string &file, std::string *error) {
    Profile renamed = profile;

    renamed.config = file;

    const std::filesystem::path from = profileDirectory(profile);
    const std::filesystem::path to = profileDirectory(renamed);
    std::error_code asked;

    // Nothing written yet, so only the name moves.
    if (from.empty() || to.empty() || from == to || !std::filesystem::exists(from, asked)) {
        profile.config = file;

        return true;
    }

    // The configs are renamed where they stand, then the folder. A failure undoes in reverse.
    const std::filesystem::path before = from / profile.config;
    const std::filesystem::path after = from / file;
    const std::pair<std::filesystem::path, std::filesystem::path> moves[] = {
        {before, after},
        {extraConfigFile(before), extraConfigFile(after)},
        {from, to},
    };

    for (size_t at = 0; at < std::size(moves); at++) {
        const std::error_code failed = renameFile(moves[at].first, moves[at].second);

        if (!failed) {
            continue;
        }

        for (size_t back = at; back-- > 0;) {
            (void) renameFile(moves[back].second, moves[back].first);
        }

        if (error != nullptr) {
            *error = moves[at].second.filename().string() + ": " + failed.message();
        }

        return false;
    }

    profile.config = file;

    return true;
}

bool discardDirectory(const Profile &profile, std::string *error) {
    const std::filesystem::path own = profileDirectory(profile);

    if (own.empty()) {
        return true;
    }

    std::error_code asked;

    std::filesystem::remove_all(own, asked);

    if (asked) {
        if (error != nullptr) {
            *error = asked.message();
        }

        return false;
    }

    return true;
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

bool copyPortConfig(const Config &config, const Profile &from, const Profile &to,
                    std::string *error) {
    const std::filesystem::path taken = portConfigFile(config, from);
    const std::filesystem::path here = portConfigFile(config, to);

    if (taken.empty() || here.empty() || taken == here) {
        return true;
    }

    std::error_code code;

    std::filesystem::create_directories(here.parent_path(), code);

    if (!std::filesystem::copy_file(taken, here,
                                    std::filesystem::copy_options::overwrite_existing, code)) {
        if (error != nullptr) {
            *error = code.message();
        }

        return false;
    }

    return true;
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

    if (demo.mode == ReplayMode::Off || file.empty()) {
        return {};
    }

    if (demo.mode == ReplayMode::Record && Dialect::of(config).dos
        && !DosFiles::spellable(file.filename().string())) {
        return "DOS cannot spell " + file.filename().string() + ", and eight characters "
            "and three is all it can spell, so there would be nothing by that name to "
            "record into.";
    }

    std::error_code code;

    if (demo.mode == ReplayMode::Play && !std::filesystem::is_regular_file(file, code)) {
        return "There is no demo at " + file.string() + " any more.";
    }

    return {};
}

}
