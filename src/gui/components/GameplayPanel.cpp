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

#include <string_view>
#include <utility>

#include "ttk/draw/Glyphs.h"
#include "ttk/draw/Theme.h"
#include "ttk/toolkit/Root.h"
#include "ttk/toolkit/controls/Pill.h"
#include "ttk/toolkit/layout/Rule.h"

#include "gui/components/GameplayPanel.h"
#include "gui/components/Parts.h"
#include "gui/state/State.h"

using namespace ttk;

namespace components {

namespace {

constexpr double FIELD_WIDTH = 200.0;
// What an empty field shows.
constexpr std::string_view PORT_DEFAULT = "Port's own";

bool isCompatibility(const Flags::Word word) {
    return Flags::info(word).family == Flags::Family::compatibility;
}

bool isToggle(const Rules::Rule rule) {
    return Rules::info(rule).kind == Rules::Kind::toggle;
}

std::string_view levelHint(const State::Compatibility kind) {
    switch (kind) {
        case State::Compatibility::modes:
            return "The compatibility mode of the port. A mode fills compatflags and compatflags2, and Custom means "
                   "they match no mode";
        case State::Compatibility::versions:
            return "The Doom release the port plays as, passed on every launch, demos played back included";
        case State::Compatibility::complevels:
        case State::Compatibility::none:
            break;
    }

    return "The complevel, passed on every launch. A demo played back keeps its own";
}

void addPart(std::string &said, const std::string_view part) {
    if (!said.empty()) {
        said += " · ";
    }

    said += part;
}

// Filled across, since a wrap given only its natural width stacks the fields in one column.
Box *group(Box *body, const char *caption) {
    Box *made = body->append(Box::column());

    made->spacing(6.0);
    made->append(std::make_unique<Label>(caption))->section();

    return made;
}

}

GameplayPanel::GameplayPanel(Reach *reach)
    : SummaryPanel("Gameplay", [reach](const bool open) { reach->config.panels().setGameplayOpen(open); }),
      _reach(reach) {
    Box *body = CollapsiblePanel::body();

    body->append(std::make_unique<Rule>());

    buildRules(body);
    buildCompatibility(body);
    buildWords();
}

void GameplayPanel::buildRules(Box *body) {
    _rules = group(body, "Rules");

    _switches = _rules->append(std::make_unique<Wrap>());
    _switches->spacing(16.0, 8.0);

    _pistolStart = _switches->append(std::make_unique<Toggle>(
            "Pistol start", [this](const bool on) { _reach->config.profile().setPistolStart(on); }));

    _cheats = _switches->append(std::make_unique<Toggle>(
            "Allow cheats", [this](const bool on) { _reach->config.profile().setCheats(on); }));

    _cheats->hint = "Turns on sv_cheats, which a netgame or the hardest skill needs before any cheat works";

    _entries = _rules->append(std::make_unique<Wrap>());
    _entries->spacing(12.0, 10.0);

    for (const Rules::Rule rule : Rules::ALL) {
        const Rules::RuleInfo &info = Rules::info(rule);

        if (isToggle(rule)) {
            _ruleToggles[Rules::index(rule)] =
                    _switches->append(std::make_unique<Toggle>(std::string(info.label), [this, rule](const bool on) {
                        _reach->config.panels().setRuleOn(rule, on);
                    }));

            continue;
        }

        Field *field = _entries->append(
                std::make_unique<Field>(std::string(info.label), [this, rule](const std::string &value) {
                    _reach->config.panels().setRule(rule, value);
                }));

        field->placeholder(info.placeholder.empty() ? PORT_DEFAULT : info.placeholder)->mono();
        field->minWidth = 0.0;
        field->fixedWidth = FIELD_WIDTH;

        _ruleFields[Rules::index(rule)] = field;
    }
}

void GameplayPanel::buildCompatibility(Box *body) {
    _compatibility = group(body, "Compatibility");

    _level = _compatibility->append(
            std::make_unique<Select>("", [this](const int index) { _reach->config.panels().setCompatibility(index); }));

    _level->placeholder("Custom");
    _level->fixedWidth = 280.0;

    _needed = _compatibility->append(std::make_unique<Label>());
    _needed->font(400, Theme::fontSmall)->tone(&Theme::Palette::warning)->wrap();

    _compatibilityEntries = _compatibility->append(std::make_unique<Wrap>());
    _compatibilityEntries->spacing(12.0, 10.0);

    _kept = _compatibility->append(std::make_unique<Label>(
            "The port keeps both compatibility words in its own settings. An empty field runs as it ran last "
            "time, and 0 clears it."));
    _kept->font(400, Theme::fontSmall)->tone(&Theme::Palette::faint)->wrap();
}

// After the rule numbers, so they lead the wrap.
void GameplayPanel::buildWords() {
    for (const Flags::Word word : Flags::ALL) {
        const Flags::Family family = Flags::info(word).family;
        Wrap *into = family == Flags::Family::compatibility ? _compatibilityEntries : _entries;

        Field *field = into->append(
                std::make_unique<Field>(std::string(Flags::name(word)), [this, word](const std::string &value) {
                    _reach->config.panels().setFlag(word, value);
                }));

        field->placeholder(PORT_DEFAULT)->mono();
        field->icon(Glyphs::Glyph::Edit, "Edit with check boxes", [this, family] { _reach->editFlags(family); });
        field->minWidth = 0.0;
        field->fixedWidth = FIELD_WIDTH;

        _wordFields[Flags::index(word)] = field;
    }
}

std::string GameplayPanel::summary() {
    const State::Cfg &cfg = State::get().cfg;
    std::string said;

    if (cfg.compatibility != State::Compatibility::none && cfg.compatibilityIndex != 0) {
        const bool named =
                cfg.compatibilityIndex > 0 && std::cmp_less(cfg.compatibilityIndex, cfg.compatibilityNames.size());

        said = named ? cfg.compatibilityNames[static_cast<size_t>(cfg.compatibilityIndex)] : "custom compatibility";
    }

    for (const Flags::Word word : Flags::ALL) {
        const State::FlagField &field = cfg.flagFields[Flags::index(word)];

        // A compatibility mode already names what the compatibility words hold.
        const bool named = isCompatibility(word) && cfg.compatibility == State::Compatibility::modes;

        if (field.shown && !field.text.empty() && !named) {
            addPart(said, Flags::name(word));
        }
    }

    for (const Rules::Rule rule : Rules::ALL) {
        const State::RuleField &field = cfg.ruleFields[Rules::index(rule)];

        if (!field.shown) {
            continue;
        }

        if (isToggle(rule) && field.on) {
            addPart(said, Rules::info(rule).label);
        } else if (!isToggle(rule) && !field.text.empty()) {
            addPart(said, Rules::info(rule).label);
            said += ' ';
            said += field.text;
        }
    }

    return said;
}

void GameplayPanel::sync() {
    const State::Cfg &cfg = State::get().cfg;

    bool toggles = false;
    bool entries = false;
    bool compatibilityEntries = false;
    bool bad = false;

    for (const Flags::Word word : Flags::ALL) {
        const State::FlagField &field = cfg.flagFields[Flags::index(word)];

        if (isCompatibility(word)) {
            compatibilityEntries = compatibilityEntries || field.shown;
        } else {
            entries = entries || field.shown;
        }

        bad = bad || (field.shown && field.bad);
    }

    for (const Rules::Rule rule : Rules::ALL) {
        const State::RuleField &field = cfg.ruleFields[Rules::index(rule)];

        if (isToggle(rule)) {
            toggles = toggles || field.shown;
        } else {
            entries = entries || field.shown;
        }

        bad = bad || (field.shown && field.bad);
    }

    const bool rules = cfg.hasPistolStart || cfg.hasCheats || toggles;
    const bool compatibility = cfg.compatibility != State::Compatibility::none || compatibilityEntries;

    set_visible(rules || entries || compatibility);

    if (!visible()) {
        return;
    }

    set_open(cfg.gameplayOpen);
    summarise(summary(), bad || cfg.complevelNeeded);

    _rules->set_visible(rules || entries);
    _switches->set_visible(rules);
    _entries->set_visible(entries);
    _compatibility->set_visible(compatibility);
    _compatibilityEntries->set_visible(compatibilityEntries);

    syncRules();
    syncCompatibility();
    syncWords();
}

void GameplayPanel::syncRules() {
    const State::Cfg &cfg = State::get().cfg;

    _pistolStart->set_visible(cfg.hasPistolStart);
    _pistolStart->set_checked(cfg.pistolStart);
    _pistolStart->hint = cfg.pistolStartSwitch
                                 ? "Starts every map with only the pistol. Left out while a demo is recorded or "
                                   "played and in a netgame, since the port stops on it there"
                                 : "Starts every map with only the pistol";
    _cheats->set_visible(cfg.hasCheats);
    _cheats->set_checked(cfg.cheats);

    for (const Rules::Rule rule : Rules::ALL) {
        const size_t at = Rules::index(rule);
        const State::RuleField &state = cfg.ruleFields[at];

        if (Toggle *toggle = _ruleToggles[at]; toggle != nullptr) {
            toggle->set_visible(state.shown);
            toggle->set_checked(state.on);

            if (toggle->hint != state.hint) {
                toggle->hint = state.hint;
            }

            continue;
        }

        Field *field = _ruleFields[at];

        field->set_visible(state.shown);

        if (field->hint != state.hint) {
            field->hint = state.hint;
        }

        if (field->text() != state.text) {
            field->set_text(state.text);
        }

        // A badge changes the height of the field, so it is set only when it changes.
        std::string badge;

        if (state.bad) {
            badge = std::to_string(state.least) + " to " + std::to_string(state.most);
        }

        if (_ruleBadges[at] != badge) {
            field->badge(badge, Pill::Kind::Warning);
            _ruleBadges[at] = std::move(badge);
        }
    }
}

void GameplayPanel::syncCompatibility() {
    const State::Cfg &cfg = State::get().cfg;

    _level->set_visible(cfg.compatibility != State::Compatibility::none);
    _level->set_options(cfg.compatibilityNames);
    _level->set_badges(cfg.compatibilityBadges);
    _level->set_current(cfg.compatibilityIndex);
    _level->tooltip(levelHint(cfg.compatibility));

    _needed->set_visible(cfg.complevelNeeded);
    _needed->set_text(cfg.complevelName.empty()
                              ? "The profile records a demo, and this port records only with a complevel."
                              : "The profile records a demo, and this port does not record at this complevel.");

    _kept->set_visible(cfg.compatibilityKept);
}

void GameplayPanel::syncWords() {
    const State::Cfg &cfg = State::get().cfg;

    for (const Flags::Word word : Flags::ALL) {
        const size_t at = Flags::index(word);
        const State::FlagField &state = cfg.flagFields[at];
        Field *field = _wordFields[at];

        field->set_visible(state.shown);

        if (field->text() != state.text) {
            field->set_text(state.text);
        }

        if (_wordBad[at] != state.bad) {
            _wordBad[at] = state.bad;
            field->badge(state.bad ? "Not a number" : "", Pill::Kind::Warning);
        }
    }
}

}
