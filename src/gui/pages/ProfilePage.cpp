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
#include <string_view>
#include <utility>

#include "core/launch/Dos.h"
#include "gui/services/Filters.h"
#include "gui/components/Parts.h"
#include "gui/components/Tones.h"
#include "gui/draw/Glyphs.h"
#include "gui/pages/ProfilePage.h"
#include "gui/state/State.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/Chip.h"
#include "gui/toolkit/controls/Fact.h"
#include "gui/toolkit/controls/Field.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/controls/Pill.h"
#include "gui/toolkit/controls/MultistateSwitch.h"
#include "gui/toolkit/controls/Select.h"
#include "gui/toolkit/controls/StatusIndicator.h"
#include "gui/toolkit/controls/Stepper.h"
#include "gui/toolkit/controls/Toggle.h"
#include "gui/toolkit/layout/Box.h"
#include "gui/toolkit/layout/Collapsible.h"
#include "gui/toolkit/layout/Pair.h"
#include "gui/toolkit/layout/Panel.h"
#include "gui/toolkit/layout/Rule.h"
#include "gui/toolkit/layout/Scroll.h"
#include "gui/toolkit/layout/Spacer.h"
#include "gui/toolkit/layout/Wrap.h"
#include "gui/toolkit/overlays/Menu.h"
#include "gui/util/Clipboard.h"
#include "gui/util/Desktop.h"
#include "gui/util/Format.h"

namespace {

enum class ProfileMenuAction : std::uint8_t {
    Rename,
    Duplicate,
    Clear,
    CopyConfig,
    LoadZdl,
    SaveZdl,
    Delete,
};

constexpr double RUN_WIDTH = 340.0;

constexpr std::array<std::string_view, 5> SKILLS = {"V. Easy", "Easy", "Medium", "Hard",
                                                   "V. Hard"};
constexpr std::array<std::string_view, 4> MONSTERS = {"No monsters", "Fast", "Respawn",
                                                      "Fast & respawn"};

// -netmode takes a number, and -1 leaves it to the port.
constexpr int NETMODE_PORT = -1;

// The tokens a custom command can be written with.
constexpr auto TOKENS = std::to_array<std::pair<const char *, const char *>>({
    {"{source_port}", "The source port this profile is set to."},
    {"{game}", "The game this profile is set to."},
    {"{addon_n}", "An add-on from the list, counting from one."},
    {"{profile}", "The profile's own folder, which the next few sit in."},
    {"{cfgdir}", "The port config written for this profile."},
    {"{extracfg}", "The second config a vanilla port keeps."},
    {"{savedir}", "The profile's saves folder, inside {profile}."},
    {"{savefile}", "The save this profile is set to load."},
    {"{replaydir}", "The profile's replays folder, inside {profile}."},
});

template <typename List>
int indexOf(const List &list, const std::string_view wanted) {
    const auto found = std::ranges::find(list, wanted);

    return found == std::ranges::end(list)
        ? -1
        : static_cast<int>(std::ranges::distance(std::ranges::begin(list), found));
}

// A panel no taller than what is in it, up to a ceiling: the command line is
// usually one line and a box four deep leaves a hole under it.
class Hug : public toolkit::Panel {
public:
    explicit Hug(const double most) : _most(most) {}

    double naturalHeight(Typeface &type, const double width) override {
        return std::min(_most, Panel::naturalHeight(type, width));
    }

private:
    double _most;
};

}

namespace pages {

using namespace toolkit;

// --- the page ------------------------------------------------------------------

bool ProfilePage::ready() {
    const State::Cfg &cfg = State::get().cfg;

    return cfg.commandOverride ? cfg.commandTrouble.empty() : !cfg.port.empty();
}

std::string ProfilePage::summary() {
    const State::Cfg &cfg = State::get().cfg;

    if (cfg.commandOverride) {
        return cfg.commandTrouble.empty() ? "Launches the command written on this page"
                                          : cfg.commandTrouble;
    }

    if (!ready()) {
        return "No source port · this profile cannot be launched yet";
    }

    std::string said = cfg.port + " · " + (cfg.iwad.empty() ? "no game" : cfg.iwad);

    if (!cfg.warp.empty()) {
        said += " · " + cfg.warp;
    }

    if (!cfg.files.empty()) {
        said += " · " + std::to_string(cfg.files.size())
            + (cfg.files.size() == 1 ? " file" : " files");
    }

    return said;
}

std::string ProfilePage::netSummary() {
    const State::Cfg &cfg = State::get().cfg;

    if (cfg.netRole == NetRole::Alone) {
        return "Off · this profile launches a single player game";
    }

    const bool unsupported = (cfg.netRole == NetRole::Host && !cfg.netHosts)
        || (cfg.netRole == NetRole::Join && !cfg.netJoins);

    if (unsupported) {
        return "not a side this port takes";
    }

    if (cfg.netRole == NetRole::Host) {
        return (cfg.netPlayers ? "for " + std::to_string(cfg.players) + " players"
                               : "open to others")
            + (cfg.netPort.empty() ? "" : ", on port " + cfg.netPort);
    }

    if (cfg.host.empty()) {
        return "no address yet";
    }

    return cfg.host + (cfg.netPort.empty() ? "" : ":" + cfg.netPort);
}

std::string ProfilePage::saveSummary() {
    const State::Cfg &cfg = State::get().cfg;

    if (!cfg.saveEnabled) {
        return "Off · every launch starts a new game";
    }

    if (cfg.saveFile.empty()) {
        return "nothing picked yet";
    }

    return "from " + Format::fitPath(cfg.savePath, 30);
}

std::string ProfilePage::replaySummary() {
    const State::Cfg &cfg = State::get().cfg;

    if (cfg.replayMode == ReplayMode::Off) {
        return "Off · nothing is recorded and nothing is played back";
    }

    if (cfg.replayFile.empty()) {
        return cfg.replayMode == ReplayMode::Record ? "no name yet" : "nothing picked yet";
    }

    return (cfg.replayMode == ReplayMode::Record ? "into " : "")
        + Format::fitPath(cfg.replayPath, 30);
}

std::string ProfilePage::tuningSummary() {
    const State::Cfg &cfg = State::get().cfg;

    const int mode = cfg.hasNetmode ? cfg.netmode : NETMODE_PORT;
    const int dup = cfg.netDup ? cfg.dup : 0;
    const bool extra = cfg.netExtratic && cfg.extratic;

    if (mode == NETMODE_PORT && dup <= 0 && !extra) {
        return "left to the port";
    }

    std::string said = mode == -1 ? "" : (mode == 0 ? "peer to peer" : "client/server");

    if (dup > 0) {
        said += (said.empty() ? "" : " · ") + std::to_string(dup) + "× tics";
    }

    if (extra) {
        said += (said.empty() ? "" : " · ") + std::string("extra tic");
    }

    return said;
}

ProfilePage::ProfilePage(Reach *reach) : _reach(reach) {
    Box *column = append(Box::column());

    column->spacing(16.0);

    // --- the head ---

    Box *head = column->append(Box::row());

    head->fixedHeight = 74.0;
    head->spacing(14.0)->pad(Theme::bleed, 0.0, Theme::bleed, 0.0)->cross(Box::Place::Centre);

    _chooser = head->append(std::make_unique<components::ProfileChooser>(reach));
    _chooser->stretch = 1.0;
    _chooser->fixedHeight = 74.0;
    _chooser->artwork = [this](const std::string &key) { return _reach->art.of(key); };

    _terminal = head->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Terminal, [this] {
        _reach->runs.show(State::get().cfg.profileKey);
    }));

    _terminal->size(Theme::control)->outlined();
    _terminal->fixedWidth = Theme::control;
    _terminal->fixedHeight = Theme::control;

    _cog = head->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Cog, [this] { showMenu(); }));
    _cog->size(Theme::control)->outlined()->spin(90.0);
    _cog->fixedWidth = Theme::control;
    _cog->fixedHeight = Theme::control;

    _launch = head->append(std::make_unique<Button>("Launch", [this] {
        _reach->config.profile().launch();
    }));

    _launch->kind(Button::Kind::Primary)->glyph(Glyphs::Glyph::Play);

    // --- the body ---

    _scroll = column->append(std::make_unique<Scroll>());
    _scroll->stretch = 1.0;

    _body = static_cast<Box *>(_scroll->hold(Box::column()));
    _body->spacing(16.0)->pad(Theme::bleed, 0.0, Theme::bleed, 8.0);

    // Add-ons floor, gap, and the run panel: under this the page is cut, not
    // squeezed.
    _body->minWidth = 260.0 + 16.0 + RUN_WIDTH + (Theme::bleed * 2.0);

    Box *top = _body->append(Box::row());

    top->spacing(16.0);

    // A floor, not a ceiling: the row grows with whatever The run holds.
    top->minHeight = 340.0;

    // Add-ons.
    Panel *addons = top->append(std::make_unique<Panel>());

    addons->stretch = 1.0;
    addons->minWidth = 260.0;

    Box *inside = addons->append(Box::column());

    inside->pad(16.0)->spacing(12.0);

    Box *addonHead = inside->append(Box::row());

    addonHead->spacing(8.0)->cross(Box::Place::Centre);
    addonHead->fixedHeight = Theme::controlSmall;

    components::panelTitle(addonHead, "Add-ons");

    addonHead->append(std::make_unique<Spacer>());

    _loaded = addonHead->append(std::make_unique<Pill>());
    _loaded->kind(Pill::Kind::Muted)->dot(false);

    _addFiles = addonHead->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Plus, [this] {
        _reach->picker.open(FilePicker::Action::AddFiles, "Add files", Filters::wad(), false, true, true,
                            FilePicker::Slot::Wad);
    }));

    _addFiles->tooltip("Add files");
    _addFiles->fixedWidth = Theme::controlSmall;

    _clearFiles = addonHead->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Trash, [this] {
        _reach->ask("Clear the file list?",
                  "Every file in this profile's list is removed. The files themselves are left "
                  "alone, and the rest of the profile is untouched.",
                  "Clear", true, [this] { _reach->config.lists().clearFiles(); });
    }));

    _clearFiles->tone(Theme::of().muted, Theme::of().danger)
        ->tooltip("Remove every file from this profile");
    _clearFiles->fixedWidth = Theme::controlSmall;

    Panel *trough = inside->append(std::make_unique<Panel>());

    trough->inset = true;
    trough->stretch = 1.0;

    Box *lane = trough->append(Box::column());

    lane->pad(6.0);

    _files = lane->append(std::make_unique<components::AddonList>(reach));
    _files->stretch = 1.0;

    // The run.
    Panel *runPanel = top->append(std::make_unique<Panel>());

    runPanel->fixedWidth = RUN_WIDTH;

    Box *run = runPanel->append(Box::column());

    run->pad(16.0)->spacing(14.0);

    buildRun(run);

    buildReplay(_body);
    buildSaves(_body);
    buildNet(_body);
    buildCommand(_body);

    // --- no profiles ---

    _none = append(Box::column());
    _none->spacing(18.0)->align(Box::Place::Centre)->cross(Box::Place::Centre);
    _none->setVisible(false);

    Label *title = _none->append(std::make_unique<Label>("No profiles"));

    title->font(600, Theme::fontMedium)->tone(Theme::of().muted);
    title->fixedWidth = 420.0;

    Label *said = _none->append(std::make_unique<Label>(
        "A profile holds a game, the port it runs on and whatever is loaded over them, under a "
        "name. This config has none."));

    said->font(400, Theme::fontSmall)->tone(Theme::of().faint)->wrap();
    said->fixedWidth = 420.0;

    Box *buttons = _none->append(Box::row());

    buttons->spacing(8.0);
    buttons->fixedWidth = 420.0;
    buttons->fixedHeight = Theme::control;

    buttons->append(std::make_unique<Button>("New profile", [this] {
        _reach->prompt("New profile", "Name", "New profile", "Create",
                     [this](const std::string &named) {
                         _reach->config.profile().addProfile(named);
                     });
    }))->kind(Button::Kind::Primary)->glyph(Glyphs::Glyph::Plus);

    buttons->append(std::make_unique<Button>("Import a .zdl", [this] {
        _reach->picker.open(FilePicker::Action::LoadZdl, "Load a .zdl launch config", Filters::zdl(), false,
                            false, false, FilePicker::Slot::Zdl);
    }))->kind(Button::Kind::Ghost)->glyph(Glyphs::Glyph::Download)
        ->tooltip("Read a .zdl launch config in as a profile of its own");

    buttons->append(std::make_unique<Spacer>());
}

void ProfilePage::buildRun(Box *into) {
    components::panelTitle(into, "The run");

    _addPort = into->append(std::make_unique<Button>("Add a source port", [this] {
        _reach->go(State::Page::Engines);
    }));

    _addPort->glyph(Glyphs::Glyph::Plus)->tooltip("Ports are set up on the Engines page");

    _port = into->append(std::make_unique<Select>("Source port", [this](const int index) {
        const std::vector<std::string> &names = State::get().cfg.portNames;

        _reach->config.profile().setPort(
            index < 0 || std::cmp_greater_equal(index, names.size())
                ? std::string()
                : names[static_cast<size_t>(index)]);
    }));

    _port->placeholder("None selected")->clearable()
        ->tooltip("What actually runs. Add ports on the Engines page.");

    _addGame = into->append(std::make_unique<Button>("Add a game", [this] {
        State::get().nav.shelf = State::Shelf::Games;

        _reach->go(State::Page::Library);
    }));

    _addGame->glyph(Glyphs::Glyph::Plus)->tooltip("Games are added on the library's games shelf");

    _iwad = into->append(std::make_unique<Select>("Game", [this](const int index) {
        const std::vector<std::string> &names = State::get().cfg.iwadNames;

        _reach->config.profile().setIwad(
            index < 0 || std::cmp_greater_equal(index, names.size())
                ? std::string()
                : names[static_cast<size_t>(index)]);
    }));

    _iwad->placeholder("None selected")->clearable()
        ->tooltip("The IWAD itself. Add games from the library.");

    _map = into->append(std::make_unique<Select>("Map", [this](const int index) {
        const std::vector<std::string> &maps = State::get().cfg.maps;

        _reach->config.profile().setWarp(
            index < 0 || std::cmp_greater_equal(index, maps.size())
                ? std::string()
                : maps[static_cast<size_t>(index)]);
    }));

    _map->clearable()->tooltip("Read out of the game and everything loaded on top of it");

    // Half the row each, whatever the panel has come down to: a width taken off
    // the page's own runs the second one off the edge as soon as it is narrower.
    Box *pair = into->append(std::make_unique<Pair>(0.0));

    pair->spacing(12.0);

    _skill = pair->append(std::make_unique<Select>("Skill", [this](const int index) {
        _reach->config.profile().setSkill(index + 1);
    }));

    _skill->clearable();
    _skill->setOptions(std::vector<std::string>(SKILLS.begin(), SKILLS.end()));

    _monsters = pair->append(std::make_unique<Select>("Monsters", [this](const int index) {
        _reach->config.profile().setMonsters(index + 1);
    }));

    _monsters->clearable();
    _monsters->setOptions(std::vector<std::string>(MONSTERS.begin(), MONSTERS.end()));

    into->append(std::make_unique<Rule>());

    _capture = into->append(std::make_unique<Toggle>("Log the game's output",
                                                     [this](const bool on) {
        _reach->config.profile().setCaptureOutput(on);
    }));

    _fullscreen = into->append(std::make_unique<Toggle>("Full screen", [this](const bool on) {
        _reach->config.profile().setDosFullscreen(on);
    }));

    _fullscreen->hint = "Give DOSBox the whole screen rather than a window";

    _levelstat = into->append(std::make_unique<Toggle>("Level stats", [this](const bool on) {
        _reach->config.profile().setLevelstat(on);
    }));

    _levelstat->hint = "Writes a levelstat.txt with the time taken on each map, which is what a "
                       "run is submitted with";

    _sharedConfig = into->append(std::make_unique<Toggle>("Use the port's config",
                                                          [this](const bool on) {
        _reach->config.profile().setSharedConfig(on);
    }));

    _sharedConfig->hint = "Launch on the settings the source port keeps for itself, shared with "
                          "everything else that uses them";

    _directory = into->append(std::make_unique<Fact>("Directory", ""));
    _directory->path()->onClick("Show in file explorer", [] {
        const std::string where = State::get().cfg.profileDirectory;
        std::error_code made;

        std::filesystem::create_directories(std::filesystem::path(where), made);

        Desktop::open(where);
    });
}

void ProfilePage::buildReplay(Box *into) {
    _replay = into->append(std::make_unique<CollapsiblePanel>("Replay", [this](const bool open) {
        _reach->config.panels().setReplayOpen(open);
    }));

    _replayMode = _replay->tools()->append(std::make_unique<MultistateSwitch>(
        [this](const int value) {
            const auto mode = static_cast<ReplayMode>(value);

            _reach->config.panels().setReplayMode(mode);
            _reach->config.panels().setReplayOpen(mode != ReplayMode::Off);
        }));

    _replayMode->setOptions({{.value = static_cast<int>(ReplayMode::Off), .label = "Off"},
                             {.value = static_cast<int>(ReplayMode::Record), .label = "Record"},
                             {.value = static_cast<int>(ReplayMode::Play), .label = "Play"}});

    // Last, so it sits against the far edge of the heading.
    _replayReset = _replay->tools()->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Refresh, [this] {
        _reach->ask("Reset the replay settings?",
                  "The profile goes back to recording nothing and playing nothing back. The "
                  "demos already in its replays folder are left where they are.",
                  "Reset", true, [this] { _reach->config.panels().clearReplay(); });
    }));

    _replayReset->size(Theme::control)->outlined();
    _replayReset->fixedWidth = Theme::control;

    Box *body = _replay->body();

    body->append(std::make_unique<Rule>());

    _replayNote = body->append(std::make_unique<Label>());
    _replayNote->font(400, Theme::fontSmall)->tone(Theme::of().faint)->wrap();

    // Recording.
    _replayRecord = body->append(Box::column());
    _replayRecord->spacing(10.0);

    _replayName = _replayRecord->append(std::make_unique<Field>("Record it as",
                                                                [this](const std::string &value) {
        _reach->config.panels().setReplayFile(value);
    }));

    _replayName->placeholder("A name for the demo")->mono()
        ->note(".lmp goes on the end by itself, and the file lands in the profile's replays "
               "folder");

    // Playing back.
    _replayPlay = body->append(Box::column());
    _replayPlay->spacing(10.0);

    Box *play = _replayPlay->append(std::make_unique<Pair>(900.0));

    play->spacing(20.0)->cross(Box::Place::End);

    Box *pick = play->append(Box::row());

    pick->spacing(8.0)->cross(Box::Place::End);

    _replayFile = pick->append(std::make_unique<Select>("Replay", [this](const int index) {
        _reach->config.panels().setReplayIndex(index);
    }));

    _replayFile->clearable()->tooltip("The demos in this profile's replays folder, newest first");
    _replayFile->fixedWidth = 340.0;

    _replayRefresh = pick->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Refresh, [this] {
        _reach->config.panels().refreshReplays();
    }));

    _replayRefresh->size(Theme::control)->outlined()->tooltip("Refresh folder");
    _replayRefresh->fixedWidth = Theme::control;

    _replayBrowse = pick->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Folder, [this] {
        _reach->picker.open(FilePicker::Action::Replay, "Select a replay", Filters::replay(), false, false,
                            false, FilePicker::Slot::Replay);
    }));

    _replayBrowse->size(Theme::control)->outlined()->tooltip("Play a demo from somewhere else");
    _replayBrowse->fixedWidth = Theme::control;

    pick->append(std::make_unique<Spacer>());

    Box *speed = play->append(Box::column());

    // Start, or the trough is drawn over the whole width rather than its tabs.
    speed->spacing(6.0)->cross(Box::Place::Start);

    speed->append(std::make_unique<Label>("How it plays"))->section();

    _replaySpeed = speed->append(std::make_unique<MultistateSwitch>([this](const int value) {
        _reach->config.panels().setReplayPlayback(static_cast<Playback>(value));
    }));

    _replayPath = body->append(std::make_unique<Label>());
    _replayPath->font(400, Theme::fontTiny)->tone(Theme::of().faint)->path();
    _replayPath->onClick([] { Desktop::open(State::get().cfg.replayFolder); });
    _replayPath->hint = "Show in file explorer";

    // Compatibility, while recording.
    _replayTune = body->append(Box::column());
    _replayTune->spacing(12.0);

    _replayTune->append(std::make_unique<Rule>());
    _replayTune->append(std::make_unique<Label>("Compatibility"))->section();

    Box *tune = _replayTune->append(Box::row());

    tune->spacing(20.0)->cross(Box::Place::Centre);

    _complevel = tune->append(std::make_unique<Select>("", [this](const int index) {
        _reach->config.panels().setReplayComplevel(index);
    }));

    _complevel->tooltip("Complevel");
    _complevel->fixedWidth = 220.0;

    _longtics = tune->append(std::make_unique<Toggle>("Long tics", [this](const bool on) {
        _reach->config.panels().setReplayLongtics(on);
    }));

    _longtics->hint = "Records turns at the port's own precision rather than vanilla's. A demo "
                      "made this way needs a port that reads them";

    _soloNet = tune->append(std::make_unique<Toggle>("Solo net", [this](const bool on) {
        _reach->config.panels().setReplaySoloNet(on);
    }));

    _soloNet->hint = "Plays alone under a netgame's rules, which is what a recorded run is "
                     "judged under";

    tune->append(std::make_unique<Spacer>());
}

void ProfilePage::buildSaves(Box *into) {
    _saves = into->append(std::make_unique<CollapsiblePanel>("Saves", [this](const bool open) {
        _reach->config.panels().setSaveOpen(open);
    }));

    _saveOn = _saves->tools()->append(std::make_unique<MultistateSwitch>(
        [this](const int value) {
            _reach->config.panels().setSaveEnabled(value != 0);
            _reach->config.panels().setSaveOpen(value != 0);
        }));

    _saveOn->setOptions({{.value = 0, .label = "Off"}, {.value = 1, .label = "On"}});

    Box *body = _saves->body();

    body->append(std::make_unique<Rule>());

    Box *pick = body->append(Box::row());

    pick->spacing(8.0)->cross(Box::Place::End);

    _saveFile = pick->append(std::make_unique<Select>("Save", [this](const int index) {
        _reach->config.panels().setSaveIndex(index);
    }));

    _saveFile->clearable();
    _saveFile->fixedWidth = 340.0;

    _saveRefresh = pick->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Refresh, [this] {
        _reach->config.panels().refreshSaves();
    }));

    _saveRefresh->size(Theme::control)->outlined()->tooltip("Refresh folder");
    _saveRefresh->fixedWidth = Theme::control;

    pick->append(std::make_unique<Spacer>());

    _saveNote = body->append(std::make_unique<Label>());
    _saveNote->font(400, Theme::fontSmall)->tone(Theme::of().faint)->wrap();

    _savePath = body->append(std::make_unique<Label>());
    _savePath->font(400, Theme::fontTiny)->tone(Theme::of().faint)->path();
    _savePath->onClick([] { Desktop::open(State::get().cfg.saveFolder); });
    _savePath->hint = "Show in file explorer";
}

void ProfilePage::buildNet(Box *into) {
    _net = into->append(std::make_unique<CollapsiblePanel>("Multiplayer", [this](const bool open) {
        _reach->config.panels().setMultiplayerOpen(open);
    }));

    _role = _net->tools()->append(std::make_unique<MultistateSwitch>([this](const int value) {
        const auto role = static_cast<NetRole>(value);

        _reach->config.panels().setNetRole(role);
        _reach->config.panels().setMultiplayerOpen(role != NetRole::Alone);
    }));

    _role->setOptions({{.value = static_cast<int>(NetRole::Alone), .label = "Off"},
                       {.value = static_cast<int>(NetRole::Host), .label = "Host"},
                       {.value = static_cast<int>(NetRole::Join), .label = "Join"}});

    // Last, so it sits against the far edge of the heading.
    _netReset = _net->tools()->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Refresh, [this] {
        _reach->ask("Reset the multiplayer settings?",
                  "The side this profile is on, the game it opens and every address, limit and "
                  "flag under it go back to their defaults. The rest of the profile is "
                  "untouched.",
                  "Reset", true, [this] { _reach->config.panels().clearMultiplayer(); });
    }));

    _netReset->size(Theme::control)->outlined();
    _netReset->fixedWidth = Theme::control;

    Box *body = _net->body();

    body->append(std::make_unique<Rule>());

    _netNote = body->append(std::make_unique<Label>());
    _netNote->font(400, Theme::fontSmall)->tone(Theme::of().faint)->wrap();

    // Hosting.
    _hosting = body->append(Box::row());
    _hosting->spacing(20.0)->cross(Box::Place::End);

    Box *type = _hosting->append(Box::column());

    type->spacing(6.0);
    type->append(std::make_unique<Label>("Game type"))->section();

    _gameType = type->append(std::make_unique<MultistateSwitch>([this](const int value) {
        _reach->config.panels().setGameType(static_cast<GameType>(value));
    }));

    _gameType->setOptions(
        {{.value = static_cast<int>(GameType::Coop), .label = "Co-op"},
         {.value = static_cast<int>(GameType::Deathmatch), .label = "Deathmatch"},
         {.value = static_cast<int>(GameType::AltDeathmatch), .label = "Alt deathmatch"}});

    _players = _hosting->append(std::make_unique<Stepper>("Players", [this](const int value) {
        _reach->config.panels().setPlayers(value);
    }));

    _players->range(1, 8)->tooltip("How many the game is opened for, this machine included");
    _players->fixedWidth = 170.0;

    _netPort = _hosting->append(std::make_unique<Field>("Listen on port",
                                                        [this](const std::string &value) {
        _reach->config.panels().setNetPort(value);
    }));

    _netPort->placeholder("Default")->mono();
    _netPort->fixedWidth = 160.0;

    Box *listing = _hosting->append(Box::column());

    listing->spacing(6.0);
    listing->append(std::make_unique<Label>("Listing"))->section();

    _listed = listing->append(std::make_unique<Toggle>("Public", [this](const bool on) {
        _reach->config.panels().setListed(on);
    }));

    _listed->hint = "Puts the game on the master server's list, where anyone can find it. Off "
                    "keeps it to whoever has the address";
    _listed->fixedHeight = Theme::control;

    _hosting->append(std::make_unique<Spacer>());

    // Joining. Start, so the note under the port leaves the boxes level.
    _joining = body->append(Box::row());
    _joining->spacing(12.0)->cross(Box::Place::Start);

    _host = _joining->append(std::make_unique<Field>("Address of the game",
                                                     [this](const std::string &value) {
        _reach->config.panels().setHost(value);
    }));

    _host->placeholder("A host name or an address")->mono();
    _host->stretch = 1.0;

    _joinPort = _joining->append(std::make_unique<Field>("Port",
                                                         [this](const std::string &value) {
        _reach->config.panels().setNetPort(value);
    }));

    _joinPort->placeholder("Default")->mono();
    _joinPort->fixedWidth = 160.0;

    // The rules of a hosted game.
    _rules = body->append(Box::column());
    _rules->spacing(12.0);

    _rules->append(std::make_unique<Rule>());
    _rules->append(std::make_unique<Label>("Rules of the game"))->section();

    Box *limits = _rules->append(Box::row());

    limits->spacing(12.0)->cross(Box::Place::Start);

    _fragLimit = limits->append(std::make_unique<Field>("Frag limit",
                                                        [this](const std::string &value) {
        _reach->config.panels().setFragLimit(value);
    }));

    _fragLimit->placeholder("None")->mono();
    _fragLimit->stretch = 1.0;

    _timeLimit = limits->append(std::make_unique<Field>("Time limit",
                                                        [this](const std::string &value) {
        _reach->config.panels().setTimeLimit(value);
    }));

    _timeLimit->placeholder("None")->mono()->note("In minutes");
    _timeLimit->stretch = 1.0;

    _dmflags = limits->append(std::make_unique<Field>("dmflags",
                                                      [this](const std::string &value) {
        _reach->config.panels().setDmflags(value);
    }));

    _dmflags->placeholder("None")->mono()->note("The port's own flag word");
    _dmflags->stretch = 1.0;

    _dmflags2 = limits->append(std::make_unique<Field>("dmflags2",
                                                       [this](const std::string &value) {
        _reach->config.panels().setDmflags2(value);
    }));

    _dmflags2->placeholder("None")->mono()->note("The second, where a port has one");
    _dmflags2->stretch = 1.0;

    _savegame = _rules->append(std::make_unique<Field>("Start from a save",
                                                       [this](const std::string &value) {
        _reach->config.panels().setSavegame(value);
    }));

    _savegame->placeholder("None · the game starts at its first map")->mono()
        ->note("Everyone joining drops into the host's saved game");

    _savegame->icon(Glyphs::Glyph::Folder, "Browse", [this] {
        _reach->picker.open(FilePicker::Action::Savegame, "Select a save game", Filters::save(), false, false,
                            false, FilePicker::Slot::Save);
    });

    _saveClash = _rules->append(std::make_unique<Label>(
        "The Saves panel names one too, and a port loads one save: that is the one it gets."));

    _saveClash->font(400, Theme::fontSmall)->tone(Theme::of().warning)->wrap();

    // The connection, which folds on its own.
    _tuning = body->append(Box::column());
    _tuning->spacing(12.0);

    _tuning->append(std::make_unique<Rule>());

    _tuningHead = _tuning->append(std::make_unique<DisclosureHeading>("Connection", [this] {
        State::get().nav.tuning = !State::get().nav.tuning;

        _reach->touch();
    }));

    Box *knobs = _tuningHead->body()->append(Box::row());

    knobs->spacing(20.0)->cross(Box::Place::End);

    Box *mode = knobs->append(Box::column());

    mode->spacing(6.0);
    mode->append(std::make_unique<Label>("Net mode"))->section();

    _netmode = mode->append(std::make_unique<MultistateSwitch>([this](const int value) {
        _reach->config.panels().setNetmode(value);
    }));

    _netmode->setOptions({{.value = NETMODE_PORT, .label = "The port's own"},
                          {.value = 0, .label = "Peer to peer"},
                          {.value = 1, .label = "Client/server"}});

    _dup = knobs->append(std::make_unique<Stepper>("Duplicate tics", [this](const int value) {
        _reach->config.panels().setDup(value);
    }));

    _dup->range(1, 9)->clearable(0, "Off")
        ->tooltip("Sends each tic more than once, which trades bandwidth for a connection that "
              "drops packets");
    _dup->fixedWidth = 170.0;

    Box *extra = knobs->append(Box::column());

    extra->spacing(6.0);
    extra->append(std::make_unique<Label>("Extra tic"))->section();

    _extratic = extra->append(std::make_unique<MultistateSwitch>([this](const int value) {
        _reach->config.panels().setExtratic(value != 0);
    }));

    _extratic->setOptions({{.value = 0, .label = "Off"}, {.value = 1, .label = "On"}});

    knobs->append(std::make_unique<Spacer>());
}

void ProfilePage::buildCommand(Box *into) {
    Panel *panel = into->append(std::make_unique<Panel>());
    Box *line = panel->append(Box::column());

    line->pad(16.0)->spacing(12.0);

    Box *head = line->append(Box::row());

    head->spacing(12.0)->cross(Box::Place::Centre);
    head->fixedHeight = Theme::control;

    components::panelTitle(head, "Command line");

    _budget = head->append(std::make_unique<Chip>("", "DOSBox runs only the first eleven "
        "commands it is given with -c and silently drops the rest."));

    _budget->plain();

    head->append(std::make_unique<Spacer>());

    _override = head->append(std::make_unique<Toggle>("Override", [this](const bool on) {
        _reach->config.profile().setCommandOverride(on);
    }));

    _override->hint = "Use a custom command instead of the generated one";

    _extra = line->append(std::make_unique<Field>("Extra arguments",
                                                  [this](const std::string &value) {
        _reach->config.profile().setExtra(value);
    }));

    _extra->placeholder("Passed to the source port as typed")->mono();

    _command = line->append(std::make_unique<Field>("The command",
                                                    [this](const std::string &value) {
        _reach->config.profile().setCommand(value);
    }));

    _command->placeholder("{source_port} -iwad {game} -file {addon_1}")->mono();

    _tokens = line->append(std::make_unique<Wrap>());
    _tokens->spacing(2.0, 2.0);

    for (const auto &[word, about] : TOKENS) {
        _tokens->append(std::make_unique<Chip>(word, about));
    }

    Panel *resolved = line->append(std::make_unique<Hug>(108.0));

    resolved->inset = true;
    resolved->hoverable = true;

    Box *inside = resolved->append(Box::row());

    inside->pad(12.0)->spacing(10.0)->cross(Box::Place::Start);

    _resolved = inside->append(std::make_unique<Label>());
    _resolved->stretch = 1.0;
    _resolved->font(Typeface::mono, Theme::fontSmall)->tone(Theme::of().muted)->wrap();
    _resolved->hint = "See the whole of it";
    _resolved->onClick([this] {
        _reach->showCommand();

        _reach->touch();
    });

    _copy = inside->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Extract, [this] {
        Clipboard::write(State::get().cfg.commandLine);
        _reach->notify.success("The command line is on the clipboard.");
    }));

    _copy->size(26.0)->tooltip("Copy it");
    _copy->fixedWidth = 26.0;
    _copy->fixedHeight = 26.0;
}

// --- keeping up with the state -------------------------------------------------

void ProfilePage::syncRun() const {
    const State::Cfg &cfg = State::get().cfg;

    _addPort->setVisible(cfg.ports.empty());
    _port->setVisible(!cfg.ports.empty());

    if (_port->visible()) {
        _port->setOptions(cfg.portNames);
        _port->setBadges(cfg.portBadges);
        _port->setCurrent(indexOf(cfg.portNames, cfg.port));
    }

    _addGame->setVisible(cfg.iwads.empty());
    _iwad->setVisible(!cfg.iwads.empty());

    if (_iwad->visible()) {
        _iwad->setOptions(cfg.iwadNames);
        _iwad->setCurrent(indexOf(cfg.iwadNames, cfg.iwad));
    }

    _map->setOptions(cfg.maps);
    _map->setCurrent(indexOf(cfg.maps, cfg.warp));

    _skill->setCurrent(cfg.skill - 1);
    _monsters->setCurrent(cfg.monsters - 1);

    // A DOS port prints into DOSBox's own window.
    _capture->setVisible(!cfg.dosPort);
    _capture->setChecked(cfg.captureOutput && !cfg.autoClose);
    _capture->setEnabled(!cfg.autoClose);
    _capture->hint = cfg.autoClose
        ? "Nothing to record while ZDL4 closes on launch: the log goes with the window. Turn "
          "that off in Settings."
        : "Takes what the source port prints into a log along the bottom of the window. Off, "
          "nothing is piped at all.";

    _fullscreen->setVisible(cfg.dosPort);
    _fullscreen->setChecked(cfg.dosFullscreen);

    _levelstat->setVisible(cfg.hasLevelstat);
    _levelstat->setChecked(cfg.levelstat);

    _sharedConfig->setVisible(cfg.profileConfigs && !cfg.dosPort);
    _sharedConfig->setChecked(cfg.sharedConfig);

    _directory->setValue(cfg.profileDirectory);

    _loaded->setVisible(!cfg.files.empty());
    _loaded->setText(std::cmp_equal(cfg.enabledCount ,cfg.files.size())
                         ? std::to_string(cfg.files.size()) + " loaded"
                         : std::to_string(cfg.enabledCount) + " of "
                               + std::to_string(cfg.files.size()) + " loaded");

    _clearFiles->setEnabled(!cfg.files.empty());

    _terminal->setVisible(cfg.captureOutput && !cfg.dosPort && !cfg.autoClose);
    _terminal->setEnabled(indexOf(State::get().runs.logged, cfg.profileKey) >= 0);
    _terminal->tooltip(_terminal->enabled() ? "Show what this profile printed"
                                        : "Nothing has been launched from this profile yet");

    _launch->setEnabled(ready());
}

void ProfilePage::syncReplay() const {
    const State::Cfg &cfg = State::get().cfg;

    _replay->setVisible(cfg.replayRecords);

    if (!cfg.replayRecords) {
        return;
    }

    const bool broken = cfg.replayMode != ReplayMode::Off && cfg.replayFile.empty();
    const bool wrong = broken || !cfg.replayTrouble.empty();

    _replay->setOpen(cfg.replayOpen);
    _replay->setSaid(replaySummary(), wrong);

    _replay->pill()->setVisible(cfg.replayMode != ReplayMode::Off);
    _replay->pill()->setText(cfg.replayMode == ReplayMode::Record ? "Recording" : "Playing");
    _replay->pill()->kind(wrong ? Pill::Kind::Warning : Pill::Kind::None);

    _replayMode->setCurrent(static_cast<int>(cfg.replayMode));
    _replayReset->setEnabled(cfg.replaySet);
    _replayReset->tooltip(cfg.replaySet ? "Put every replay setting back to its default"
                                    : "Nothing here has been set");

    _replayRecord->setVisible(cfg.replayMode == ReplayMode::Record);
    _replayPlay->setVisible(cfg.replayMode == ReplayMode::Play);

    if (cfg.replayMode == ReplayMode::Off) {
        _replayNote->setVisible(true);
        _replayNote->setText("This profile neither records nor plays anything back. Record "
                             "writes what is played into the profile's own replays folder; Play "
                             "runs one of them back.");
        _replayNote->tone(Theme::of().faint);
    } else if (!cfg.replayTrouble.empty()) {
        _replayNote->setVisible(true);
        _replayNote->setText(cfg.replayTrouble);
        _replayNote->tone(Theme::of().danger);
    } else if (broken) {
        _replayNote->setVisible(true);
        _replayNote->setText(
            cfg.replayMode == ReplayMode::Record
                ? "Without a name there is nothing to record into, and the profile launches "
                  "without recording."
                : cfg.replayFiles.empty()
                    ? "This profile has recorded nothing yet, so there is nothing to play back. "
                      "Record writes a demo the next time it is launched."
                    : "Without a demo picked there is nothing to play back, and the profile "
                      "launches an ordinary game.");
        _replayNote->tone(Theme::of().warning);
    } else if (cfg.replayMode == ReplayMode::Record && cfg.replayNameTaken) {
        _replayNote->setVisible(true);
        _replayNote->setText("A demo by that name is in the folder already. Some ports record "
                             "into a numbered name beside it, others write over it.");
        _replayNote->tone(Theme::of().warning);
    } else {
        _replayNote->setVisible(false);
    }

    if (_replayName->text() != cfg.replayFile && cfg.replayMode == ReplayMode::Record) {
        _replayName->setText(cfg.replayFile);
    }

    _replayFile->setOptions(cfg.replayFiles);
    _replayFile->setCurrent(cfg.replayIndex);
    _replayFile->setEnabled(!cfg.replayFiles.empty());
    _replayFile->placeholder(cfg.replayFiles.empty() ? "Nothing recorded yet" : "Nothing picked");

    std::vector<MultistateSwitch::Choice> speeds = {
        {.value = static_cast<int>(Playback::AsRecorded), .label = "As recorded"},
        {.value = static_cast<int>(Playback::Timed), .label = "Timed"}};

    if (cfg.replayFast) {
        speeds.push_back({.value = static_cast<int>(Playback::Fast),
                          .label = "As fast as it draws"});
    }

    _replaySpeed->setOptions(std::move(speeds));
    _replaySpeed->setCurrent(static_cast<int>(cfg.replayPlayback));
    _replaySpeed->parent()->setVisible(cfg.replayTimed);

    _replayPath->setVisible(!broken && cfg.replayMode != ReplayMode::Off);
    _replayPath->setText(cfg.replayPath);

    const bool tunable = cfg.replayHasComplevel || cfg.replayHasLongtics || cfg.replayHasSoloNet;

    _replayTune->setVisible(cfg.replayMode == ReplayMode::Record && tunable);

    _complevel->setVisible(cfg.replayHasComplevel);
    _complevel->setOptions(cfg.replayComplevels);
    _complevel->setBadges(cfg.replayComplevelNumbers);
    _complevel->setCurrent(cfg.replayComplevel);

    _longtics->setVisible(cfg.replayHasLongtics);
    _longtics->setChecked(cfg.replayLongtics);

    _soloNet->setVisible(cfg.replayHasSoloNet);
    _soloNet->setChecked(cfg.replaySoloNet);
}

void ProfilePage::syncSaves() const {
    const State::Cfg &cfg = State::get().cfg;

    _saves->setVisible(cfg.saveLoads);

    if (!cfg.saveLoads) {
        return;
    }

    const bool broken = cfg.saveEnabled && cfg.saveFile.empty();
    const bool homeless = cfg.saveFolder.empty();

    _saves->setOpen(cfg.saveOpen);
    _saves->setSaid(saveSummary(), broken || !cfg.saveTrouble.empty());

    _saveOn->setCurrent(cfg.saveEnabled ? 1 : 0);

    _saveFile->setOptions(cfg.saveFiles);
    _saveFile->setBadges(cfg.saveSlotLabels);
    _saveFile->setCurrent(cfg.saveIndex);
    _saveFile->setEnabled(!cfg.saveFiles.empty());
    _saveFile->placeholder(cfg.saveFiles.empty() ? "Nothing saved yet" : "Nothing picked");
    _saveFile->tooltip(cfg.saveSlots
                       ? "The saves in this profile's folder, newest first. The port is handed "
                         "the slot it sits in"
                       : "The saves in this profile's folder, newest first");

    if (!cfg.saveTrouble.empty()) {
        _saveNote->setVisible(true);
        _saveNote->setText(cfg.saveTrouble);
        _saveNote->tone(Theme::of().danger);
    } else if (homeless) {
        _saveNote->setVisible(true);
        _saveNote->setText("This profile launches on the settings the source port keeps for "
                           "itself, so its saves are the port's own and there is nothing here "
                           "to list. Give it settings of its own to keep them apart.");
        _saveNote->tone(Theme::of().faint);
    } else if (cfg.saveFiles.empty()) {
        _saveNote->setVisible(true);
        _saveNote->setText("Nothing has been saved in this profile yet. A game saved while it "
                           "is playing lands in its saves folder and shows up here.");
        _saveNote->tone(Theme::of().faint);
    } else if (broken) {
        _saveNote->setVisible(true);
        _saveNote->setText("Without a save picked there is nothing to load, and the profile "
                           "launches a new game.");
        _saveNote->tone(Theme::of().warning);
    } else if (!cfg.saveFile.empty() && cfg.replayMode != ReplayMode::Off) {
        _saveNote->setVisible(true);
        _saveNote->setText(cfg.replayMode == ReplayMode::Record
                               ? "A demo is being recorded, which starts where a new game "
                                 "starts, so the save is left out of the launch."
                               : "A demo is being played back, so the save is left out of the "
                                 "launch.");
        _saveNote->tone(Theme::of().warning);
    } else {
        _saveNote->setVisible(false);
    }

    _savePath->setVisible(!cfg.saveFile.empty());
    _savePath->setText(cfg.savePath);
}

void ProfilePage::syncNet() const {
    const State::Cfg &cfg = State::get().cfg;

    _net->setVisible(cfg.netHosts || cfg.netJoins);

    if (!_net->visible()) {
        return;
    }

    const bool unsupported = (cfg.netRole == NetRole::Host && !cfg.netHosts)
        || (cfg.netRole == NetRole::Join && !cfg.netJoins);
    const bool broken = unsupported || (cfg.netRole == NetRole::Join && cfg.host.empty());

    _net->setOpen(cfg.multiplayerOpen);
    _net->setSaid(netSummary(), broken);

    _net->pill()->setVisible(cfg.netRole != NetRole::Alone);
    _net->pill()->setText(cfg.netRole == NetRole::Join           ? "Joining"
                          : cfg.gameType == GameType::Coop       ? "Co-op"
                          : cfg.gameType == GameType::Deathmatch ? "Deathmatch"
                                                                 : "Alt deathmatch");
    _net->pill()->kind(broken ? Pill::Kind::Warning : Pill::Kind::None);

    _role->setCurrent(static_cast<int>(cfg.netRole));
    _netReset->setEnabled(cfg.multiplayerSet);
    _netReset->tooltip(cfg.multiplayerSet ? "Put every multiplayer setting back to its default"
                                      : "Nothing here has been set");

    const bool hosting = cfg.netRole == NetRole::Host && cfg.netHosts;
    const bool joining = cfg.netRole == NetRole::Join && cfg.netJoins;

    _hosting->setVisible(hosting);
    _joining->setVisible(joining);
    _rules->setVisible(hosting);

    if (cfg.netRole == NetRole::Alone) {
        _netNote->setVisible(true);
        _netNote->setText("This profile starts a game for one. Host opens a game other machines "
                          "can connect to; Join connects to one somebody else is running.");
        _netNote->tone(Theme::of().faint);
    } else if (unsupported) {
        _netNote->setVisible(true);
        _netNote->setText(cfg.netRole == NetRole::Host
                              ? "This port opens no game of its own: a server program beside it "
                                "does, and the port joins that. Nothing under here reaches the "
                                "launch."
                              : "This port has no way to join a game, so nothing under here "
                                "reaches the launch.");
        _netNote->tone(Theme::of().warning);
    } else if (joining) {
        _netNote->setVisible(true);
        _netNote->setText(cfg.host.empty()
                              ? "Without an address there is nothing to join, and the profile "
                                "launches a single player game."
                              : "How the game is played is the host's to decide, so there is "
                                "nothing else to set on this side.");
        _netNote->tone(cfg.host.empty() ? Theme::of().warning : Theme::of().faint);
    } else {
        _netNote->setVisible(false);
    }

    _gameType->setCurrent(static_cast<int>(cfg.gameType));

    _players->setVisible(cfg.netPlayers);
    _players->setValue(cfg.players);

    _listed->parent()->setVisible(cfg.netListing);
    _listed->setChecked(cfg.listed);

    if (_netPort->text() != cfg.netPort) {
        _netPort->setText(cfg.netPort);
    }

    if (_host->text() != cfg.host) {
        _host->setText(cfg.host);
    }

    if (_joinPort->text() != cfg.netPort) {
        _joinPort->setText(cfg.netPort);
    }

    _fragLimit->setVisible(cfg.netFragLimit);
    _dmflags->setVisible(cfg.netFlags);
    _dmflags2->setVisible(cfg.netFlags);
    _savegame->setVisible(cfg.netSavegame);
    _saveClash->setVisible(cfg.netSavegame && !cfg.savegame.empty() && cfg.saveEnabled
                           && !cfg.saveFile.empty());

    if (_fragLimit->text() != cfg.fragLimit) {
        _fragLimit->setText(cfg.fragLimit);
    }

    if (_timeLimit->text() != cfg.timeLimit) {
        _timeLimit->setText(cfg.timeLimit);
    }

    if (_dmflags->text() != cfg.dmflags) {
        _dmflags->setText(cfg.dmflags);
    }

    if (_dmflags2->text() != cfg.dmflags2) {
        _dmflags2->setText(cfg.dmflags2);
    }

    if (_savegame->text() != cfg.savegame) {
        _savegame->setText(cfg.savegame);
    }

    const bool tunable = cfg.netRole != NetRole::Alone && !unsupported
        && (cfg.netExtratic || cfg.hasNetmode || cfg.netDup);

    const bool turned = State::get().nav.tuning;

    _tuning->setVisible(tunable);

    _tuningHead->setOpen(turned);
    _tuningHead->setSaid(turned ? std::string() : tuningSummary());

    _netmode->parent()->setVisible(cfg.hasNetmode);
    _netmode->setCurrent(std::clamp(cfg.netmode, NETMODE_PORT, 1));

    _dup->setVisible(cfg.netDup);
    _dup->setValue(cfg.dup);

    _extratic->parent()->setVisible(cfg.netExtratic);
    _extratic->setCurrent(cfg.extratic ? 1 : 0);
}

void ProfilePage::syncCommand() const {
    const State::Cfg &cfg = State::get().cfg;

    _override->setChecked(cfg.commandOverride);

    _extra->setVisible(!cfg.commandOverride);
    _command->setVisible(cfg.commandOverride);
    _tokens->setVisible(cfg.commandOverride);

    if (!cfg.commandOverride && _extra->text() != cfg.extra) {
        _extra->setText(cfg.extra);
    }

    if (cfg.commandOverride && _command->text() != cfg.command) {
        _command->setText(cfg.command);
    }

    _resolved->setText(!cfg.commandTrouble.empty()  ? cfg.commandTrouble
                       : !cfg.commandLine.empty()   ? cfg.commandLine
                       : cfg.dosPort && cfg.dosbox.empty() && cfg.systemDosbox.empty()
                           ? "A DOS source port, and no DOSBox to run it in. Set one in "
                             "Settings."
                           : "Nothing to run yet.");

    _resolved->tone(!cfg.commandTrouble.empty() ? Theme::of().danger
                    : ready()                   ? Theme::of().muted
                                                : Theme::of().faint);

    _copy->setEnabled(!cfg.commandLine.empty());

    // Only a launch that runs DOSBox spends anything, generated or typed.
    _budget->setVisible(cfg.dosCommands > 0);
    _budget->setTight(cfg.dosCommands >= Dos::COMMANDS);
    _budget->setText(std::to_string(cfg.dosCommands) + " / " + std::to_string(Dos::COMMANDS)
                     + " DOSBox commands");
}

void ProfilePage::sync() const {
    const State::Cfg &cfg = State::get().cfg;
    const bool empty = cfg.profileIndex < 0;

    _none->setVisible(empty);
    children().front()->setVisible(!empty);

    if (empty) {
        return;
    }

    _chooser->setSaid(summary(), ready());
    _chooser->setStatus(components::statusOf(_reach->runs.stateOf(cfg.profileKey)),
                        _reach->runs.reasonOf(cfg.profileKey));

    syncRun();
    syncReplay();
    syncSaves();
    syncNet();
    syncCommand();
}

void ProfilePage::showMenu() const {
    if (root() == nullptr) {
        return;
    }

    const State::Cfg &cfg = State::get().cfg;

    const std::vector<Menu::Row> rows = {
        Menu::item(ProfileMenuAction::Rename, "Rename", Glyphs::Glyph::Edit),
        Menu::item(ProfileMenuAction::Duplicate, "Duplicate", Glyphs::Glyph::Extract),
        Menu::item(ProfileMenuAction::Clear, "Reset", Glyphs::Glyph::Refresh),
        Menu::rule(),
        Menu::item(ProfileMenuAction::CopyConfig, "Copy port config", Glyphs::Glyph::Copy,
                   false, cfg.port.empty()),
        Menu::rule(),
        Menu::item(ProfileMenuAction::LoadZdl, "Import .zdl", Glyphs::Glyph::Download),
        Menu::item(ProfileMenuAction::SaveZdl, "Save as .zdl", Glyphs::Glyph::Save),
        Menu::rule(),
        Menu::item(ProfileMenuAction::Delete, "Delete", Glyphs::Glyph::Trash,
                   true),
    };

    const double tall = Menu::heightOf(rows);
    const BLRect cog = _cog->box();

    Widget *menu = root()->layer(Root::POPUPS)->add(
        std::make_unique<Menu>(rows, [this](const int action) {
            root()->dismiss();

            const State::Cfg &held = State::get().cfg;

            switch (static_cast<ProfileMenuAction>(action)) {
                case ProfileMenuAction::Rename:
                    _reach->prompt("Rename profile", "Name", held.profileName, "Rename",
                                   [this](const std::string &named) {
                                       _reach->config.profile().renameProfile(named);
                                   });
                    break;
                case ProfileMenuAction::Duplicate:
                    _reach->config.profile().duplicateProfile();
                    break;
                case ProfileMenuAction::CopyConfig:
                    _reach->copyConfig();
                    break;
                case ProfileMenuAction::Clear:
                    _reach->ask("Empty \"" + held.profileName + "\"?",
                                "Everything this profile launches is emptied: the port, the "
                                "game, the files and the multiplayer settings. The profile "
                                "itself stays.",
                                "Empty", true,
                                [this] { _reach->config.profile().clearProfile(); });
                    break;
                case ProfileMenuAction::Delete:
                    _reach->ask("Delete \"" + held.profileName + "\"?",
                                "The profile and everything in it goes. The files it loaded are "
                                "left alone.",
                                "Delete", true, [this] {
                                    _reach->config.profile().removeProfile();
                                    _reach->go(State::Page::Library);
                                });
                    break;
                case ProfileMenuAction::LoadZdl:
                    _reach->picker.open(FilePicker::Action::LoadZdl, "Load a .zdl launch config", Filters::zdl(),
                                        false, false, false, FilePicker::Slot::Zdl);
                    break;
                case ProfileMenuAction::SaveZdl:
                    _reach->picker.openSave(FilePicker::Action::SaveZdl, "Save this profile as a .zdl",
                                            Filters::zdl(), FilePicker::Slot::Zdl,
                                            ProfileBridge::zdlFileName());
                    break;
            }
        }));

    menu->place(BLRect{cog.x + cog.w - Menu::WIDTH, cog.y + cog.h + 4.0, Menu::WIDTH, tall},
                root()->type());

    Widget  const*held = menu;

    root()->setDismiss([this, held] {
        const BLRect was = held->box();

        root()->layer(Root::POPUPS)->erase(held);
        root()->damage(was);

        _cog->spun(false);
    }, _cog);

    _cog->spun(true);

    menu->invalidate();
}

}
