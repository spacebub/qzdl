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

#include <utility>
#include <vector>

#include "core/config/Profile.h"
#include "gui/components/ReplayPanel.h"
#include "gui/draw/Glyphs.h"
#include "gui/draw/Theme.h"
#include "gui/services/Filters.h"
#include "gui/state/State.h"
#include "gui/toolkit/controls/Pill.h"
#include "gui/toolkit/layout/Pair.h"
#include "gui/toolkit/layout/Rule.h"
#include "gui/toolkit/layout/Spacer.h"
#include "gui/util/Desktop.h"
#include "gui/util/Format.h"

namespace components {

using namespace toolkit;

ReplayPanel::ReplayPanel(Reach *reach)
    : CollapsiblePanel("Replay",
                       [reach](const bool open) { reach->config.panels().setReplayOpen(open); }),
      _reach(reach) {
    _mode = tools()->append(std::make_unique<MultistateSwitch>(
        [this](const int value) {
            const auto mode = static_cast<ReplayMode>(value);

            _reach->config.panels().setReplayMode(mode);
            _reach->config.panels().setReplayOpen(mode != ReplayMode::Off);
        }));

    _mode->setOptions({{.value = static_cast<int>(ReplayMode::Off), .label = "Off"},
                       {.value = static_cast<int>(ReplayMode::Record), .label = "Record"},
                       {.value = static_cast<int>(ReplayMode::Play), .label = "Play"}});

    // Last, so it sits against the far edge of the heading.
    _reset = tools()->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Refresh, [this] {
        _reach->ask("Reset the replay settings?",
                  "The profile goes back to recording nothing and playing nothing back. The "
                  "demos already in its replays folder are left where they are.",
                  "Reset", true, [this] { _reach->config.panels().clearReplay(); });
    }));

    _reset->size(Theme::control)->outlined();
    _reset->fixedWidth = Theme::control;

    Box *body = CollapsiblePanel::body();

    body->append(std::make_unique<Rule>());

    _note = body->append(std::make_unique<Label>());
    _note->font(400, Theme::fontSmall)->tone(Theme::of().faint)->wrap();

    // Recording.
    _record = body->append(Box::column());
    _record->spacing(10.0);

    _name = _record->append(std::make_unique<Field>("Record it as",
                                                    [this](const std::string &value) {
        _reach->config.panels().setReplayFile(value);
    }));

    _name->placeholder("A name for the demo")->mono()
        ->note(".lmp goes on the end by itself, and the file lands in the profile's replays "
               "folder");

    // Playing back.
    _play = body->append(Box::column());
    _play->spacing(10.0);

    Box *play = _play->append(std::make_unique<Pair>(900.0));

    play->spacing(20.0)->cross(Box::Place::End);

    Box *pick = play->append(Box::row());

    pick->spacing(8.0)->cross(Box::Place::End);

    _file = pick->append(std::make_unique<Select>("Replay", [this](const int index) {
        _reach->config.panels().setReplayIndex(index);
    }));

    _file->clearable()->tooltip("The demos in this profile's replays folder, newest first");
    _file->fixedWidth = 340.0;

    _refresh = pick->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Refresh, [this] {
        _reach->config.panels().refreshReplays();
    }));

    _refresh->size(Theme::control)->outlined()->tooltip("Refresh folder");
    _refresh->fixedWidth = Theme::control;

    _browse = pick->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Folder, [this] {
        _reach->picker.open(FilePicker::Action::Replay, "Select a replay", Filters::replay(), false, false,
                            false, FilePicker::Slot::Replay);
    }));

    _browse->size(Theme::control)->outlined()->tooltip("Play a demo from somewhere else");
    _browse->fixedWidth = Theme::control;

    pick->append(std::make_unique<Spacer>());

    Box *speed = play->append(Box::column());

    // Start, or the trough is drawn over the whole width rather than its tabs.
    speed->spacing(6.0)->cross(Box::Place::Start);

    speed->append(std::make_unique<Label>("How it plays"))->section();

    _speed = speed->append(std::make_unique<MultistateSwitch>([this](const int value) {
        _reach->config.panels().setReplayPlayback(static_cast<Playback>(value));
    }));

    _path = body->append(std::make_unique<Label>());
    _path->font(400, Theme::fontTiny)->tone(Theme::of().faint)->path();
    _path->onClick([] { Desktop::open(State::get().cfg.replayFolder); });
    _path->hint = "Show in file explorer";

    // Compatibility, while recording.
    _tune = body->append(Box::column());
    _tune->spacing(12.0);

    _tune->append(std::make_unique<Rule>());
    _tune->append(std::make_unique<Label>("Compatibility"))->section();

    Box *tune = _tune->append(Box::row());

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

std::string ReplayPanel::summary() {
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

void ReplayPanel::sync() {
    const State::Cfg &cfg = State::get().cfg;

    setVisible(cfg.replayRecords);

    if (!cfg.replayRecords) {
        return;
    }

    const bool broken = cfg.replayMode != ReplayMode::Off && cfg.replayFile.empty();
    const bool wrong = broken || !cfg.replayTrouble.empty();

    setOpen(cfg.replayOpen);
    setSaid(summary(), wrong);

    pill()->setVisible(cfg.replayMode != ReplayMode::Off);
    pill()->setText(cfg.replayMode == ReplayMode::Record ? "Recording" : "Playing");
    pill()->kind(wrong ? Pill::Kind::Warning : Pill::Kind::None);

    _mode->setCurrent(static_cast<int>(cfg.replayMode));
    _reset->setEnabled(cfg.replaySet);
    _reset->tooltip(cfg.replaySet ? "Put every replay setting back to its default"
                                    : "Nothing here has been set");

    _record->setVisible(cfg.replayMode == ReplayMode::Record);
    _play->setVisible(cfg.replayMode == ReplayMode::Play);

    if (cfg.replayMode == ReplayMode::Off) {
        _note->setVisible(true);
        _note->setText("This profile neither records nor plays anything back. Record "
                       "writes what is played into the profile's own replays folder; Play "
                       "runs one of them back.");
        _note->tone(Theme::of().faint);
    } else if (!cfg.replayTrouble.empty()) {
        _note->setVisible(true);
        _note->setText(cfg.replayTrouble);
        _note->tone(Theme::of().danger);
    } else if (broken) {
        _note->setVisible(true);
        _note->setText(
            cfg.replayMode == ReplayMode::Record
                ? "Without a name there is nothing to record into, and the profile launches "
                  "without recording."
                : cfg.replayFiles.empty()
                    ? "This profile has recorded nothing yet, so there is nothing to play back. "
                      "Record writes a demo the next time it is launched."
                    : "Without a demo picked there is nothing to play back, and the profile "
                      "launches an ordinary game.");
        _note->tone(Theme::of().warning);
    } else if (cfg.replayMode == ReplayMode::Record && cfg.replayNameTaken) {
        _note->setVisible(true);
        _note->setText("A demo by that name is in the folder already. Some ports record "
                       "into a numbered name beside it, others write over it.");
        _note->tone(Theme::of().warning);
    } else {
        _note->setVisible(false);
    }

    if (_name->text() != cfg.replayFile && cfg.replayMode == ReplayMode::Record) {
        _name->setText(cfg.replayFile);
    }

    _file->setOptions(cfg.replayFiles);
    _file->setCurrent(cfg.replayIndex);
    _file->setEnabled(!cfg.replayFiles.empty());
    _file->placeholder(cfg.replayFiles.empty() ? "Nothing recorded yet" : "Nothing picked");

    std::vector<MultistateSwitch::Choice> speeds = {
        {.value = static_cast<int>(Playback::AsRecorded), .label = "As recorded"},
        {.value = static_cast<int>(Playback::Timed), .label = "Timed"}};

    if (cfg.replayFast) {
        speeds.push_back({.value = static_cast<int>(Playback::Fast),
                          .label = "As fast as it draws"});
    }

    _speed->setOptions(std::move(speeds));
    _speed->setCurrent(static_cast<int>(cfg.replayPlayback));
    _speed->parent()->setVisible(cfg.replayTimed);

    _path->setVisible(!broken && cfg.replayMode != ReplayMode::Off);
    _path->setText(cfg.replayPath);

    const bool tunable = cfg.replayHasComplevel || cfg.replayHasLongtics || cfg.replayHasSoloNet;

    _tune->setVisible(cfg.replayMode == ReplayMode::Record && tunable);

    _complevel->setVisible(cfg.replayHasComplevel);
    _complevel->setOptions(cfg.replayComplevels);
    _complevel->setBadges(cfg.replayComplevelNumbers);
    _complevel->setCurrent(cfg.replayComplevel);

    _longtics->setVisible(cfg.replayHasLongtics);
    _longtics->setChecked(cfg.replayLongtics);

    _soloNet->setVisible(cfg.replayHasSoloNet);
    _soloNet->setChecked(cfg.replaySoloNet);
}

}
