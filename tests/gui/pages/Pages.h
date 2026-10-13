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

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <blend2d/blend2d.h>
#include <gtest/gtest.h>

#include "ttk/system/Text.h"
#include "ttk/toolkit/Event.h"
#include "ttk/toolkit/Root.h"
#include "ttk/toolkit/Widget.h"
#include "ttk/toolkit/controls/Label.h"

#include "core/config/Config.h"
#include "core/config/Session.h"
#include "gui/state/State.h"
#include "support/Fixtures.h"
#include "support/Rig.h"
#include "support/Sandbox.h"

namespace paging {

template <typename Kind>
void gather(ttk::Widget *from, std::vector<Kind *> &out) {
    if (from == nullptr) {
        return;
    }

    if (auto *hit = dynamic_cast<Kind *>(from); hit != nullptr) {
        out.push_back(hit);
    }

    for (const ttk::Widget::Ptr &child : from->children()) {
        gather(child.get(), out);
    }
}

template <typename Kind>
std::vector<Kind *> all(ttk::Widget *from) {
    std::vector<Kind *> out;

    gather(from, out);

    return out;
}

// The first one whose tooltip starts with `hint`, or null.
template <typename Kind>
Kind *hinted(ttk::Widget *from, const std::string &hint) {
    for (Kind *each : all<Kind>(from)) {
        if (each->hint.starts_with(hint)) {
            return each;
        }
    }

    return nullptr;
}

inline std::vector<std::string> texts(ttk::Widget *from) {
    std::vector<std::string> out;

    for (const ttk::Label *label : all<ttk::Label>(from)) {
        out.push_back(label->text());
    }

    return out;
}

inline bool shows(ttk::Widget *from, const std::string &text) {
    const std::vector<std::string> said = texts(from);

    return std::ranges::find(said, text) != said.end();
}

// Whether `widget` and every parent above it is visible.
inline bool seen(const ttk::Widget *widget) {
    for (const ttk::Widget *at = widget; at != nullptr; at = at->parent()) {
        if (!at->visible()) {
            return false;
        }
    }

    return true;
}

inline void click(ttk::Root &root, const double x, const double y) {
    root.motion(x, y);
    root.press(ttk::Pointer{.x = x, .y = y});
    root.release(ttk::Pointer{.x = x, .y = y});
}

inline void click(ttk::Root &root, const ttk::Widget *widget) {
    const BLRect box = widget->box();

    click(root, box.x + (box.w / 2.0), box.y + (box.h / 2.0));
}

inline NameEntry entry(const std::string &name, const std::string &file, const bool dosbox = false) {
    return NameEntry{.name = name, .file = file, .dosbox = dosbox};
}

// Profiles named Alpha onwards on Doom II and GZDoom, three ports, two games.
inline Config world(const int profiles) {
    static const std::vector<std::string> names = {"Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot"};
    Config made;

    made.iwads = {entry("Doom II", "/games/doom2.wad"), entry("Plutonia", "/games/plutonia.wad")};
    made.ports = {entry("GZDoom", "/ports/gzdoom"),
                  entry("PrBoom", "/ports/prboom-plus"),
                  entry("Vanilla", "/ports/doom2.exe", true)};

    for (int at = 0; at < profiles; ++at) {
        const std::string name =
                at < static_cast<int>(names.size()) ? names[static_cast<size_t>(at)] : "Profile " + std::to_string(at);
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

// The whole interface, with the engines service told what to do with the port list as App tells it.
class Paged : public tests::Rigged {
protected:
    void SetUp() override {
        tests::Rigged::SetUp();

        if (IsSkipped()) {
            return;
        }

        std::string error;

        ASSERT_TRUE(Session::get().saveAs(tests::Sandbox::scratch("pages") / "zdl.json", &error)) << error;

        rig().engines().removePort = [this](const int at) { rig().config().lists().removePort(at); };
        rig().engines().updatePort =
                [this](const int at, const std::string &name, const std::string &file, const bool dos) {
                    rig().config().lists().updatePort(at, name, file, dos);
                };
    }

    void load(Config what) {
        tests::Fixtures::install(std::move(what));

        rig().config().reload();
        rig().turn();
    }

    void show(const State::Page page) {
        rig().go(page);
        rig().turn();
    }

    static State::Cfg &cfg() { return State::get().cfg; }
    static Config &config() { return Session::get().config(); }
};

}
