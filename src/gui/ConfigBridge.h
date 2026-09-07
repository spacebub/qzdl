/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/Config.h"
#include "main.h"
#include "gui/Notifier.h"
#include "gui/Runs.h"

// The only way the interface changes the config. Every setter writes the config,
// pushes what the interface reads and schedules a save; never one without the
// other two.
class ConfigBridge {
public:
    ConfigBridge(const ui::Zdl *window, Notifier *notifier, Runs *runs);

    // Tells every page to read the config again, after it was replaced.
    void reload();

    // Every change made through here schedules one already; anything changing the
    // config behind this object's back calls it for itself.
    void scheduleSave();

    // Writes the config now, if a change is waiting.
    void flush();

    // The source ports, which the engine browser also puts entries into.
    std::string addPort(const std::string &file, const std::string &name, bool dosbox);
    void updatePort(int row, const std::string &name, const std::string &file, bool dosbox);
    void removePort(int row);
    [[nodiscard]] static const std::vector<NameEntry> &ports();

    // The port started, so a config set to close on launch can do it.
    std::function<void()> launched;

    // The whole config was replaced, so what is on disk but not in the list it
    // came with can be put back into it.
    std::function<void()> replaced;

private:
    void bind();

    // Anything that changes what would be launched.
    void touch();

    void pushProfiles();
    void pushProfile();
    void pushMultiplayer();
    void pushGeneral();
    void pushLists();
    void pushMaps();
    void pushCommand();

    // Only ever reached through the timer above it.
    void showCommand();

    void pushPath();
    void pushShelf();

    // Moves the key the library's play hints hang off, and only when one of the
    // things they are worked out from changed.
    void pushGameRev();

    // Profiles name IWADs and ports by name, so a rename has to be carried across
    // rather than quietly emptying every profile on it.
    void renamedIwad(const std::string &before, const std::string &after);
    void renamedPort(const std::string &before, const std::string &after);

    [[nodiscard]] static ui::ProfileCard cardOf(int index);
    [[nodiscard]] static ui::NameRow rowOf(const std::vector<NameEntry> &list, int index,
                                           bool ports);

    // A name nothing else in the list is already called.
    [[nodiscard]] static std::string uniqueName(const std::vector<NameEntry> &list,
                                                const std::string &base, int ignoring = -1);

    // Runs one worked out config, filed under this name.
    bool start(const std::string &key, const std::string &title, const Config &what);

    // Long enough that a field being typed into is one write, not twenty.
    static constexpr std::chrono::milliseconds AUTOSAVE{400};

    // Shorter: the preview is only read, but it is read while typing.
    static constexpr std::chrono::milliseconds PREVIEW{120};

    const ui::Zdl *_window;
    Notifier *_notifier;
    Runs *_runs;

    // Held for the life of the bridge and updated in place: a repeater handed a
    // model it has not seen rebuilds every item. See Models::reconcile.
    std::shared_ptr<slint::VectorModel<ui::ProfileCard>> _profileCards
        = std::make_shared<slint::VectorModel<ui::ProfileCard>>();
    std::shared_ptr<slint::VectorModel<ui::ProfileCard>> _shelfProfiles
        = std::make_shared<slint::VectorModel<ui::ProfileCard>>();
    std::shared_ptr<slint::VectorModel<ui::NameRow>> _shelfGames
        = std::make_shared<slint::VectorModel<ui::NameRow>>();
    std::shared_ptr<slint::VectorModel<ui::FileRow>> _files
        = std::make_shared<slint::VectorModel<ui::FileRow>>();
    std::shared_ptr<slint::VectorModel<ui::NameRow>> _iwads
        = std::make_shared<slint::VectorModel<ui::NameRow>>();
    std::shared_ptr<slint::VectorModel<ui::NameRow>> _ports
        = std::make_shared<slint::VectorModel<ui::NameRow>>();

    slint::Timer _autosave;
    slint::Timer _preview;

    // Whether a change is waiting to be written, and whether saying so failed.
    bool _pendingSave{false};
    bool _warnedSave{false};

    mutable std::vector<std::string> _maps;
    mutable bool _mapsKnown{false};

    // What the two above were worked out from, so neither is redone for a change
    // that cannot have altered it.
    std::string _mapsMark;
    std::string _gameMark;

    std::string _filter;
    int _rev{0};
    int _gameRev{0};
};
