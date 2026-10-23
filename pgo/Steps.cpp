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

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "ttk/dialogs/ConfirmDialog.h"
#include "ttk/dialogs/DialogLayer.h"
#include "ttk/dialogs/FilePickerDialog.h"
#include "ttk/dialogs/PromptDialog.h"
#include "ttk/draw/Theme.h"
#include "ttk/toolkit/Event.h"
#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/MultistateSwitch.h"
#include "ttk/toolkit/controls/Toggle.h"
#include "ttk/toolkit/layout/Scroll.h"
#include "ttk/toolkit/overlays/Menu.h"

#include "Steps.h"
#include "core/config/Config.h"
#include "core/config/Import.h"
#include "core/config/Schema.h"
#include "core/config/Session.h"
#include "core/launch/Arguments.h"
#include "core/launch/Command.h"
#include "core/launch/Dialect.h"
#include "core/launch/Storage.h"
#include "core/ports/Archive.h"
#include "core/ports/Catalog.h"
#include "core/ports/Detect.h"
#include "core/ports/Install.h"
#include "core/ports/Releases.h"
#include "core/util/Ansi.h"
#include "core/util/Ini.h"
#include "core/util/Md5.h"
#include "core/util/Sha256.h"
#include "core/wad/Artwork.h"
#include "core/wad/FileInfo.h"
#include "core/wad/MapFile.h"
#include "gui/components/AddonList.h"
#include "gui/components/EngineCard.h"
#include "gui/components/LibraryCard.h"
#include "gui/components/LogDock.h"
#include "gui/dialogs/AboutDialog.h"
#include "gui/dialogs/CommandDialog.h"
#include "gui/dialogs/CopyConfigDialog.h"
#include "gui/dialogs/EntryDialog.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/ProfileBridge.h"
#include "gui/pages/EnginesPage.h"
#include "gui/pages/LibraryPage.h"
#include "gui/pages/ProfilePage.h"
#include "gui/pages/SettingsPage.h"
#include "gui/services/Filters.h"
#include "gui/services/Runs.h"
#include "gui/state/State.h"
#include "support/Corpus.h"
#include "support/Fixtures.h"
#include "support/Sandbox.h"

namespace fs = std::filesystem;

namespace {

using components::EngineCard;
using components::LibraryCard;
using pgo::Drive;
using pgo::World;

State::Cfg &cfg() {
    return State::get().cfg;
}

Config &config() {
    return Session::get().config();
}

ttk::Theme::Mode modeOf(const std::string &theme) {
    if (theme == ThemeMode::DARK) {
        return ttk::Theme::Mode::Dark;
    }

    if (theme == ThemeMode::LIGHT) {
        return ttk::Theme::Mode::Light;
    }

    return ttk::Theme::Mode::System;
}

// The ones on the shelf, not those the filter hides.
std::vector<LibraryCard *> cards(Drive &drive) {
    std::vector<LibraryCard *> shown = pgo::all<LibraryCard>(&drive.rig().library());

    std::erase_if(shown, [](const LibraryCard *card) { return !card->visible(); });

    return shown;
}

// A spot on a card clear of its play button and its badges.
BLPoint onCard(const LibraryCard *card) {
    const BLRect box = card->box();

    return BLPoint{box.x + (box.w / 2.0), box.y + 40.0};
}

// The corner the pointer rests in when it is off everything.
void park(Drive &drive) {
    drive.move(4.0, drive.canvas().height() - 4.0);
    drive.rest();
}

void act(Drive &drive, LibraryCard *card, const std::string &label) {
    for (const ttk::Menu::Row &row : card->actions) {
        if (row.label == label && !row.disabled) {
            card->triggered(row.action);

            return;
        }
    }

    drive.check(false, "the card offers " + label);
}

// The largest scrollable area in `from`, or null.
ttk::Scroll *scrollOf(ttk::Widget *from) {
    ttk::Scroll *found = nullptr;

    for (ttk::Scroll *each : pgo::all<ttk::Scroll>(from)) {
        if (each->scrollable() && (found == nullptr || each->box().h > found->box().h)) {
            found = each;
        }
    }

    return found;
}

// Whether any scrollable area in `from` is scrolled down.
bool scrolled(ttk::Widget *from) {
    return std::ranges::any_of(pgo::all<ttk::Scroll>(from),
                               [](const ttk::Scroll *each) { return each->scrollable() && each->offset() > 0.0; });
}

// Three notches down and back up over the largest scrollable area, each glide run
// to its end. A list inside the page takes the wheel over it, as it does for a player.
void scrollThrough(Drive &drive, ttk::Widget *from, const char *where) {
    const ttk::Scroll *scroll = scrollOf(from);

    if (scroll == nullptr) {
        return;
    }

    const BLPoint at = pgo::middle(scroll->box());

    for (int notch = 0; notch < 3; ++notch) {
        drive.wheel(-3.0, at.x, at.y);
        drive.turnFor(0.12);
    }

    drive.rest();
    drive.check(scrolled(from), std::string(where) + ": the wheel scrolled the page");

    for (int notch = 0; notch < 3; ++notch) {
        drive.wheel(3.0, at.x, at.y);
        drive.turnFor(0.12);
    }

    drive.rest();
}

// Across each box, `down` from its top, in steps a pointer makes between two frames.
void sweep(Drive &drive, const std::vector<BLRect> &boxes, const double down, const double step) {
    for (const BLRect &box : boxes) {
        const int count = static_cast<int>((box.w + 16.0) / step);

        for (int at = 0; at <= count; ++at) {
            drive.move(box.x - 8.0 + (at * step), box.y + down);
        }
    }
}

void startup(Drive &drive, const World & /*world*/) {
    // As qzdl_gui.cpp and App::start: the session finds the user config, the saved
    // settings are applied, the services push it and the window settles.
    Session &session = Session::get();

    std::ignore = session.start({});

    const GeneralSettings &general = session.config().general;

    ttk::Theme::set_mode(modeOf(general.theme));

    State::get().nav.shelf = general.startView == StartView::Games ? State::Shelf::Games : State::Shelf::Profiles;

    drive.rig().config().reload();
    drive.rig().engines().relist();
    drive.rig().go(State::Page::Library);
    drive.rig().ready();
    drive.rest();

    drive.check(cards(drive).size() == session.config().profiles.size(), "startup: a card per profile");
}

void idle(Drive &drive, const World & /*world*/) {
    drive.idle(static_cast<int>(drive.hz() * 2.0));
}

void libraryHover(Drive &drive, const World & /*world*/) {
    const std::vector<LibraryCard *> shelf = cards(drive);

    if (shelf.empty()) {
        return;
    }

    // Along the first rows, a card in a few frames and a pause on every third.
    for (size_t at = 0; at < shelf.size() && at < 9; ++at) {
        sweep(drive, {shelf[at]->box()}, shelf[at]->box().h * 0.45, 9.0);

        if (at % 3 == 0) {
            drive.idle(24);
        }
    }

    // Wandering inside one card, which is where the pointer spends most of its time.
    const BLRect box = shelf.front()->box();

    for (int step = 0; step < 48; ++step) {
        drive.move(box.x + 40.0 + (step * 3.0), box.y + 60.0 + ((step % 7) * 2.0));
    }

    drive.idle(30);
    park(drive);
}

void libraryScroll(Drive &drive, const World & /*world*/) {
    scrollThrough(drive, &drive.rig().library(), "library");
}

void libraryFilter(Drive &drive, const World & /*world*/) {
    const std::vector<ttk::Field *> fields = pgo::all<ttk::Field>(&drive.rig().library());

    if (fields.size() != 1) {
        drive.check(false, "library: one filter field");

        return;
    }

    const size_t whole = cfg().shelfProfiles.size();

    drive.ui().focus(fields.front()->input());
    drive.turn();
    drive.type("anci");
    drive.rest();

    drive.check(cfg().filter == "anci", "library: the filter follows the keys");

    if (whole > 1) {
        drive.check(cfg().shelfProfiles.size() < whole, "library: the filter narrowed the shelf");
    }

    for (int at = 0; at < 4; ++at) {
        drive.key(ttk::Code::Backspace);
    }

    drive.rest();
    drive.check(cfg().filter.empty(), "library: the filter cleared");

    drive.ui().focus(nullptr);
    drive.turn();
}

void libraryShelves(Drive &drive, const World & /*world*/) {
    State::get().nav.shelf = State::Shelf::Games;
    State::get().touch();
    drive.rest();

    const std::vector<LibraryCard *> games = cards(drive);

    drive.check(games.size() == config().iwads.size(), "library: a card per game");

    std::vector<BLRect> boxes;

    for (size_t at = 0; at < games.size() && at < 4; ++at) {
        boxes.push_back(games[at]->box());
    }

    sweep(drive, boxes, 40.0, 12.0);
    drive.idle(20);
    park(drive);

    State::get().nav.shelf = State::Shelf::Profiles;
    State::get().touch();
    drive.rest();
}

void libraryReorder(Drive &drive, const World & /*world*/) {
    std::vector<LibraryCard *> shelf = cards(drive);

    if (shelf.size() < 3 || !shelf.front()->draggable) {
        return;
    }

    const std::string first = config().profiles.front().name;
    const BLPoint from = onCard(shelf[0]);
    const BLRect onto = shelf[2]->box();

    drive.drag(from.x, from.y, onto.x + (onto.w / 2.0) + 30.0, from.y, 14);
    drive.rest();

    drive.check(config().profiles.front().name != first, "library: the drop reordered the profiles");

    // And back, so the sitting goes on with the shelf as it was.
    shelf = cards(drive);

    if (shelf.size() >= 3) {
        const BLPoint back = onCard(shelf[2]);
        const BLRect head = shelf[0]->box();

        drive.drag(back.x, back.y, head.x + (head.w / 2.0) - 30.0, back.y, 14);
        drive.rest();
    }
}

void libraryActions(Drive &drive, const World & /*world*/) {
    std::vector<LibraryCard *> shelf = cards(drive);

    if (shelf.empty()) {
        return;
    }

    const size_t count = config().profiles.size();

    act(drive, shelf.front(), "Duplicate");
    drive.rest();

    shelf = cards(drive);

    drive.check(shelf.size() == count + 1, "library: duplicate added a card");

    if (shelf.size() != count + 1) {
        return;
    }

    const size_t prompts = drive.rig().prompts.size();

    act(drive, shelf.back(), "Rename");
    drive.turn();

    if (drive.rig().prompts.size() > prompts) {
        drive.rig().prompts.back().accepted("Renamed copy");
        drive.rest();
    }

    const size_t asks = drive.rig().asks.size();

    shelf = cards(drive);

    act(drive, shelf.back(), "Delete");
    drive.turn();

    if (drive.rig().asks.size() > asks) {
        drive.rig().asks.back().accepted();
        drive.rest();
    }

    drive.check(cards(drive).size() == count, "library: delete took the copy away");
}

void openProfile(Drive &drive, const World & /*world*/) {
    const std::vector<LibraryCard *> shelf = cards(drive);

    if (shelf.empty()) {
        drive.tab(State::Page::Profile);
    } else {
        const BLPoint at = onCard(shelf[std::min<size_t>(1, shelf.size() - 1)]);

        drive.click(at.x, at.y);
    }

    drive.rest();
    drive.check(State::get().sys.page == State::Page::Profile && drive.rig().hasProfile(), "profile: the page opened");
}

void profileEdit(Drive &drive, const World & /*world*/) {
    if (!drive.rig().hasProfile()) {
        return;
    }

    if (config().profiles.empty()) {
        drive.idle(10);

        return;
    }

    pages::ProfilePage &page = drive.rig().profile();
    ConfigBridge &bridge = drive.rig().config();
    const int opened = cfg().profileIndex;

    // Typing into the extra arguments, with the preview recomputed per keystroke.
    if (const auto *extra = pgo::captioned<ttk::Field>(&page, "Extra arguments"); extra != nullptr) {
        drive.ui().focus(extra->input());
        drive.turn();
        drive.key('a', true);
        drive.type("-fast -respawn -nomonsters");
        drive.rest();

        drive.check(cfg().extra == "-fast -respawn -nomonsters", "profile: the extra arguments follow the keys");
        drive.check(cfg().commandLine.contains("-respawn"), "profile: the command preview caught up");

        drive.ui().focus(nullptr);
        drive.turn();
    } else {
        drive.check(false, "profile: an extra arguments field");
    }

    for (int skill = 1; skill <= 5; ++skill) {
        bridge.profile().setSkill(skill);
        drive.turn();
    }

    for (int monsters = 0; monsters < 3; ++monsters) {
        bridge.profile().setMonsters(monsters);
        drive.turn();
    }

    bridge.profile().setWarp("MAP07");
    drive.rest();
    bridge.profile().setWarp("");
    drive.rest();

    // Every port in turn: a dialect each, which decides what the panels offer.
    const std::string port = cfg().port;

    for (const NameEntry &each : config().ports) {
        bridge.profile().setPort(each.name);
        drive.rest();
    }

    bridge.profile().setPort(port);
    drive.rest();

    bridge.panels().setMultiplayerOpen(true);
    drive.rest();
    bridge.panels().setGameType(GameType::Coop);
    drive.turn();
    bridge.panels().setPlayers(4);
    drive.turn();
    bridge.panels().setHost("10.0.0.7");
    drive.turn();
    bridge.panels().setNetPort("5030");
    drive.turn();
    bridge.panels().clearMultiplayer();
    drive.turn();
    bridge.panels().setMultiplayerOpen(false);
    drive.rest();

    bridge.panels().setReplayOpen(true);
    drive.rest();
    bridge.panels().setReplayMode(ReplayMode::Record);
    drive.turn();
    bridge.panels().setReplayFile("second-run");
    drive.turn();
    bridge.panels().setReplayMode(ReplayMode::Play);
    drive.turn();
    bridge.panels().setReplayMode(ReplayMode::Off);
    drive.turn();
    bridge.panels().setReplayOpen(false);
    drive.rest();

    bridge.panels().setSaveOpen(true);
    drive.rest();
    bridge.panels().setSaveEnabled(true);
    drive.turn();
    bridge.panels().setSaveEnabled(false);
    drive.turn();
    bridge.panels().setSaveOpen(false);
    drive.rest();

    scrollThrough(drive, &page, "profile");

    // Other profiles, as the chooser picks them.
    const int count = static_cast<int>(config().profiles.size());

    for (int at = 1; at <= 4 && at < count; ++at) {
        bridge.profile().setProfileIndex(at);
        drive.rest();
    }

    bridge.profile().setProfileIndex(opened);
    drive.rest();
}

void profileFiles(Drive &drive, const World &world) {
    if (!drive.rig().hasProfile() || config().profiles.empty()) {
        return;
    }

    ConfigBridge &bridge = drive.rig().config();
    const std::vector<components::AddonList *> lists = pgo::all<components::AddonList>(&drive.rig().profile());

    if (lists.size() != 1) {
        drive.check(false, "profile: one add-on list");

        return;
    }

    // As drops from the picker arrive: the four add-ons past the ones every profile has.
    const std::vector<std::string> &addons = world.addons();

    bridge.lists().addFiles({addons[8], addons[9], addons[10], addons[11]});
    drive.rest();

    const size_t count = config().activeProfile().files.size();

    drive.check(count >= 4, "profile: the add-ons were added");

    // The rows: the check, the grip and the cross.
    const BLRect box = lists.front()->box();
    const double row = components::AddonList::rowHeight();
    const auto rowY = [&](const int at) { return box.y + (at * row) + (row / 2.0); };

    drive.click(box.x + 33.0, rowY(1));
    drive.rest();
    drive.click(box.x + 33.0, rowY(1));
    drive.rest();

    const std::string first = config().activeProfile().files.front().file;

    drive.drag(box.x + 10.0, rowY(0), box.x + 10.0, rowY(2), 10);
    drive.rest();

    drive.check(config().activeProfile().files.front().file != first, "profile: the grip reordered the files");

    drive.click(box.x + box.w - 19.0, rowY(0));
    drive.rest();

    drive.check(config().activeProfile().files.size() == count - 1, "profile: the cross removed a file");

    for (int at = 0; at < 4; ++at) {
        for (int step = 0; step * 40.0 < box.w - 5.0; ++step) {
            drive.move(box.x + 5.0 + (step * 40.0), rowY(at));
        }
    }

    park(drive);

    bridge.lists().moveFile(0, 2);
    drive.turn();
    bridge.lists().setFileEnabled(0, false);
    drive.turn();
    bridge.lists().setFileEnabled(0, true);
    drive.turn();
    bridge.lists().clearFiles();
    drive.rest();
    bridge.lists().addFiles({addons[0], addons[1], addons[2]});
    drive.rest();
}

void dialogs(Drive &drive, const World &world) {
    tests::Rig &rig = drive.rig();
    ttk::DialogLayer &layer = rig.dialogs();

    const auto up = [&](std::unique_ptr<ttk::Dialog> dialog) {
        ttk::Dialog *raw = layer.show(std::move(dialog));

        drive.rest();

        return raw;
    };

    const auto down = [&] {
        layer.dismiss();
        drive.rest();
    };

    const auto accept = [&](ttk::Widget *dialog) {
        const std::vector<ttk::Button *> buttons = pgo::all<ttk::Button>(dialog);

        if (!buttons.empty()) {
            drive.click(buttons.back());
        }

        drive.rest();
    };

    up(std::make_unique<dialogs::AboutDialog>());
    drive.idle(12);
    down();

    up(std::make_unique<dialogs::CommandDialog>([] {}));
    drive.idle(12);
    down();

    bool accepted = false;

    accept(up(std::make_unique<ttk::ConfirmDialog>(
            "Delete this profile?", "Its folder goes with it.", "Delete", true, [&accepted] { accepted = true; })));

    drive.check(accepted && !layer.covered(), "dialogs: the confirmation was accepted");

    std::string named;

    up(std::make_unique<ttk::PromptDialog>(
            "New profile", "Name", "New profile", "Create", [&named](const std::string &typed) { named = typed; }));
    drive.key('a', true);
    drive.type("Ultimate");
    drive.key(ttk::Code::Return);
    drive.rest();

    drive.check(named == "Ultimate" && !layer.covered(), "dialogs: the prompt took the name");

    std::string edited;

    ttk::Dialog *entry = up(std::make_unique<dialogs::EntryDialog>(
            "Edit Doom",
            dialogs::EntryDialog::Kind::Game,
            Filters::wad(),
            LastDir::WAD,
            "Doom",
            world.games().empty() ? std::string() : world.games().front().file,
            false,
            false,
            rig.files(),
            [&edited](const std::string &name, const std::string & /*file*/, bool /*dos*/) { edited = name; }));

    // The name has focus and is selected once open, so the typing replaces it.
    drive.type("Doom Registered");
    accept(entry);

    drive.check(edited == "Doom Registered", "dialogs: the entry saved the name");

    if (!config().profiles.empty()) {
        ProfileBridge::pushConfigDonors();

        std::string picked;
        ttk::Dialog *copy =
                up(std::make_unique<dialogs::CopyConfigDialog>([&picked](const std::string &id) { picked = id; }));
        const std::vector<ttk::Scroll *> rows = pgo::all<ttk::Scroll>(copy);

        if (!rows.empty() && !cfg().configDonors.empty()) {
            const BLRect box = rows.front()->box();

            drive.click(box.x + 40.0, box.y + 23.0);
            accept(copy);
        } else {
            down();
        }
    }

    // The picker over the games folder: a look down the list, one marked and taken.
    rig.files().open("Add games", Filters::wad(), false, false, true, LastDir::WAD, [](const auto &, bool) {});

    ttk::Dialog *picker = up(std::make_unique<ttk::FilePickerDialog>(rig.files()));

    rig.files().go(world.gamesDirectory().string());
    drive.rest();

    drive.check(rig.files().state().entries.size() == world.games().size(), "dialogs: the picker lists the games");

    if (const std::vector<ttk::Scroll *> rows = pgo::all<ttk::Scroll>(picker); !rows.empty()) {
        const BLRect box = rows.front()->box();

        for (int at = 0; at < 5; ++at) {
            drive.move(box.x + 100.0, box.y + 17.0 + (at * 34.0));
        }

        drive.wheel(-2.0, pgo::middle(box).x, pgo::middle(box).y);
        drive.rest();
    }

    if (!world.games().empty()) {
        rig.files().mark(world.games().front().file);
        drive.rest();
    }

    rig.files().choose_marked();
    drive.turn();

    if (!rig.files().state().open) {
        layer.dismiss();
    }

    drive.rest();
    drive.check(!layer.covered(), "dialogs: the picker closed on its choice");

    rig.notify().info("A game was added to the library.", "Added");
    rig.notify().success("The command line is on the clipboard.");
    rig.notify().warning("This profile has no source port to run.", "Nothing to launch");
    rig.notify().error("Could not write the config.", "Not saved");
    drive.turnFor(6.0);
    drive.rest();
}

void launch(Drive &drive, const World &world) {
    if (config().profiles.empty()) {
        return;
    }

    tests::Rig &rig = drive.rig();
    ConfigBridge &bridge = rig.config();
    Runs &runs = rig.runs();

    const auto profileOn = [&](const int port) {
        const std::vector<Profile> &profiles = config().profiles;

        for (size_t at = 0; at < profiles.size(); ++at) {
            if (profiles[at].port == world.ports()[static_cast<size_t>(port)].name) {
                return static_cast<int>(at);
            }
        }

        return -1;
    };

    const auto closed = [&](const std::string &key) {
        return drive.until([&] { return runs.stateOf(key) == State::RunState::Closed; }, 8.0);
    };

    // A profile on the chatty port, recording the game's output into the dock.
    const int chatty = profileOn(World::CHATTY);

    if (chatty >= 0) {
        bridge.profile().setProfileIndex(chatty);
        drive.rest();
        bridge.profile().setCaptureOutput(true);
        drive.turn();

        const std::string key = ConfigBridge::profileKey(config().activeProfileId);

        bridge.profile().launch();
        drive.turn();

        drive.check(runs.stateOf(key) == State::RunState::Launching, "launch: the port is launching");

        runs.show(key);
        drive.rest();

        drive.check(closed(key), "launch: the chatty port closed");
        drive.turnFor(0.2);

        bool seen = false;
        bool clean = true;

        for (const State::LogRow &row : State::get().runs.lines) {
            seen = seen || row.line == "Init: subsystem 47 ready";
            clean = clean && !row.line.contains('\033');
        }

        drive.check(seen && clean, "launch: the dock shows the port's lines with the escapes gone");

        if (const ttk::Scroll *log = scrollOf(&rig.logs()); log != nullptr) {
            const BLPoint at = pgo::middle(log->box());

            drive.wheel(3.0, at.x, at.y);
            drive.rest();
            drive.wheel(-3.0, at.x, at.y);
            drive.rest();
        }

        runs.hide();
        drive.rest();
        runs.close(key);
        drive.rest();
    }

    // From the library: the play button on a card, and a game run on its own.
    drive.tab(State::Page::Library);

    if (const std::vector<LibraryCard *> shelf = cards(drive); chatty >= 0 && std::cmp_less(chatty, shelf.size())) {
        shelf[static_cast<size_t>(chatty)]->played();
        drive.turn();

        drive.check(closed(ConfigBridge::profileKey(config().profiles[static_cast<size_t>(chatty)].id)),
                    "launch: the card's play button ran the port");
    }

    if (!config().iwads.empty()) {
        const std::string game = config().iwads.front().name;

        bridge.library().launchGame(game);
        drive.turn();

        drive.check(closed(ConfigBridge::gameKey(game)), "launch: a game ran on the library's port");
    }

    drive.rest();

    // A port that holds on, stopped from its tab.
    if (const int waiting = profileOn(World::WAITING); waiting >= 0) {
        bridge.profile().setProfileIndex(waiting);
        drive.rest();
        bridge.profile().setCaptureOutput(true);
        drive.turn();

        const std::string key = ConfigBridge::profileKey(config().activeProfileId);

        bridge.profile().launch();
        drive.turnFor(0.6);

        drive.check(runs.alive(key), "launch: the waiting port stays up");

        runs.close(key);
        drive.turn();

        drive.check(runs.stateOf(key) == State::RunState::Stopping, "launch: closing asks the port to stop");
        drive.check(closed(key), "launch: the stopped port closed");

        runs.close(key);
        drive.rest();
    }

    // A DOS port, staged and run under DOSBox.
    if (const int dos = profileOn(World::DOS); dos >= 0) {
        bridge.profile().setProfileIndex(dos);
        drive.rest();

        const std::string key = ConfigBridge::profileKey(config().activeProfileId);

        bridge.profile().launch();
        drive.turn();

        drive.check(closed(key), "launch: the DOS port ran under DOSBox");

        runs.close(key);
        drive.rest();
    }
}

void engines(Drive &drive, const World & /*world*/) {
    drive.tab(State::Page::Engines);
    drive.check(State::get().sys.page == State::Page::Engines && drive.rig().hasEnginesPage(),
                "engines: the page opened");

    if (!drive.rig().hasEnginesPage()) {
        return;
    }

    pages::EnginesPage &page = drive.rig().enginesPage();
    std::vector<EngineCard *> shelf = pgo::all<EngineCard>(&page);

    for (size_t at = 0; at < shelf.size() && at < 6; ++at) {
        sweep(drive, {shelf[at]->box()}, 24.0, 12.0);
        drive.idle(8);
    }

    park(drive);

    if (shelf.size() >= 3 && shelf.front()->draggable) {
        const std::string first = config().ports.front().name;
        const BLRect from = shelf[0]->box();
        const BLRect onto = shelf[2]->box();

        drive.drag(from.x + 60.0, from.y + 30.0, onto.x + (onto.w / 2.0) + 30.0, from.y + 30.0, 14);
        drive.rest();

        drive.check(config().ports.front().name != first, "engines: the drop reordered the ports");

        shelf = pgo::all<EngineCard>(&page);

        if (shelf.size() >= 3) {
            const BLRect back = shelf[2]->box();
            const BLRect head = shelf[0]->box();

            drive.drag(back.x + 60.0, back.y + 30.0, head.x + (head.w / 2.0) - 30.0, back.y + 30.0, 14);
            drive.rest();
        }
    }

    // Editing a port, answered as the window would.
    shelf = pgo::all<EngineCard>(&page);

    if (shelf.size() > 1) {
        const size_t edits = drive.rig().edits.size();

        shelf[1]->opened();
        drive.turn();

        if (drive.rig().edits.size() > edits) {
            const tests::Edit &asked = drive.rig().edits.back();

            asked.accepted(asked.name, asked.file, asked.dosbox);
            drive.rest();
        }
    }

    State::get().nav.engines = State::EnginesTab::Browse;
    State::get().touch();
    drive.rest();

    const std::vector<EngineCard *> rows = pgo::all<EngineCard>(&page);

    drive.check(!rows.empty(), "engines: the catalogue has rows");

    for (size_t at = 0; at < rows.size() && at < 4; ++at) {
        sweep(drive, {rows[at]->box()}, 24.0, 24.0);
    }

    scrollThrough(drive, &page, "engines");

    drive.rig().engines().measure();
    drive.rest();

    State::get().nav.engines = State::EnginesTab::Installed;
    State::get().touch();
    drive.rest();
    park(drive);
}

void settings(Drive &drive, const World & /*world*/) {
    drive.tab(State::Page::Settings);
    drive.check(State::get().sys.page == State::Page::Settings && drive.rig().hasSettings(),
                "settings: the page opened");

    if (!drive.rig().hasSettings()) {
        return;
    }

    pages::SettingsPage &page = drive.rig().settings();

    if (const std::vector<ttk::Field *> fields = pgo::all<ttk::Field>(&page); !fields.empty()) {
        drive.ui().focus(fields.front()->input());
        drive.turn();
        drive.key('a', true);
        drive.type("-nomusic");
        drive.rest();

        drive.check(config().general.alwaysAdd == "-nomusic", "settings: the arguments follow the keys");

        drive.ui().focus(nullptr);
        drive.turn();
    }

    for (const char *hint : {"Quit ZDL4 as soon as",
                             "Show the directory a file came from",
                             "Each profile keeps the source port's settings"}) {
        auto *toggle = pgo::hinted<ttk::Toggle>(&page, hint);

        if (toggle == nullptr) {
            drive.check(false, std::string("settings: a switch hinting ") + hint);

            continue;
        }

        for (int flip = 0; flip < 2; ++flip) {
            drive.ui().focus(toggle);
            drive.key(ttk::Code::Space);
            drive.rest();
        }
    }

    if (const std::vector<ttk::MultistateSwitch *> switches = pgo::all<ttk::MultistateSwitch>(&page);
        !switches.empty()) {
        switches.front()->key(ttk::Key{.code = ttk::Code::Right});
        drive.rest();
        switches.front()->key(ttk::Key{.code = ttk::Code::Left});
        drive.rest();
    }

    for (const ttk::Button *button : pgo::all<ttk::Button>(&page)) {
        if (button->visible()) {
            const BLPoint at = pgo::middle(button->box());

            drive.move(at.x, at.y);
        }
    }

    park(drive);
    scrollThrough(drive, &page, "settings");
}

void theme(Drive &drive, const World & /*world*/) {
    const ttk::Theme::Mode was = ttk::Theme::mode();

    for (int cycle = 0; cycle < 3; ++cycle) {
        ttk::Theme::cycle_mode();
        drive.ui().damage_all();
        drive.rest();

        // A look round the shelf in the new palette.
        const std::vector<LibraryCard *> shelf = cards(drive);
        std::vector<BLRect> boxes;

        for (size_t at = 0; at < shelf.size() && at < 3; ++at) {
            boxes.push_back(shelf[at]->box());
        }

        sweep(drive, boxes, 40.0, 16.0);
        park(drive);
    }

    ttk::Theme::set_mode(was);
    drive.ui().damage_all();
    drive.rest();
}

void resize(Drive &drive, const World & /*world*/) {
    const int width = drive.canvas().width();
    const int height = drive.canvas().height();

    // A corner dragged in and back out: a relayout and a whole frame per step.
    for (int step = 1; step <= 6; ++step) {
        drive.canvas().resize(width - (step * 24), height - (step * 12));
        drive.ui().relayout();
        drive.turn();
    }

    drive.rest();

    for (int step = 5; step >= 0; --step) {
        drive.canvas().resize(width - (step * 24), height - (step * 12));
        drive.ui().relayout();
        drive.turn();
    }

    drive.rest();
    drive.check(drive.canvas().width() == width && drive.canvas().height() == height,
                "resize: the window is back to its size");
}

void persist(Drive &drive, const World & /*world*/) {
    // The autosave the last edit scheduled, then the write on the way out.
    drive.rig().shell().advance(0.5);
    drive.turn();
    drive.rig().config().flush();

    std::string error;

    drive.check(Session::get().save(&error), "persist: the config was written: " + error);

    std::error_code code;

    drive.check(fs::file_size(Session::get().path(), code) > 0, "persist: the config file has content");
}

constexpr std::array STEPS = std::to_array<pgo::Step>({
        {"startup", startup},
        {"idle", idle},
        {"library-hover", libraryHover},
        {"library-scroll", libraryScroll},
        {"library-filter", libraryFilter},
        {"library-shelves", libraryShelves},
        {"library-reorder", libraryReorder},
        {"library-actions", libraryActions},
        {"open-profile", openProfile},
        {"profile-edit", profileEdit},
        {"profile-files", profileFiles},
        {"dialogs", dialogs},
        {"launch", launch},
        {"engines", engines},
        {"settings", settings},
        {"theme", theme},
        {"resize", resize},
        {"persist", persist},
});

// What a port prints between two escapes, as the Ansi benchmark spells it.
std::string decorated() {
    const std::string clean = "\0337\033[24;0H\033[0J\0338";
    const std::string bar =
            "\0337\033[24;0H\033[2K[" + std::string(30, '=') + std::string(48, '.') + "\033[24;80H]\0338";
    std::string out;

    for (int at = 0; at < 64; at++) {
        out += clean;
        out += "\033[38;2;223;223;223m";
        out += tests::Fixtures::words(8 + (at % 5), static_cast<unsigned>(at) + 1U);
        out += "\033[0m\r\n";
        out += bar;
    }

    return out;
}

}

namespace pgo {

std::span<const Step> steps() {
    return STEPS;
}

void core(const World &world, Tally &tally) {
    std::size_t items = 0;

    const auto ok = [&tally](const bool value, const char *what) {
        if (!value) {
            tally.failed.emplace_back(what);
        }
    };

    const fs::path scratch = tests::Sandbox::scratch("core");

    // The config as a start and a save handle it, at the sizes people have.
    for (const auto [ports, profiles, addons] : {std::tuple{8, 12, 3}, std::tuple{12, 40, 8}, std::tuple{32, 64, 16}}) {
        const Config made = tests::Fixtures::shaped(ports, profiles, addons);
        const fs::path at = scratch / ("shaped-" + std::to_string(profiles) + ".json");

        ok(made.save(at), "core: a shaped config saves");

        for (int rep = 0; rep < 3; ++rep) {
            Config loaded;

            ok(loaded.load(at) && std::cmp_equal(loaded.profiles.size(), profiles), "core: a shaped config loads");

            const Config copy = loaded;

            items += copy.profiles.size();
        }
    }

    // A legacy config, read the once a migration happens, and the .zdl a profile is shared as.
    {
        Config imported;

        ok(Import::loadLegacyFile(tests::Corpus::ini(), imported), "core: the legacy config imports");

        const Profile profile = world.config(4, 3).profiles[1];
        const fs::path zdl = scratch / "shared.zdl";
        Profile back;

        ok(Import::saveZdlFile(zdl, profile) && Import::loadZdlFile(zdl, back) && back.name == profile.name,
           "core: a .zdl round-trips");

        Ini ini;

        ok(ini.read(tests::Corpus::ini()) && ini.write(scratch / "copy.ini"), "core: an ini round-trips");

        items += 3;
    }

    // The launch line for every port, which the preview builds per keystroke.
    {
        Config launching = world.config(8, 4);

        for (const Profile &profile : launching.profiles) {
            launching.setActiveProfile(profile.id);

            const Dialect::Port speaks = Dialect::of(launching);

            std::ignore = Dialect::demos(speaks);
            std::ignore = Dialect::saves(speaks);
            std::ignore = Dialect::net(speaks);

            for (int key = 0; key < 4; ++key) {
                std::ignore = Arguments::of(launching);
                std::ignore = Command::line(launching);
                std::ignore = Command::pattern(launching);
                std::ignore = Command::trouble(launching);
                std::ignore = Command::dosSpend(launching);

                ++items;
            }

            std::ignore = Storage::saves(launching);
            std::ignore = Storage::replays(launching);
            std::ignore = Storage::configFile(launching);
        }

        launching.setActiveProfile(launching.profiles.front().id);
        launching.activeProfile().customCommand = true;
        launching.activeProfile().command = "{source_port} -iwad {game} -file {addon_1} -skill 4";

        ok(!Command::custom(launching).empty(), "core: a custom command resolves");

        std::ignore = Command::line(launching);
    }

    // Game files: every add-on's maps, cold and then cached, and the titles the cards show.
    for (int pass = 0; pass < 2; ++pass) {
        for (const std::string &file : world.addons()) {
            items += MapFile::maps(file).names.size();
        }
    }

    for (const NameEntry &game : world.games()) {
        const Artwork::Title title = Artwork::titleOf(game.file);
        int wide = 0;
        int tall = 0;

        Artwork::measure(title, wide, tall);

        ok(!Artwork::decode(title).empty(), "core: a title screen decodes");

        std::ignore = Artwork::paletteOf(game.file);
        std::ignore = FileInfo::describeIwad(game.file);

        if (const auto opened = MapFile::open(game.file); opened != nullptr) {
            std::ignore = opened->mapNames();
            std::ignore = opened->lumpNames();
            std::ignore = opened->isGame();
            std::ignore = opened->lump("PLAYPAL");
            std::ignore = opened->iwadinfoName();
        }

        ++items;
    }

    for (const NameEntry &port : world.ports()) {
        std::ignore = FileInfo::describePort(port.file);
        std::ignore = Detect::of(port.file);

        ++items;
    }

    if (!world.games().empty()) {
        ok(!md5File(world.games().front().file).empty(), "core: a file hashes");
        ok(!sha256File(world.games().front().file).empty(), "core: a file digests");

        std::ignore = md5Text(decorated());
    }

    // A fetched port: hashed, unpacked and placed, and the catalogue it came from.
    {
        std::string error;

        ok(Archive::extract(tests::Corpus::pk3(), scratch / "unpacked", &error), "core: a zip extracts");

        for (const Catalog::Port &port : Catalog::ports()) {
            std::ignore = Catalog::pattern(port);
            std::ignore = Catalog::directory(port);
            std::ignore = Catalog::find(port.id);
            std::ignore = Install::stampedVersion(port);

            ++items;
        }

        if (const Catalog::Port *gzdoom = Catalog::find("gzdoom"); gzdoom != nullptr) {
            const std::string body =
                    R"({"tag_name":"g4.14.2","assets":[)"
                    R"({"name":"gzdoom-4-14-2-linux-x86_64.tar.xz","browser_download_url":"https://example.invalid/g.tar.xz","size":1},)"
                    R"({"name":"gzdoom-4-14-2-linux-portable.zip","browser_download_url":"https://example.invalid/g.zip","size":4096}]})";
            const Install::Release release = Install::parseRelease(body, *gzdoom);

            std::ignore = Install::downloadName(*gzdoom, release.asset);
            std::ignore = Install::secure(release.url);
            std::ignore = Install::fileNameOf(release.url);
        }

        const Releases::Answer answer{
                .portId = "gzdoom",
                .version = "4.14.2",
                .url = "https://example.invalid/g.zip",
                .asset = "gzdoom-4-14-2-linux-portable.zip",
                .verdict = 0,
                .note = "",
                .checked = Releases::now(),
                .size = 4096,
                .sha256 = "",
        };

        Releases::write({&answer, 1});

        ok(!Releases::read().empty() && Releases::fresh(answer.checked), "core: the release cache round-trips");
    }

    std::ignore = Detect::ports();
    std::ignore = Detect::dosbox();

    // The game's output as the dock filters it.
    {
        const std::string input = decorated();
        std::string out;

        for (int rep = 0; rep < 20; ++rep) {
            Ansi filter;

            out.clear();
            filter.filter(input, out);

            ++items;
        }

        ok(!out.empty() && !out.contains('\033'), "core: the escapes are filtered out");
    }

    tally.frames = items;
}

}
