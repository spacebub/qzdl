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
#include <array>
#include <filesystem>
#include <string_view>
#include <utility>

#include "ttk/system/Text.h"

#include "core/launch/Arguments.h"
#include "core/launch/Command.h"
#include "core/launch/Dialect.h"
#include "core/launch/Dos.h"
#include "core/launch/Launcher.h"
#include "core/launch/Storage.h"

using namespace ttk;

namespace Command {

namespace {

// Quoted so a shell, and Text::parse_arguments, hand the token back as it is. Outside Windows
// single quotes keep everything literal, so a quote inside needs no escape.
std::string quote(const std::string &token) {
#ifdef _WIN32
    return Text::quote_argument(token);
#else
    static constexpr std::string_view PLAIN = "_./:=+,@%-";

    const bool bare = !token.empty() && std::ranges::all_of(token, [](const char letter) {
        return (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z') || (letter >= '0' && letter <= '9')
               || PLAIN.contains(letter);
    });

    if (bare) {
        return token;
    }

    if (!token.contains('\'')) {
        return "'" + token + "'";
    }

    std::string quoted = "\"";

    for (const char letter : token) {
        if (letter == '"' || letter == '\\' || letter == '$' || letter == '`') {
            quoted.push_back('\\');
        }

        quoted.push_back(letter);
    }

    quoted.push_back('"');

    return quoted;
#endif
}

void say(std::string *error, std::string text) {
    if (error != nullptr) {
        *error = std::move(text);
    }
}

constexpr auto TOKENS = std::to_array<Token>({
        {
                .kind = Placeholder::SourcePort,
                .text = "{source_port}",
                .about = "The source port this profile is set to.",
        },
        {
                .kind = Placeholder::Game,
                .text = "{game}",
                .about = "The game this profile is set to.",
        },
        {
                .kind = Placeholder::Addon,
                .text = "{addon_n}",
                .about = "An add-on from the list, counting from one.",
        },
        {
                .kind = Placeholder::Profile,
                .text = "{profile}",
                .about = "The profile's own folder, which the next few sit in.",
        },
        {
                .kind = Placeholder::CfgDir,
                .text = "{cfgdir}",
                .about = "The port config written for this profile.",
        },
        {
                .kind = Placeholder::ExtraCfg,
                .text = "{extracfg}",
                .about = "The second config a vanilla port keeps.",
        },
        {
                .kind = Placeholder::SaveDir,
                .text = "{savedir}",
                .about = "The profile's saves folder, inside {profile}.",
        },
        {
                .kind = Placeholder::SaveFile,
                .text = "{savefile}",
                .about = "The save this profile is set to load.",
        },
        {
                .kind = Placeholder::ReplayDir,
                .text = "{replaydir}",
                .about = "The profile's replays folder, inside {profile}.",
        },
});

constexpr std::string_view ADDON_STEM = "addon_";

// The text between the braces.
constexpr std::string_view nameOf(const Token &token) {
    return token.text.substr(1, token.text.size() - 2);
}

const Token *lookUp(const std::string_view name) {
    const std::span<const Token> all(TOKENS);
    const auto found = std::ranges::find_if(all, [name](const Token &token) {
        return token.kind == Placeholder::Addon ? name.starts_with(ADDON_STEM) : nameOf(token) == name;
    });

    return found == all.end() ? nullptr : &*found;
}

std::string known() {
    std::string out;

    for (size_t at = 0; at < TOKENS.size(); ++at) {
        out += at == 0 ? "" : at + 1 == TOKENS.size() ? " and " : ", ";

        // {addon_n} is a pattern, and typed as it is shown it is refused.
        out += TOKENS[at].kind == Placeholder::Addon ? "{" + std::string(ADDON_STEM) + "1} upwards"
                                                     : std::string(TOKENS[at].text);
    }

    return out;
}

bool substitute(const Config &config, const std::string &name, std::string &value, std::string *error) {
    const Profile &profile = config.activeProfile();
    const Token *token = lookUp(name);

    if (token == nullptr) {
        say(error, "{" + name + "} is not one ZDL4 knows. There is " + known() + ".");

        return false;
    }

    switch (token->kind) {
        case Placeholder::SourcePort:
            value = Launcher::executable(config).string();

            if (value.empty()) {
                say(error,
                    "This profile has no source port, so there is nothing for " + std::string(token->text) + ".");

                return false;
            }

            return true;

        case Placeholder::Game:
            value = config.activeIwadFile();

            if (value.empty()) {
                say(error, "This profile has no game, so there is nothing for " + std::string(token->text) + ".");

                return false;
            }

            return true;

        case Placeholder::Addon: {
            const std::string which = name.substr(ADDON_STEM.size());

            if (!Text::is_int(which) || Text::to_int(which) < 1) {
                say(error, "{" + name + "} has to end in a number, counting from one.");

                return false;
            }

            const int wanted = Text::to_int(which);

            if (std::cmp_greater(wanted, profile.files.size())) {
                say(error,
                    profile.files.empty() ? "This profile has no add-ons, so there is nothing for {" + name + "}."
                                          : "This profile has " + std::to_string(profile.files.size())
                                                    + " add-ons, so there is nothing for {" + name + "}.");

                return false;
            }

            value = profile.files[static_cast<size_t>(wanted - 1)].file;

            return true;
        }

        case Placeholder::ReplayDir: {
            const std::filesystem::path replays = Storage::replayDirectory(config);

            if (replays.empty()) {
                say(error,
                    "This profile has no folder of its own, so there is nothing for " + std::string(token->text) + ".");

                return false;
            }

            value = replays.string();

            return true;
        }

        case Placeholder::SaveFile: {
            const std::filesystem::path save = Storage::saveFile(config);

            if (save.empty()) {
                say(error,
                    "This profile has no save picked, so there is nothing for " + std::string(token->text) + ".");

                return false;
            }

            value = save.string();

            return true;
        }

        case Placeholder::Profile:
        case Placeholder::CfgDir:
        case Placeholder::ExtraCfg:
        case Placeholder::SaveDir: {
            const std::filesystem::path own = Storage::configFile(config);

            if (own.empty()) {
                say(error, "This profile has no folder of its own, so there is nothing for {" + name + "}.");

                return false;
            }

            if (token->kind == Placeholder::Profile) {
                value = own.parent_path().string();
            } else if (token->kind == Placeholder::CfgDir) {
                value = own.string();
            } else if (token->kind == Placeholder::ExtraCfg) {
                value = Storage::extraConfigFile(own).string();
            } else {
                value = Storage::saveDirectory(config).string();
            }

            return true;
        }
    }

    return false;
}

// DOSBox under any of its names, since only it counts -c commands.
bool runsDosbox(const std::string &program) {
    return Text::lower(std::filesystem::path(program).stem().string()).starts_with("dosbox");
}

}

std::span<const Token> tokens() {
    return TOKENS;
}

std::string_view text(const Placeholder kind) {
    const std::span<const Token> all(TOKENS);
    const auto found = std::ranges::find(all, kind, &Token::kind);

    return found == all.end() ? std::string_view() : found->text;
}

bool uses(const std::string &command, const Placeholder kind) {
    return kind == Placeholder::Addon ? command.contains("{" + std::string(ADDON_STEM)) : command.contains(text(kind));
}

std::string example() {
    return std::string(text(Placeholder::SourcePort)) + " -iwad " + std::string(text(Placeholder::Game)) + " -file {"
           + std::string(ADDON_STEM) + "1}";
}

std::vector<std::string> custom(const Config &config, std::string *error) {
    const Profile &profile = config.activeProfile();
    const std::vector<std::string> tokens = Text::parse_arguments(profile.command);

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
            const size_t close = open == std::string::npos ? std::string::npos : token.find('}', open);

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
    const std::string save =
            Dialect::of(config).loads == Dialect::SaveNames::path ? Storage::saveFile(config).string() : std::string();

    const auto spell = [&port, &iwad, &own, &extra, &saves, &save, &profile](const std::string &token) {
        if (!port.empty() && token == port) {
            return std::string(text(Placeholder::SourcePort));
        }

        if (!own.empty() && token == own) {
            return std::string(text(Placeholder::CfgDir));
        }

        if (!extra.empty() && token == extra) {
            return std::string(text(Placeholder::ExtraCfg));
        }

        if (!saves.empty() && token == saves) {
            return std::string(text(Placeholder::SaveDir));
        }

        if (!save.empty() && token == save) {
            return std::string(text(Placeholder::SaveFile));
        }

        if (!iwad.empty() && token == iwad) {
            return std::string(text(Placeholder::Game));
        }

        for (size_t index = 0; index < profile.files.size(); index++) {
            if (profile.files[index].file == token) {
                return "{" + std::string(ADDON_STEM) + std::to_string(index + 1) + "}";
            }
        }

        return quote(token);
    };

    std::vector<std::string> parts;

    // Override drops the prefix row, so the command it starts from carries the prefix in its place.
    for (const std::string &word : Text::parse_arguments(profile.prefix)) {
        parts.push_back(quote(word));
    }

    if (!port.empty()) {
        parts.push_back(spell(port));
    }

    for (const std::string &argument : Arguments::of(config)) {
        parts.push_back(spell(argument));
    }

    return Text::join(parts, " ");
}

int dosSpend(const Config &config) {
    // A typed command is counted as written: it is what DOSBox will be handed.
    if (config.activeProfile().customCommand) {
        const std::vector<std::string> tokens = custom(config, nullptr);

        return tokens.empty() || !runsDosbox(tokens.front()) ? 0 : static_cast<int>(std::ranges::count(tokens, "-c"));
    }

    return Dialect::of(config).dos ? Dos::spent(Dos::command(config)) : 0;
}

namespace {

std::string programLine(const Config &config) {
    std::vector<std::string> parts;

    if (config.activeProfile().customCommand) {
        for (const std::string &token : custom(config, nullptr)) {
            parts.push_back(quote(token));
        }

        return Text::join(parts, " ");
    }

    if (Dialect::of(config).dos) {
        const Dos::Command command = Dos::command(config);

        if (command.arguments.empty()) {
            return {};
        }

        parts.push_back(quote(command.dosbox.string()));

        for (const std::string &argument : command.arguments) {
            parts.push_back(quote(argument));
        }

        return Text::join(parts, " ");
    }

    const std::filesystem::path port = Launcher::executable(config);
    std::filesystem::path directory;

    if (!port.empty()) {
        std::error_code code;

        parts.push_back(quote(port.string()));
        directory = std::filesystem::absolute(port, code).parent_path();
    }

    for (const std::string &argument : Arguments::of(config, directory)) {
        parts.push_back(quote(argument));
    }

    return Text::join(parts, " ");
}

}

std::string line(const Config &config) {
    std::string program = programLine(config);

    if (program.empty()) {
        return program;
    }

    const Profile &profile = config.activeProfile();
    std::vector<std::string> parts;

    for (const EnvVariable &variable : profile.environment) {
        if (!variable.name.empty()) {
            parts.push_back(variable.name + "=" + quote(Launcher::value(variable.value)));
        }
    }

    if (!profile.customCommand) {
        for (const std::string &word : Text::parse_arguments(profile.prefix)) {
            parts.push_back(quote(word));
        }
    }

    parts.push_back(std::move(program));

    return Text::join(parts, " ");
}

}
