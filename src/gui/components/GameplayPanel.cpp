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

#include <cstddef>
#include <utility>
#include <vector>

#include "ttk/draw/Glyphs.h"
#include "ttk/draw/Theme.h"
#include "ttk/toolkit/Root.h"
#include "ttk/toolkit/controls/Pill.h"
#include "ttk/toolkit/layout/Rule.h"

#include "gui/components/GameplayPanel.h"
#include "gui/state/State.h"

using namespace ttk;

namespace components {

namespace {

constexpr double WORD_WIDTH = 200.0;

bool compatibility(const size_t at) {
    return Flags::info(static_cast<Flags::Word>(at)).family == Flags::Family::compatibility;
}

}

GameplayPanel::GameplayPanel(Reach *reach)
    : CollapsiblePanel("Gameplay", [reach](const bool open) { reach->config.panels().setGameplayOpen(open); }),
      _reach(reach) {
    Box *body = CollapsiblePanel::body();

    body->append(std::make_unique<Rule>());

    // Filled across, since a wrap given only its natural width stacks the fields in one column.
    const auto group = [body](const char *caption) {
        Box *made = body->append(Box::column());

        made->spacing(6.0);
        made->append(std::make_unique<Label>(caption))->section();

        return made;
    };

    _rules = group("Rules");

    _switches = _rules->append(Box::row());
    _switches->spacing(16.0);

    _pistolStart = _switches->append(std::make_unique<Toggle>(
            "Pistol start", [this](const bool on) { _reach->config.profile().setPistolStart(on); }));

    _cheats = _switches->append(std::make_unique<Toggle>(
            "Allow cheats", [this](const bool on) { _reach->config.profile().setCheats(on); }));

    _cheats->hint = "Turns on sv_cheats, which a netgame or the hardest skill needs before any cheat works";

    _gameplayWords = _rules->append(std::make_unique<Wrap>());
    _gameplayWords->spacing(12.0, 10.0);

    _compatibility = group("Compatibility");

    _level = _compatibility->append(
            std::make_unique<Select>("", [this](const int index) { _reach->config.panels().setCompatibility(index); }));

    _level->placeholder("Custom");
    _level->fixedWidth = 280.0;

    _needed = _compatibility->append(
            std::make_unique<Label>("The profile records a demo, and this port records only with a complevel."));
    _needed->font(400, Theme::fontSmall)->tone(&Theme::Palette::warning)->wrap();

    _compatibilityWords = _compatibility->append(std::make_unique<Wrap>());
    _compatibilityWords->spacing(12.0, 10.0);

    _kept = _compatibility->append(std::make_unique<Label>(
            "The port keeps both compatibility words in its own settings. An empty field runs as it ran last "
            "time, and 0 clears it."));
    _kept->font(400, Theme::fontSmall)->tone(&Theme::Palette::faint)->wrap();

    for (size_t at = 0; at < Flags::WORDS; ++at) {
        const auto word = static_cast<Flags::Word>(at);
        const Flags::Family family = Flags::info(word).family;
        Wrap *into = compatibility(at) ? _compatibilityWords : _gameplayWords;

        Field *field = into->append(
                std::make_unique<Field>(std::string(Flags::info(word).name), [this, word](const std::string &value) {
                    _reach->config.panels().setFlag(word, value);
                }));

        field->placeholder("Port's own")->mono();
        field->icon(Glyphs::Glyph::Edit, "Edit with check boxes", [this, family] { _reach->editFlags(family); });
        field->minWidth = 0.0;
        field->fixedWidth = WORD_WIDTH;

        _fields[at] = field;
    }
}

std::string GameplayPanel::summary() {
    const State::Cfg &cfg = State::get().cfg;
    std::string said;

    if (cfg.hasCompatibility && cfg.compatibilityIndex != 0) {
        said = cfg.compatibilityIndex < 0
                               || std::cmp_greater_equal(cfg.compatibilityIndex, cfg.compatibilityNames.size())
                       ? std::string("custom compatibility")
                       : cfg.compatibilityNames[static_cast<size_t>(cfg.compatibilityIndex)];
    }

    for (size_t at = 0; at < cfg.flagFields.size(); ++at) {
        const State::FlagField &field = cfg.flagFields[at];

        // A compatibility mode already names what the compatibility words hold.
        if (!field.shown || field.text.empty() || (compatibility(at) && cfg.compatibilityModes)) {
            continue;
        }

        said += (said.empty() ? "" : " · ") + std::string(Flags::info(static_cast<Flags::Word>(at)).name);
    }

    return said;
}

void GameplayPanel::sync() {
    const State::Cfg &cfg = State::get().cfg;

    bool gameplayWords = false;
    bool compatibilityWords = false;
    bool bad = false;

    for (size_t at = 0; at < cfg.flagFields.size(); ++at) {
        const State::FlagField &field = cfg.flagFields[at];

        (compatibility(at) ? compatibilityWords : gameplayWords) |= field.shown;
        bad = bad || (field.shown && field.bad);
    }

    set_visible(cfg.hasPistolStart || cfg.hasCheats || cfg.hasCompatibility || gameplayWords || compatibilityWords);

    if (!visible()) {
        return;
    }

    set_open(cfg.gameplayOpen);
    set_said(summary(), bad || cfg.complevelNeeded);

    _rules->set_visible(cfg.hasPistolStart || cfg.hasCheats || gameplayWords);
    _switches->set_visible(cfg.hasPistolStart || cfg.hasCheats);
    _pistolStart->set_visible(cfg.hasPistolStart);
    _pistolStart->set_checked(cfg.pistolStart);
    _pistolStart->hint = cfg.pistolStartSwitch
                                 ? "Starts every map with only the pistol. Left out while a demo is recorded or "
                                   "played and in a netgame, since the port stops on it there"
                                 : "Starts every map with only the pistol";
    _cheats->set_visible(cfg.hasCheats);
    _cheats->set_checked(cfg.cheats);
    _gameplayWords->set_visible(gameplayWords);

    _compatibility->set_visible(cfg.hasCompatibility || compatibilityWords);
    _level->set_visible(cfg.hasCompatibility);
    _level->set_options(cfg.compatibilityNames);
    _level->set_badges(cfg.compatibilityBadges);
    _level->set_current(cfg.compatibilityIndex);
    _level->tooltip(cfg.compatibilityModes ? "The compatibility mode of the port. A mode fills compatflags and "
                                             "compatflags2, and Custom means they match no mode"
                                           : "The complevel, passed on every launch. A demo played back keeps "
                                             "its own");
    _needed->set_visible(cfg.complevelNeeded);

    // A label keeps its box when its text changes.
    if (const char *needed = cfg.complevelName.empty()
                                     ? "The profile records a demo, and this port records only with a complevel."
                                     : "The profile records a demo, and this port does not record at this complevel.";
        _needed->text() != needed) {
        _needed->set_text(needed);

        if (_needed->root() != nullptr) {
            _needed->root()->relayout();
        }
    }
    _compatibilityWords->set_visible(compatibilityWords);
    _kept->set_visible(cfg.compatibilityKept);

    for (size_t at = 0; at < cfg.flagFields.size() && at < _fields.size(); ++at) {
        const State::FlagField &state = cfg.flagFields[at];
        Field *field = _fields[at];

        field->set_visible(state.shown);

        if (field->text() != state.text) {
            field->set_text(state.text);
        }

        // A badge changes the height of the field, so it is set only when it changes.
        if (_bad[at] != state.bad) {
            _bad[at] = state.bad;
            field->badge(state.bad ? "Not a number" : "", Pill::Kind::Warning);
        }
    }
}

}
