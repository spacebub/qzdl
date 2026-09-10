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

#include <utility>

#include "core/launch/Arguments.h"
#include "core/launch/Command.h"
#include "core/launch/Dialect.h"
#include "core/launch/Dos.h"
#include "core/launch/Launcher.h"
#include "core/launch/Storage.h"
#include "core/util/Text.h"

namespace Command {

namespace {

void say(std::string *error, std::string text) {
    if (error != nullptr) {
        *error = std::move(text);
    }
}

bool substitute(const Config &config, const std::string &name, std::string &value,
                std::string *error) {
    const Profile &profile = config.activeProfile();

    if (name == "source_port") {
        value = Launcher::executable(config).string();

        if (value.empty()) {
            say(error, "This profile has no source port, so there is nothing for {source_port}.");

            return false;
        }

        return true;
    }

    if (name == "game") {
        value = config.activeIwadFile();

        if (value.empty()) {
            say(error, "This profile has no game, so there is nothing for {game}.");

            return false;
        }

        return true;
    }

    if (name.starts_with("addon_")) {
        const std::string which = name.substr(6);

        if (!Text::isInt(which) || Text::toInt(which) < 1) {
            say(error, "{" + name + "} has to end in a number, counting from one.");

            return false;
        }

        const int wanted = Text::toInt(which);

        if (std::cmp_greater(wanted, profile.files.size())) {
            say(error, profile.files.empty()
                ? "This profile has no add-ons, so there is nothing for {" + name + "}."
                : "This profile has " + std::to_string(profile.files.size())
                  + " add-ons, so there is nothing for {" + name + "}.");

            return false;
        }

        value = profile.files[static_cast<size_t>(wanted - 1)].file;

        return true;
    }

    if (name == "replaydir") {
        const std::filesystem::path replays = Storage::replayDirectory(config);

        if (replays.empty()) {
            say(error, "This profile has no folder of its own, so there is nothing for "
                "{replaydir}.");

            return false;
        }

        value = replays.string();

        return true;
    }

    if (name == "savefile") {
        const std::filesystem::path save = Storage::saveFile(config);

        if (save.empty()) {
            say(error, "This profile has no save picked, so there is nothing for {savefile}.");

            return false;
        }

        value = save.string();

        return true;
    }

    if (name == "profile" || name == "cfgdir" || name == "savedir" || name == "extracfg") {
        const std::filesystem::path own = Storage::configFile(config);

        if (own.empty()) {
            say(error, "This profile has no folder of its own, so there is nothing for {"
                + name + "}.");

            return false;
        }

        if (name == "profile") {
            value = own.parent_path().string();
        } else if (name == "cfgdir") {
            value = own.string();
        } else if (name == "extracfg") {
            value = Storage::extraConfigFile(own).string();
        } else {
            value = Storage::saveDirectory(config).string();
        }

        return true;
    }

    say(error, "{" + name + "} is not one ZDL4 knows. There is {source_port}, {game}, "
        "{addon_1} upwards, {profile}, {cfgdir}, {extracfg}, {savedir}, {savefile} and "
        "{replaydir}.");

    return false;
}

}

std::vector<std::string> custom(const Config &config, std::string *error) {
    const Profile &profile = config.activeProfile();
    const std::vector<std::string> tokens = Text::parseArguments(profile.command);

    if (tokens.empty()) {
        say(error, "There is nothing here to run.");

        return {};
    }

    std::vector<std::string> out;
    out.reserve(tokens.size());

    for (const std::string &token : tokens) {
        std::string filled;
        size_t at = 0;

        while (at < token.size()) {
            const size_t open = token.find('{', at);
            const size_t close = open == std::string::npos
                ? std::string::npos
                : token.find('}', open);

            if (close == std::string::npos) {
                filled.append(token, at);

                break;
            }

            filled.append(token, at, open - at);
            std::string value;

            if (!substitute(config, token.substr(open + 1, close - open - 1), value, error)) {
                return {};
            }

            filled += value;
            at = close + 1;
        }

        out.push_back(std::move(filled));
    }

    return out;
}

std::string trouble(const Config &config) {
    if (!config.activeProfile().customCommand) {
        return {};
    }

    std::string said;

    [[maybe_unused]] const std::vector<std::string> shown = custom(config, &said);

    return said;
}

std::string pattern(const Config &config) {
    const Profile &profile = config.activeProfile();
    const std::string port = Launcher::executable(config).string();
    const std::string iwad = config.activeIwadFile();
    const std::string own = Storage::configFile(config).string();
    const std::string extra = own.empty() ? std::string() : Storage::extraConfigFile(own).string();
    const std::string saves = Storage::saveDirectory(config).string();

    // Only a port handed the full path can have it substituted.
    const std::string save = Dialect::of(config).loads == Dialect::SaveNames::path
        ? Storage::saveFile(config).string()
        : std::string();

    const auto spell = [&port, &iwad, &own, &extra, &saves, &save, &profile](
        const std::string &token) {
        if (!port.empty() && token == port) {
            return std::string("{source_port}");
        }

        if (!own.empty() && token == own) {
            return std::string("{cfgdir}");
        }

        if (!extra.empty() && token == extra) {
            return std::string("{extracfg}");
        }

        if (!saves.empty() && token == saves) {
            return std::string("{savedir}");
        }

        if (!save.empty() && token == save) {
            return std::string("{savefile}");
        }

        if (!iwad.empty() && token == iwad) {
            return std::string("{game}");
        }

        for (size_t index = 0; index < profile.files.size(); index++) {
            if (profile.files[index].file == token) {
                return "{addon_" + std::to_string(index + 1) + "}";
            }
        }

        return Text::quoteArgument(token);
    };

    std::vector<std::string> parts;

    if (!port.empty()) {
        parts.push_back(spell(port));
    }

    for (const std::string &argument : Arguments::of(config)) {
        parts.push_back(spell(argument));
    }

    return Text::join(parts, " ");
}

std::string line(const Config &config) {
    std::vector<std::string> parts;

    if (config.activeProfile().customCommand) {
        for (const std::string &token : custom(config, nullptr)) {
            parts.push_back(Text::quoteArgument(token));
        }

        return Text::join(parts, " ");
    }

    if (Dialect::of(config).dos) {
        const Dos::Command command = Dos::command(config);

        if (command.arguments.empty()) {
            return {};
        }

        parts.push_back(Text::quoteArgument(command.dosbox.string()));

        for (const std::string &argument : command.arguments) {
            parts.push_back(Text::quoteArgument(argument));
        }

        return Text::join(parts, " ");
    }

    const std::filesystem::path port = Launcher::executable(config);

    if (!port.empty()) {
        parts.push_back(Text::quoteArgument(port.string()));
    }

    for (const std::string &argument : Arguments::of(config)) {
        parts.push_back(Text::quoteArgument(argument));
    }

    return Text::join(parts, " ");
}

}
