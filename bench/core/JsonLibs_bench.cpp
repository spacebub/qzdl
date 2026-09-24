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

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <benchmark/benchmark.h>
#include <simdjson.h>

#include "ttk/system/Json.h"
#include "ttk/system/Text.h"

#include "core/config/Config.h"
#include "core/config/Schema.h"
#include "support/Corpus.h"
#include "support/Fixtures.h"
#include "support/Sandbox.h"

using namespace ttk;

namespace {

namespace od = simdjson::ondemand;

struct Shape {
    int ports;
    int profiles;
    int addons;
};

Shape shapeOf(const benchmark::State &state) {
    return {static_cast<int>(state.range(0)), static_cast<int>(state.range(1)),
            static_cast<int>(state.range(2))};
}

struct Text {
    std::string plain;
    simdjson::padded_string padded;
};

const Text &text(const Shape shape) {
    static std::map<std::tuple<int, int, int>, Text> held;

    const std::tuple key{shape.ports, shape.profiles, shape.addons};

    if (const auto found = held.find(key); found != held.end()) {
        return found->second;
    }

    std::ifstream in(bench::Corpus::json(shape.ports, shape.profiles, shape.addons), std::ios::binary);
    Text made;

    made.plain.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    made.padded = simdjson::padded_string(made.plain);

    return held.emplace(key, std::move(made)).first->second;
}

void tally(benchmark::State &state, const Text &text) {
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(text.plain.size()));
}

std::string str(od::value value) {
    std::string_view view;

    return value.get_string().get(view) == 0 ? std::string(view) : std::string();
}

int toInt(od::value value, const int def = 0) {
    od::json_type type;

    if (value.type().get(type) != 0) {
        return def;
    }

    if (type == od::json_type::number) {
        int64_t whole = 0;
        double real = 0;

        if (value.get_int64().get(whole) == 0) {
            return static_cast<int>(whole);
        }

        return value.get_double().get(real) == 0 ? static_cast<int>(real) : def;
    }

    if (type == od::json_type::string) {
        std::string_view view;

        return value.get_string().get(view) == 0 ? ::Text::to_int(view, def) : def;
    }

    return def;
}

bool toBool(od::value value, const bool def = false) {
    od::json_type type;

    if (value.type().get(type) != 0) {
        return def;
    }

    if (type == od::json_type::boolean) {
        bool flag = false;

        return value.get_bool().get(flag) == 0 ? flag : def;
    }

    if (type == od::json_type::number) {
        int64_t whole = 0;

        return value.get_int64().get(whole) == 0 ? whole != 0 : def;
    }

    if (type == od::json_type::string) {
        std::string_view view;

        return value.get_string().get(view) == 0 && (view == "1" || ::Text::iequals(view, "true"));
    }

    return def;
}

bool isString(od::value value) {
    od::json_type type;

    return value.type().get(type) == 0 && type == od::json_type::string;
}

template<class F>
void eachField(od::value value, F &&visit) {
    od::object object;

    if (value.get_object().get(object) != 0) {
        return;
    }

    for (auto field : object) {
        std::string_view key;
        od::value inner;

        if (field.escaped_key().get(key) != 0 || field.value().get(inner) != 0) {
            break;
        }

        visit(key, inner);
    }
}

template<class F>
void eachItem(od::value value, F &&visit) {
    od::array array;

    if (value.get_array().get(array) != 0) {
        return;
    }

    for (auto item : array) {
        od::value inner;

        if (item.get(inner) != 0) {
            break;
        }

        visit(inner);
    }
}

bool intPair(od::value value, int *out) {
    od::array array;

    if (value.get_array().get(array) != 0) {
        return false;
    }

    int count = 0;

    for (auto item : array) {
        double real = 0;

        if (item.get_double().get(real) != 0) {
            return false;
        }

        if (count < 2) {
            out[count] = static_cast<int>(real);
        }

        ++count;
    }

    return count >= 2;
}

void readEntries(od::value value, std::vector<NameEntry> &out) {
    eachItem(value, [&out](od::value item) {
        NameEntry entry;

        eachField(item, [&entry](const std::string_view key, od::value val) {
            if (key == ConfigKey::NAME) {
                entry.name = str(val);
            } else if (key == ConfigKey::FILE) {
                entry.file = str(val);
            } else if (key == ConfigKey::DOSBOX) {
                entry.dosbox = toBool(val);
            } else if (key == ConfigKey::PORT_ID) {
                entry.portId = str(val);
            }
        });

        if (!entry.file.empty()) {
            out.push_back(std::move(entry));
        }
    });
}

void readFiles(od::value value, std::vector<FileEntry> &files) {
    eachItem(value, [&files](od::value item) {
        if (isString(item)) {
            files.push_back({.file = str(item), .enabled = true});

            return;
        }

        FileEntry entry;

        eachField(item, [&entry](const std::string_view key, od::value val) {
            if (key == ProfileKey::FILE) {
                entry.file = str(val);
            } else if (key == ProfileKey::ENABLED) {
                entry.enabled = toBool(val, true);
            }
        });

        if (!entry.file.empty()) {
            files.push_back(std::move(entry));
        }
    });
}

void readMultiplayer(od::value value, MultiplayerSettings &m) {
    eachField(value, [&m](const std::string_view key, od::value val) {
        if (key == ProfileKey::GAME_TYPE) {
            m.gameType = gameTypeOf(toInt(val));
        } else if (key == ProfileKey::PLAYERS) {
            m.players = toInt(val);
        } else if (key == ProfileKey::EXTRATIC) {
            m.extratic = toInt(val) != 0;
        } else if (key == ProfileKey::NETMODE) {
            m.netmode = toInt(val, -1);
        } else if (key == ProfileKey::DUP) {
            m.dup = toInt(val);
        } else if (key == ProfileKey::HOST) {
            m.host = str(val);
        } else if (key == ProfileKey::PORT) {
            m.port = str(val);
        } else if (key == ProfileKey::FRAG_LIMIT) {
            m.fragLimit = str(val);
        } else if (key == ProfileKey::TIME_LIMIT) {
            m.timeLimit = str(val);
        } else if (key == ProfileKey::DMFLAGS) {
            m.dmflags = str(val);
        } else if (key == ProfileKey::DMFLAGS2) {
            m.dmflags2 = str(val);
        } else if (key == ProfileKey::SAVEGAME) {
            m.savegame = str(val);
        } else if (key == ProfileKey::LISTED) {
            m.listed = toBool(val);
        }
    });
}

void readReplay(od::value value, ReplaySettings &r) {
    eachField(value, [&r](const std::string_view key, od::value val) {
        if (key == ProfileKey::MODE) {
            r.mode = replayModeOf(toInt(val));
        } else if (key == ProfileKey::FILE) {
            r.file = str(val);
        } else if (key == ProfileKey::PLAYBACK) {
            r.playback = playbackOf(toInt(val));
        } else if (key == ProfileKey::COMPATIBILITY) {
            r.compatibility = toInt(val, -1);
        } else if (key == ProfileKey::LONGTICS) {
            r.longtics = toBool(val);
        } else if (key == ProfileKey::SOLO_NET) {
            r.soloNet = toBool(val);
        }
    });
}

Profile readProfile(od::value value) {
    Profile profile = {};

    eachField(value, [&profile](const std::string_view key, od::value val) {
        if (key == ProfileKey::ID) {
            profile.id = str(val);
        } else if (key == ProfileKey::NAME) {
            profile.name = str(val);
        } else if (key == ProfileKey::IWAD) {
            profile.iwad = str(val);
        } else if (key == ProfileKey::PORT) {
            profile.port = str(val);
        } else if (key == ProfileKey::FILES) {
            readFiles(val, profile.files);
        } else if (key == ProfileKey::SKILL) {
            profile.skill = toInt(val);
        } else if (key == ProfileKey::MONSTERS) {
            profile.monsters = toInt(val);
        } else if (key == ProfileKey::WARP) {
            profile.warp = str(val);
        } else if (key == ProfileKey::EXTRA) {
            profile.extra = str(val);
        } else if (key == ProfileKey::DIALOG_OPEN) {
            profile.dialogOpen = toBool(val);
        } else if (key == ProfileKey::REPLAY_OPEN) {
            profile.replayOpen = toBool(val);
        } else if (key == ProfileKey::SAVE_OPEN) {
            profile.saveOpen = toBool(val);
        } else if (key == ProfileKey::CONFIG) {
            profile.config = str(val);
        } else if (key == ProfileKey::SHARED_CONFIG) {
            profile.sharedConfig = toBool(val);
        } else if (key == ProfileKey::CUSTOM_COMMAND) {
            profile.customCommand = toBool(val);
        } else if (key == ProfileKey::COMMAND) {
            profile.command = str(val);
        } else if (key == ProfileKey::DOS_FULLSCREEN) {
            profile.dosFullscreen = toBool(val, true);
        } else if (key == ProfileKey::DOS_EXIT) {
            profile.dosExit = toBool(val, true);
        } else if (key == ProfileKey::CAPTURE_OUTPUT) {
            profile.captureOutput = toBool(val);
        } else if (key == ProfileKey::LEVELSTAT) {
            profile.levelstat = toBool(val);
        } else if (key == ProfileKey::MULTIPLAYER) {
            readMultiplayer(val, profile.multiplayer);
        } else if (key == ProfileKey::REPLAY) {
            readReplay(val, profile.replay);
        } else if (key == ProfileKey::SAVE) {
            eachField(val, [&profile](const std::string_view sub, od::value inner) {
                if (sub == ProfileKey::ENABLED) {
                    profile.save.enabled = toBool(inner);
                } else if (sub == ProfileKey::FILE) {
                    profile.save.file = str(inner);
                }
            });
        }
    });

    if (profile.id.empty()) {
        profile.id = Profile::newId();
    }

    return profile;
}

void readGeneral(od::value value, GeneralSettings &general) {
    eachField(value, [&general](const std::string_view key, od::value val) {
        if (key == ConfigKey::ALWAYS_ADD) {
            general.alwaysAdd = str(val);
        } else if (key == ConfigKey::DOSBOX) {
            general.dosbox = str(val);
        } else if (key == ConfigKey::DETECTED) {
            eachItem(val, [&general](od::value item) {
                if (isString(item)) {
                    general.detected.push_back(str(item));
                }
            });
        } else if (key == ConfigKey::AUTO_CLOSE) {
            general.autoClose = toBool(val);
        } else if (key == ConfigKey::LAUNCH_ZDL_IMMEDIATELY) {
            general.launchZdlImmediately = toBool(val);
        } else if (key == ConfigKey::SHOW_PATHS) {
            general.showPaths = toBool(val, ConfigDefaults::SHOW_PATHS);
        } else if (key == ConfigKey::NO_USER_CONF) {
            general.noUserConf = toBool(val);
        } else if (key == ConfigKey::SHOW_HIDDEN) {
            general.showHidden = toBool(val);
        } else if (key == ConfigKey::PROFILE_CONFIGS) {
            general.profileConfigs = toBool(val);
        } else if (key == ConfigKey::START_VIEW) {
            general.startView = str(val) == StartViewText::GAMES ? StartView::Games : StartView::Profiles;
        } else if (key == ConfigKey::GAME_PORT) {
            general.gamePort = str(val);
        } else if (key == ConfigKey::THEME) {
            general.theme = isString(val) ? str(val) : std::string(ConfigDefaults::THEME);
        } else if (key == ConfigKey::IS_IMPORTED) {
            general.isImported = toBool(val);
        } else if (key == ConfigKey::IMPORTED_FROM) {
            general.importedFrom = str(val);
        } else if (key == ConfigKey::IMPORT_DATE) {
            general.importDate = str(val);
        } else if (key == ConfigKey::LAST_DIRS) {
            LastDirs &dirs = general.lastDirs;

            eachField(val, [&dirs](const std::string_view sub, od::value inner) {
                if (sub == ConfigKey::GENERAL) {
                    dirs.general = str(inner);
                } else if (sub == ConfigKey::WAD) {
                    dirs.wad = str(inner);
                } else if (sub == ConfigKey::SRC) {
                    dirs.src = str(inner);
                } else if (sub == ConfigKey::SAVE) {
                    dirs.save = str(inner);
                } else if (sub == ConfigKey::ZDL) {
                    dirs.zdl = str(inner);
                } else if (sub == ConfigKey::CONFIG) {
                    dirs.config = str(inner);
                } else if (sub == ConfigKey::REPLAY) {
                    dirs.replay = str(inner);
                }
            });
        } else if (key == ConfigKey::WINDOW) {
            WindowGeometry &window = general.window;

            eachField(val, [&window](const std::string_view sub, od::value inner) {
                int pair[2] = {0, 0};

                if (sub == ConfigKey::SIZE && intPair(inner, pair)) {
                    window.hasSize = true;
                    window.width = pair[0];
                    window.height = pair[1];
                } else if (sub == ConfigKey::POS && intPair(inner, pair)) {
                    window.hasPosition = true;
                    window.x = pair[0];
                    window.y = pair[1];
                }
            });
        }
    });
}

bool loadOnDemand(od::parser &parser, const simdjson::padded_string &padded, Config &config) {
    od::document document;
    od::value root;

    if (parser.iterate(padded).get(document) != 0 || document.get_value().get(root) != 0) {
        return false;
    }

    config.reset();

    eachField(root, [&config](const std::string_view key, od::value val) {
        if (key == ConfigKey::GENERAL) {
            readGeneral(val, config.general);
        } else if (key == ConfigKey::IWADS) {
            readEntries(val, config.iwads);
        } else if (key == ConfigKey::PORTS) {
            readEntries(val, config.ports);
        } else if (key == ConfigKey::PROFILES) {
            eachItem(val, [&config](od::value item) { config.profiles.push_back(readProfile(item)); });
        } else if (key == ConfigKey::ACTIVE_PROFILE) {
            config.activeProfileId = str(val);
        }
    });

    config.ensureActiveProfile();
    config.ensureConfigFiles();

    return true;
}

// Compact only: simdjson's builder has no pretty mode and no automatic commas.
struct Emit {
    simdjson::builder::string_builder &out;
    bool first{true};

    void separate() {
        if (!first) {
            out.append_comma();
        }

        first = false;
    }

    void key(const char *name) {
        separate();
        out.escape_and_append_with_quotes(std::string_view(name));
        out.append_colon();
    }

    void add(const char *name, const std::string_view value) {
        key(name);
        out.escape_and_append_with_quotes(value);
    }

    void add(const char *name, const int value) {
        key(name);
        out.append(static_cast<int64_t>(value));
    }

    void add(const char *name, const bool value) {
        key(name);
        out.append_raw(value ? "true" : "false");
    }

    void open(const char *name, const char bracket) {
        key(name);
        out.append(bracket);
        first = true;
    }

    void openItem(const char bracket) {
        separate();
        out.append(bracket);
        first = true;
    }

    void close(const char bracket) {
        out.append(bracket);
        first = false;
    }

    void entries(const char *name, const std::vector<NameEntry> &list) {
        open(name, '[');

        for (const NameEntry &entry : list) {
            openItem('{');
            add(ConfigKey::NAME, entry.name);
            add(ConfigKey::FILE, entry.file);

            if (entry.dosbox) {
                add(ConfigKey::DOSBOX, true);
            }

            if (!entry.portId.empty()) {
                add(ConfigKey::PORT_ID, entry.portId);
            }

            close('}');
        }

        close(']');
    }

    void profile(const Profile &p) {
        openItem('{');
        add(ProfileKey::ID, p.id);
        add(ProfileKey::NAME, p.name);
        add(ProfileKey::IWAD, p.iwad);
        add(ProfileKey::PORT, p.port);
        open(ProfileKey::FILES, '[');

        for (const FileEntry &entry : p.files) {
            openItem('{');
            add(ProfileKey::FILE, entry.file);
            add(ProfileKey::ENABLED, entry.enabled);
            close('}');
        }

        close(']');
        add(ProfileKey::SKILL, p.skill);
        add(ProfileKey::MONSTERS, p.monsters);
        add(ProfileKey::WARP, p.warp);
        add(ProfileKey::EXTRA, p.extra);
        add(ProfileKey::DIALOG_OPEN, p.dialogOpen);
        add(ProfileKey::REPLAY_OPEN, p.replayOpen);
        add(ProfileKey::SAVE_OPEN, p.saveOpen);
        add(ProfileKey::CONFIG, p.config);
        add(ProfileKey::SHARED_CONFIG, p.sharedConfig);
        add(ProfileKey::CUSTOM_COMMAND, p.customCommand);
        add(ProfileKey::COMMAND, p.command);
        add(ProfileKey::DOS_FULLSCREEN, p.dosFullscreen);
        add(ProfileKey::DOS_EXIT, p.dosExit);
        add(ProfileKey::CAPTURE_OUTPUT, p.captureOutput);
        add(ProfileKey::LEVELSTAT, p.levelstat);

        const MultiplayerSettings &m = p.multiplayer;

        open(ProfileKey::MULTIPLAYER, '{');
        add(ProfileKey::GAME_TYPE, static_cast<int>(m.gameType));
        add(ProfileKey::PLAYERS, m.players);
        add(ProfileKey::EXTRATIC, m.extratic ? 1 : 0);
        add(ProfileKey::NETMODE, m.netmode);
        add(ProfileKey::DUP, m.dup);
        add(ProfileKey::HOST, m.host);
        add(ProfileKey::PORT, m.port);
        add(ProfileKey::FRAG_LIMIT, m.fragLimit);
        add(ProfileKey::TIME_LIMIT, m.timeLimit);
        add(ProfileKey::DMFLAGS, m.dmflags);
        add(ProfileKey::DMFLAGS2, m.dmflags2);
        add(ProfileKey::SAVEGAME, m.savegame);
        add(ProfileKey::LISTED, m.listed);
        close('}');

        const ReplaySettings &r = p.replay;

        open(ProfileKey::REPLAY, '{');
        add(ProfileKey::MODE, static_cast<int>(r.mode));
        add(ProfileKey::FILE, r.file);
        add(ProfileKey::PLAYBACK, static_cast<int>(r.playback));
        add(ProfileKey::COMPATIBILITY, r.compatibility);
        add(ProfileKey::LONGTICS, r.longtics);
        add(ProfileKey::SOLO_NET, r.soloNet);
        close('}');

        open(ProfileKey::SAVE, '{');
        add(ProfileKey::ENABLED, p.save.enabled);
        add(ProfileKey::FILE, p.save.file);
        close('}');
        close('}');
    }

    void config(const Config &c) {
        const GeneralSettings &g = c.general;

        out.clear();
        first = true;
        openItem('{');
        add(ConfigKey::VERSION, Config::SCHEMA_VERSION);
        add(ConfigKey::ENGINE, std::string_view(ConfigFile::ENGINE));
        add(ConfigKey::APP_VERSION, std::string_view(QZDL_VERSION));
        open(ConfigKey::GENERAL, '{');
        add(ConfigKey::ALWAYS_ADD, g.alwaysAdd);
        add(ConfigKey::DOSBOX, g.dosbox);
        add(ConfigKey::AUTO_CLOSE, g.autoClose);
        add(ConfigKey::LAUNCH_ZDL_IMMEDIATELY, g.launchZdlImmediately);
        add(ConfigKey::SHOW_PATHS, g.showPaths);
        add(ConfigKey::NO_USER_CONF, g.noUserConf);
        add(ConfigKey::SHOW_HIDDEN, g.showHidden);
        add(ConfigKey::PROFILE_CONFIGS, g.profileConfigs);
        add(ConfigKey::START_VIEW,
            std::string_view(g.startView == StartView::Games ? StartViewText::GAMES : StartViewText::PROFILES));
        add(ConfigKey::GAME_PORT, g.gamePort);
        add(ConfigKey::THEME, g.theme);
        add(ConfigKey::IS_IMPORTED, g.isImported);
        add(ConfigKey::IMPORTED_FROM, g.importedFrom);
        add(ConfigKey::IMPORT_DATE, g.importDate);
        open(ConfigKey::WINDOW, '{');

        if (g.window.hasSize) {
            open(ConfigKey::SIZE, '[');
            separate();
            out.append(static_cast<int64_t>(g.window.width));
            separate();
            out.append(static_cast<int64_t>(g.window.height));
            close(']');
        }

        if (g.window.hasPosition) {
            open(ConfigKey::POS, '[');
            separate();
            out.append(static_cast<int64_t>(g.window.x));
            separate();
            out.append(static_cast<int64_t>(g.window.y));
            close(']');
        }

        close('}');
        open(ConfigKey::LAST_DIRS, '{');
        add(ConfigKey::GENERAL, g.lastDirs.general);
        add(ConfigKey::WAD, g.lastDirs.wad);
        add(ConfigKey::SRC, g.lastDirs.src);
        add(ConfigKey::SAVE, g.lastDirs.save);
        add(ConfigKey::ZDL, g.lastDirs.zdl);
        add(ConfigKey::CONFIG, g.lastDirs.config);
        add(ConfigKey::REPLAY, g.lastDirs.replay);
        close('}');
        open(ConfigKey::DETECTED, '[');

        for (const std::string &file : g.detected) {
            separate();
            out.escape_and_append_with_quotes(std::string_view(file));
        }

        close(']');
        close('}');
        entries(ConfigKey::IWADS, c.iwads);
        entries(ConfigKey::PORTS, c.ports);
        add(ConfigKey::ACTIVE_PROFILE, c.activeProfileId);
        open(ConfigKey::PROFILES, '[');

        for (const Profile &p : c.profiles) {
            profile(p);
        }

        close(']');
        close('}');
    }
};

bool writeAtomic(const std::filesystem::path &path, const std::string_view json) {
    std::filesystem::path temporary = path;

    temporary += ".new";

    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);

        file.write(json.data(), static_cast<std::streamsize>(json.size()));
        file.put('\n');
        file.close();

        if (!file) {
            return false;
        }
    }

    std::error_code code;

    std::filesystem::rename(temporary, path, code);

    return !code;
}

// Config::save is deterministic, so two configs agree when their files do.
bool same(const Config &left, const Config &right) {
    const std::filesystem::path at = bench::Sandbox::scratch("jsonlibs-same");

    if (!left.save(at / "left.json") || !right.save(at / "right.json")) {
        return false;
    }

    std::ifstream first(at / "left.json", std::ios::binary);
    std::ifstream second(at / "right.json", std::ios::binary);

    return std::string(std::istreambuf_iterator<char>(first), std::istreambuf_iterator<char>())
        == std::string(std::istreambuf_iterator<char>(second), std::istreambuf_iterator<char>());
}

void JsonLibs_parseYyjson(benchmark::State &state) {
    const Text &held = text(shapeOf(state));

    for ([[maybe_unused]] auto step : state) {
        const Json::Doc doc = Json::read_data(held.plain);

        benchmark::DoNotOptimize(doc.root());
    }

    tally(state, held);
}

BENCHMARK(JsonLibs_parseYyjson)->Apply(bench::Fixtures::shapes);

void JsonLibs_parseSimdjsonDom(benchmark::State &state) {
    const Text &held = text(shapeOf(state));
    simdjson::dom::parser parser;

    for ([[maybe_unused]] auto step : state) {
        simdjson::dom::element root;

        if (parser.parse(held.padded).get(root) != 0) {
            state.SkipWithError("the file did not parse");

            break;
        }

        benchmark::DoNotOptimize(root);
    }

    tally(state, held);
}

BENCHMARK(JsonLibs_parseSimdjsonDom)->Apply(bench::Fixtures::shapes);

// On-Demand's structural index alone. The values are read while walking.
void JsonLibs_parseSimdjsonIndex(benchmark::State &state) {
    const Text &held = text(shapeOf(state));
    od::parser parser;

    for ([[maybe_unused]] auto step : state) {
        od::document document;

        if (parser.iterate(held.padded).get(document) != 0) {
            state.SkipWithError("the file did not index");

            break;
        }

        benchmark::DoNotOptimize(document);
    }

    tally(state, held);
}

BENCHMARK(JsonLibs_parseSimdjsonIndex)->Apply(bench::Fixtures::shapes);

void JsonLibs_startupYyjson(benchmark::State &state) {
    const Shape shape = shapeOf(state);
    const std::filesystem::path &path = bench::Corpus::json(shape.ports, shape.profiles, shape.addons);

    for ([[maybe_unused]] auto step : state) {
        Config config;

        if (!config.load(path)) {
            state.SkipWithError("the config did not load");

            break;
        }

        benchmark::DoNotOptimize(config);
    }

    tally(state, text(shape));
}

BENCHMARK(JsonLibs_startupYyjson)->Apply(bench::Fixtures::shapes);

void JsonLibs_startupSimdjson(benchmark::State &state) {
    const Shape shape = shapeOf(state);
    const std::filesystem::path &path = bench::Corpus::json(shape.ports, shape.profiles, shape.addons);
    od::parser parser;

    {
        Config theirs;
        Config ours;

        if (!theirs.load(path) || !loadOnDemand(parser, text(shape).padded, ours) || !same(theirs, ours)) {
            state.SkipWithError("the On-Demand reader disagrees with Config::load");

            return;
        }
    }

    for ([[maybe_unused]] auto step : state) {
        simdjson::padded_string padded;
        Config config;

        if (simdjson::padded_string::load(path.string()).get(padded) != 0
            || !loadOnDemand(parser, padded, config)) {
            state.SkipWithError("the config did not load");

            break;
        }

        benchmark::DoNotOptimize(config);
    }

    tally(state, text(shape));
}

BENCHMARK(JsonLibs_startupSimdjson)->Apply(bench::Fixtures::shapes);

void JsonLibs_saveYyjson(benchmark::State &state) {
    const Shape shape = shapeOf(state);
    const Config config = bench::Fixtures::shaped(shape.ports, shape.profiles, shape.addons);
    const std::filesystem::path at = bench::Sandbox::scratch("jsonlibs-yyjson") / "saved.json";

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(config.save(at));
    }

    tally(state, text(shape));
}

BENCHMARK(JsonLibs_saveYyjson)->Apply(bench::Fixtures::shapes);

void JsonLibs_saveSimdjson(benchmark::State &state) {
    const Shape shape = shapeOf(state);
    const Config config = bench::Fixtures::shaped(shape.ports, shape.profiles, shape.addons);
    const std::filesystem::path at = bench::Sandbox::scratch("jsonlibs-simdjson") / "saved.json";
    simdjson::builder::string_builder builder(text(shape).plain.size());

    {
        Emit{builder}.config(config);

        std::string_view json;
        Config back;

        if (builder.view().get(json) != 0 || !writeAtomic(at, json) || !back.load(at) || !same(config, back)) {
            state.SkipWithError("the string_builder output does not load back to the same Config");

            return;
        }
    }

    for ([[maybe_unused]] auto step : state) {
        Emit{builder}.config(config);

        std::string_view json;

        if (builder.view().get(json) != 0) {
            state.SkipWithError("the builder overflowed");

            break;
        }

        benchmark::DoNotOptimize(writeAtomic(at, json));
    }

    state.counters["compactKB"] = static_cast<double>(builder.size()) / 1024.0;
    tally(state, text(shape));
}

BENCHMARK(JsonLibs_saveSimdjson)->Apply(bench::Fixtures::shapes);

}
