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
#include <cstddef>
#include <filesystem>
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

#include "ttk/system/Text.h"

#include "core/launch/Arguments.h"
#include "core/launch/Command.h"
#include "core/launch/Dialect.h"
#include "core/launch/Dos.h"
#include "core/launch/Launcher.h"
#include "core/launch/Storage.h"
#include "core/ports/Flatpak.h"

using namespace ttk;

namespace Command {

namespace {

// Quoted so a shell, and Text::parse_arguments, hand the token back as it is. Outside Windows
// single quotes keep everything literal, so a quote inside needs no escape.
void quoteOnto(std::string &into, const std::string &token) {
#ifdef _WIN32
    into += Text::quote_argument(token);
#else
    static constexpr std::array<bool, 256> PLAIN = [] {
        std::array<bool, 256> plain{};

        for (const unsigned char letter : std::string_view("_./:=+,@%-")) {
            plain[letter] = true;
        }

        for (int letter = 0; letter < 256; ++letter) {
            plain[letter] = plain[letter] || (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z')
                            || (letter >= '0' && letter <= '9');
        }

        return plain;
    }();

    const bool bare = !token.empty() && std::ranges::all_of(token, [](const char letter) {
        return PLAIN[static_cast<unsigned char>(letter)];
    });

    if (bare) {
        into += token;
    } else if (!token.contains('\'')) {
        into.push_back('\'');
        into += token;
        into.push_back('\'');
    } else {
        into.push_back('"');

        for (const char letter : token) {
            if (letter == '"' || letter == '\\' || letter == '$' || letter == '`') {
                into.push_back('\\');
            }

            into.push_back(letter);
        }

        into.push_back('"');
    }
#endif
}

std::string quote(const std::string &token) {
    std::string quoted;

    quoteOnto(quoted, token);

    return quoted;
}

// One word onto a command line, with a space before every word but the first.
void addWord(std::string &out, const std::string &token) {
    if (!out.empty()) {
        out.push_back(' ');
    }

    quoteOnto(out, token);
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

void reads(std::vector<Arguments::Reached> *reached, const std::filesystem::path &path) {
    if (reached != nullptr && path.is_absolute()) {
        reached->push_back({.path = path, .access = Arguments::Access::reads});
    }
}

void writesIn(std::vector<Arguments::Reached> *reached, const std::filesystem::path &folder) {
    if (reached != nullptr && folder.is_absolute()) {
        reached->push_back({.path = folder, .access = Arguments::Access::writes});
    }
}

// Reached gets what the value sends the port to, as the generated launch would grant it.
bool substitute(const Config &config,
                const Launcher::PortPath &port,
                const std::string &name,
                std::string &value,
                std::vector<Arguments::Reached> *reached,
                std::string *error) {
    const Profile &profile = config.activeProfile();
    const Token *token = lookUp(name);

    if (token == nullptr) {
        say(error, "{" + name + "} is not one ZDL4 knows. There is " + known() + ".");

        return false;
    }

    switch (token->kind) {
        case Placeholder::SourcePort:
            value = port.file.string();

            if (value.empty()) {
                say(error,
                    "This profile has no source port, so there is nothing for " + std::string(token->text) + ".");

                return false;
            }

            return true;

        case Placeholder::Game:
            value = Arguments::beside(config.activeIwadFile(), port.directory);

            if (value.empty()) {
                say(error, "This profile has no game, so there is nothing for " + std::string(token->text) + ".");

                return false;
            }

            reads(reached, std::filesystem::path(value).parent_path());

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

            value = Arguments::beside(profile.files[static_cast<size_t>(wanted - 1)].file, port.directory);
            reads(reached, value);

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
            writesIn(reached, replays);

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
            reads(reached, save);

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

            if (token->kind == Placeholder::SaveDir) {
                const std::filesystem::path saves = Storage::saveDirectory(config);

                value = saves.string();
                writesIn(reached, saves);
            } else {
                // A port may replace a config through a temporary beside it.
                writesIn(reached, own.parent_path());

                if (token->kind == Placeholder::Profile) {
                    value = own.parent_path().string();
                } else if (token->kind == Placeholder::CfgDir) {
                    value = own.string();
                } else {
                    value = Storage::extraConfigFile(own).string();
                }
            }

            return true;
        }
    }

    return false;
}

// DOSBox under any of its names, since only it counts -c commands.
bool runsDosbox(const std::string &file) {
    return Text::lower(std::filesystem::path(file).stem().string()).starts_with("dosbox");
}

int spentBy(const std::vector<std::string> &tokens) {
    return tokens.empty() || !runsDosbox(tokens.front()) ? 0 : static_cast<int>(std::ranges::count(tokens, "-c"));
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

bool startsPort(const std::string &command) {
    return std::ranges::contains(Text::parse_arguments(command), std::string(text(Placeholder::SourcePort)));
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

    const Launcher::PortPath port = Launcher::portPath(config);
    std::vector<Arguments::Reached> reached;
    std::vector<Arguments::Reached> *grants = Flatpak::exported(port.file) ? &reached : nullptr;
    std::vector<size_t> starts;
    std::vector<std::string> out;
    out.reserve(tokens.size());

    for (const std::string &token : tokens) {
        if (token == text(Placeholder::SourcePort) && !port.file.empty()) {
            starts.push_back(out.size());

            continue;
        }

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

            if (!substitute(config, port, token.substr(open + 1, close - open - 1), value, grants, error)) {
                return {};
            }

            filled += value;
            at = close + 1;
        }

        out.push_back(std::move(filled));
    }

    if (starts.empty()) {
        return out;
    }

    // The words that start the port grant what every placeholder sends it to, so they go in last.
    const std::vector<std::string> run = Launcher::portRun(config, port, std::move(reached), error);

    if (run.empty()) {
        return {};
    }

    for (const size_t start : std::views::reverse(starts)) {
        out.insert(out.begin() + static_cast<std::ptrdiff_t>(start), run.begin(), run.end());
    }

    return out;
}

std::string pattern(const Config &config) {
    const Profile &profile = config.activeProfile();
    const Launcher::PortPath port = Launcher::portPath(config);
    const std::filesystem::path &portDirectory = port.directory;
    const std::string file = port.file.string();
    const std::string iwad = Arguments::beside(config.activeIwadFile(), portDirectory);
    const std::string own = Storage::configFile(config).string();
    const std::string extra = own.empty() ? std::string() : Storage::extraConfigFile(own).string();
    const std::string saves = Storage::saveDirectory(config).string();

    // Only a port handed the full path can have it substituted.
    const std::string save =
            Dialect::of(config).loadsBy(Dialect::SaveNames::path) ? Storage::saveFile(config).string() : std::string();

    std::vector<std::string> addons;

    addons.reserve(profile.files.size());

    for (const FileEntry &entry : profile.files) {
        addons.push_back(Arguments::beside(entry.file, portDirectory));
    }

    const auto spell = [&file, &iwad, &own, &extra, &saves, &save, &addons](const std::string &token) {
        if (!file.empty() && token == file) {
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

        for (size_t index = 0; index < addons.size(); index++) {
            if (addons[index] == token) {
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

    if (!file.empty()) {
        parts.push_back(spell(file));
    }

    for (const std::string &argument : Arguments::uncaptured(config, portDirectory)) {
        parts.push_back(spell(argument));
    }

    return Text::join(parts, " ");
}

namespace {

std::string runLine(const Config &config, Shown &shown) {
    std::string out;

    if (config.activeProfile().on(Switch::customCommand)) {
        const std::vector<std::string> tokens = custom(config, &shown.trouble);

        // A typed command is counted as written: it is what DOSBox will be handed.
        shown.dosCommands = spentBy(tokens);

        for (const std::string &token : tokens) {
            addWord(out, token);
        }

        return out;
    }

    if (Dialect::of(config).has(Dialect::Feature::dos)) {
        Dos::Command command = Dos::command(config);

        shown.dosCommands = Dos::spent(command);
        shown.notes = std::move(command.notes);

        if (command.arguments.empty()) {
            return {};
        }

        addWord(out, command.program.string());

        for (const std::string &argument : command.arguments) {
            addWord(out, argument);
        }

        return out;
    }

    Launcher::Program program = Launcher::program(config, &shown.trouble);

    shown.notes = std::move(program.notes);

    if (!program.file.empty()) {
        addWord(out, program.file.string());
    }

    for (const std::string &argument : program.arguments) {
        addWord(out, argument);
    }

    return out;
}

}

Shown shown(const Config &config) {
    Shown out;
    const std::string run = runLine(config, out);

    if (run.empty()) {
        return out;
    }

    const Profile &profile = config.activeProfile();

    // A sandbox gets the variables as --env words in the run part.
    if (!Launcher::sandboxed(config)) {
        for (const auto &[name, value] : Launcher::variables(profile)) {
            if (!out.line.empty()) {
                out.line.push_back(' ');
            }

            out.line += name;
            out.line.push_back('=');
            quoteOnto(out.line, value);
        }
    }

    if (!profile.on(Switch::customCommand)) {
        for (const std::string &word : Text::parse_arguments(profile.prefix)) {
            addWord(out.line, word);
        }
    }

    if (!out.line.empty()) {
        out.line.push_back(' ');
    }

    out.line += run;

    return out;
}

std::string line(const Config &config) {
    return shown(config).line;
}

}
