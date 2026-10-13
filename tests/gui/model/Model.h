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

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/notices/Notice.h"
#include "ttk/notices/Notifier.h"
#include "ttk/system/Process.h"
#include "ttk/system/Text.h"

#include "core/config/Config.h"
#include "core/config/ConfigPaths.h"
#include "core/config/Session.h"
#include "gui/model/ConfigBridge.h"
#include "gui/services/Runs.h"
#include "gui/state/State.h"
#include "support/Fixtures.h"
#include "support/Sandbox.h"
#include "support/Shell.h"

namespace model {

namespace fs = std::filesystem;

// The autosave debounce ConfigBridge waits out.
constexpr double AUTOSAVE = 0.4;

inline NameEntry port(const std::string &name, const std::string &file, const bool dosbox = false) {
    return NameEntry{.name = name, .file = file, .dosbox = dosbox};
}

inline NameEntry game(const std::string &name, const std::string &file) {
    return NameEntry{.name = name, .file = file};
}

inline const std::vector<std::string> &names() {
    static const std::vector<std::string> held = {"Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot"};

    return held;
}

// Profiles named Alpha onwards, each on Doom II and GZDoom with a config of its own.
inline Config world(const int profiles = 3) {
    Config made;

    made.iwads = {game("Doom II", "/games/doom2.wad"), game("Plutonia", "/games/plutonia.wad")};

    made.ports = {
            port("GZDoom", "/ports/gzdoom"),
            port("Chocolate", "/ports/chocolate-doom"),
            port("PrBoom", "/ports/prboom-plus"),
            port("DSDA", "/ports/dsda-doom"),
            port("Woof", "/ports/woof"),
            port("UZDoom", "/ports/uzdoom"),
            port("Vanilla", "/ports/doom2.exe", true),
    };

    for (int at = 0; at < profiles; ++at) {
        const std::string &name = names()[static_cast<size_t>(at) % names().size()];
        Profile profile;

        profile.id = "id-" + ttk::Text::lower(name);
        profile.name = name;
        profile.iwad = "Doom II";
        profile.port = "GZDoom";
        profile.config = ttk::Text::lower(name) + ".cfg";

        made.profiles.push_back(std::move(profile));
    }

    if (!made.profiles.empty()) {
        made.activeProfileId = made.profiles.front().id;
    }

    return made;
}

// The folder a profile from world() keeps its config, saves and replays in.
inline fs::path ownFolder(const std::string &name) {
    return Config::profileFolder(ttk::Text::lower(name));
}

inline void write(const fs::path &file, const std::string &text = "x") {
    std::error_code code;

    fs::create_directories(file.parent_path(), code);

    std::ofstream(file, std::ios::binary) << text;
}

// Written `age` seconds before now, so a listing sorted newest first is known.
inline void writeAged(const fs::path &file, const int age) {
    write(file);

    std::error_code code;

    fs::last_write_time(file, fs::file_time_type::clock::now() - std::chrono::seconds(age), code);
}

// A bridge on its own, as App builds it, with the config saved to a scratch file.
class Bridged : public ::testing::Test {
protected:
    void SetUp() override {
        std::error_code code;

        // A profile folder left by another case would change the names handed out.
        fs::remove_all(Config::profileFolder("any").parent_path(), code);
        fs::remove(ConfigPaths::get().configPath(ConfigPaths::USER), code);

        _folder = tests::Sandbox::scratch("model");

        Session::get().config() = Config{};
        State::get() = State::All{};
        State::get().changed = [this] { ++touches; };

        std::string error;

        ASSERT_TRUE(Session::get().saveAs(_folder / "zdl.json", &error)) << error;

        _runs = std::make_unique<Runs>(&shell);
        _bridge = std::make_unique<ConfigBridge>(&shell, &notifier, _runs.get());
    }

    void TearDown() override {
        _bridge.reset();
        _runs.reset();

        State::get() = State::All{};
        Session::get().config() = Config{};
    }

    void install(Config what) const {
        tests::Fixtures::install(std::move(what));

        _bridge->reload();
    }

    ConfigBridge &bridge() const { return *_bridge; }
    Runs &runs() const { return *_runs; }

    static Config &config() { return Session::get().config(); }
    static State::Cfg &cfg() { return State::get().cfg; }

    const fs::path &folder() const { return _folder; }

    // What is on disk now.
    static Config saved() {
        Config read;

        read.load(Session::get().path());

        return read;
    }

    // Waits out the autosave.
    void settle() { shell.advance(AUTOSAVE); }

    // Runs the command line preview, which is due at once.
    void preview() { shell.advance(0.0); }

    [[nodiscard]] std::vector<ttk::Notice> said(const ttk::Severity severity) const {
        std::vector<ttk::Notice> out;

        for (const ttk::Notice &notice : notifier.messages()) {
            if (notice.severity == severity) {
                out.push_back(notice);
            }
        }

        return out;
    }

    // A run held open with no process behind it.
    void hold(const std::string &id) const {
        _runs->began(ConfigBridge::profileKey(id), id, "", 0, ttk::Process::NOTHING);
    }

    // Gives a launched port the wall time to end, so it does not outlive the sandbox.
    void waitOut(const std::string &key) {
        for (int at = 0; at < 200 && _runs->alive(key); ++at) {
            shell.await(0.02);
            shell.advance(0.5);
        }
    }

    tests::Shell shell;
    ttk::Notifier notifier;
    int touches = 0;

private:
    fs::path _folder;

    std::unique_ptr<Runs> _runs;
    std::unique_ptr<ConfigBridge> _bridge;
};

}
