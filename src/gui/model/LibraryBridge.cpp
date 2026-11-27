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

#include <span>
#include <utility>
#include <vector>

#include "ttk/system/Text.h"

#include "core/config/Groups.h"
#include "core/launch/Command.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/LibraryBridge.h"
#include "gui/model/ListsBridge.h"
#include "gui/model/ProfileBridge.h"

using namespace ttk;

namespace {

// An engine the list no longer has counts as unset.
std::string gameEngineName(const Config &config) {
    const std::string &chosen = config.general.gameEngine;

    return !chosen.empty() && config.findEngine(chosen) != nullptr ? chosen : config.activeProfile().engine;
}

// A minimal config for a library launch. Copying the whole would carry every file list.
Config oneGame(const Config &config, const std::string &iwad) {
    const Profile &active = config.activeProfile();
    Config made;
    Profile target;

    made.general = config.general;
    made.iwads = config.iwads;
    made.engines = config.engines;

    // DOS staging needs an id even with no profile open.
    target.id = active.id.empty() ? "shelf" : active.id;
    target.name = active.name;
    target.engine = gameEngineName(config);
    target.iwad = iwad;
    target.set(Switch::sharedConfig, true);

    made.activeProfileId = target.id;
    made.profiles.push_back(std::move(target));

    return made;
}

bool contains(const std::string &value, const std::string &needle) {
    return Text::lower(value).contains(Text::lower(needle));
}

template <typename Item, typename Row, typename Make>
void shelve(const std::vector<Item> &members,
            const std::vector<Group> &groups,
            const Groups::Layout &layout,
            const std::string &filter,
            std::vector<Row> &rows,
            std::vector<State::Section> &sections,
            const Make &make) {
    const auto matches = [&members, &filter](const int index) {
        return filter.empty() || contains(members[static_cast<size_t>(index)].name, filter);
    };

    const auto place = [&rows, &matches, &make](const std::span<const int> run) {
        for (const int index : run) {
            if (matches(index)) {
                rows.push_back(make(index));
            }
        }
    };

    if (layout.sections.empty()) {
        place(layout.order);

        return;
    }

    for (const Groups::Section &run : layout.sections) {
        State::Section section = sectionOf(groups, run.group);

        section.collapsed = section.collapsed && filter.empty();
        section.first = static_cast<int>(rows.size());

        if (!section.collapsed) {
            place(std::span<const int>(layout.order)
                          .subspan(static_cast<size_t>(run.first), static_cast<size_t>(run.count)));
        }

        section.count = section.collapsed ? run.count : static_cast<int>(rows.size()) - section.first;

        // The ungrouped rest stays for the tile that adds a card.
        if (run.group != 0 && section.count == 0 && !filter.empty()) {
            continue;
        }

        sections.push_back(std::move(section));
    }
}

}

void LibraryBridge::pushShelf() {
    const bool grouped = config().general.groupLibrary;
    std::vector<State::ProfileCard> profiles;
    std::vector<State::NameRow> games;
    std::vector<State::Section> profileSections;
    std::vector<State::Section> gameSections;
    Groups::Layout layout;

    Groups::arrange(config().profiles, config().profileGroups, grouped, layout);
    shelve(config().profiles, config().profileGroups, layout, _filter, profiles, profileSections, [](const int index) {
        return ProfileBridge::cardOf(index);
    });

    Groups::arrange(config().iwads, config().gameGroups, grouped, layout);
    shelve(config().iwads, config().gameGroups, layout, _filter, games, gameSections, [](const int index) {
        return ListsBridge::rowOf(config().iwads, index);
    });

    // The page rebuilds every card and relayouts the window off this, so it is
    // compared rather than bumped: a keystroke on another page moves cfg.rev, and
    // the shelf has nothing to do with it.
    if (profiles == cfg().shelfProfiles && games == cfg().shelfGames && profileSections == cfg().shelfProfileSections
        && gameSections == cfg().shelfGameSections) {
        return;
    }

    cfg().shelfProfiles = std::move(profiles);
    cfg().shelfGames = std::move(games);
    cfg().shelfProfileSections = std::move(profileSections);
    cfg().shelfGameSections = std::move(gameSections);
    cfg().shelfRev = ++_shelfRev;

    State::get().touch();
}

void LibraryBridge::pushGameRev() {
    std::string mark;

    mark.reserve(_gameMark.size());

    const auto line = [&mark](const std::string &part) {
        mark += part;
        mark += '\n';
    };

    line(config().general.gameEngine);
    line(config().general.alwaysAdd);
    line(config().general.dosbox);
    line(active().engine);
    line(active().id);

    for (const Engine &engine : config().engines) {
        mark += engine.name;
        mark += '\t';
        mark += engine.file;
        mark += engine.dosbox ? "\tdos\n" : "\n";
    }

    for (const GameEntry &game : config().iwads) {
        mark += game.name;
        mark += '\t';
        mark += game.file;
        mark += '\n';
    }

    if (mark == _gameMark) {
        return;
    }

    _gameMark = std::move(mark);
    cfg().gameRev = ++_gameRev;

    State::get().touch();
}

void LibraryBridge::setFilter(const std::string &value) {
    _filter = value;
    cfg().filter = value;

    pushShelf();
}

void LibraryBridge::dropProfile(const int from, const int section, const int place) const {
    Groups::drop(config().profiles,
                 config().profileGroups,
                 cfg().shelfProfiles[static_cast<size_t>(from)].index,
                 cfg().shelfProfileSections[static_cast<size_t>(section)].group,
                 place);

    _hub->scheduleSave();
    _hub->profile().pushCards();
    _hub->profile().push();
}

void LibraryBridge::dropIwad(const int from, const int section, const int place) const {
    Groups::drop(config().iwads,
                 config().gameGroups,
                 cfg().shelfGames[static_cast<size_t>(from)].index,
                 cfg().shelfGameSections[static_cast<size_t>(section)].group,
                 place);

    _hub->scheduleSave();
    _hub->lists().push();
    _hub->profile().touch();
}

std::string LibraryBridge::gameCommandLine(const std::string &iwad) {
    return Command::line(oneGame(config(), iwad));
}

void LibraryBridge::launchGame(const std::string &iwad) const {
    _hub->start(ConfigBridge::gameKey(iwad), iwad, oneGame(config(), iwad));
}
