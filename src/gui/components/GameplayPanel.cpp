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

bool isCompatibility(const Flags::Word word) {
    return Flags::info(word).family == Flags::Family::compatibility;
}

bool isToggle(const Options::Option &option) {
    return option.kind == Options::Kind::toggle;
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

    _entries = _rules->append(std::make_unique<Wrap>());
    _entries->spacing(12.0, 10.0);

    for (std::size_t at = 0; at < Options::ALL.size(); ++at) {
        const Options::Option &option = Options::ALL[at];
        const Dialect::Feature feature = option.feature;

        if (!Options::RULES.has(feature)) {
            continue;
        }

        if (isToggle(option)) {
            _options[at] = _switches->append(optionToggle(_reach->config, option));

            continue;
        }

        Field *field = _entries->append(
                std::make_unique<Field>(std::string(option.label), [this, feature](const std::string &value) {
                    _reach->config.panels().setNumber(feature, value);
                }));

        field->placeholder(option.placeholder.empty() ? Dialect::PORTS_OWN : option.placeholder)->mono();
        field->minWidth = 0.0;
        field->fixedWidth = FIELD_WIDTH;

        _options[at] = field;
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

// After the option numbers, so they lead the wrap.
void GameplayPanel::buildWords() {
    for (const Flags::Word word : Flags::ALL) {
        const Flags::Family family = Flags::info(word).family;
        Wrap *into = family == Flags::Family::compatibility ? _compatibilityEntries : _entries;

        Field *field = into->append(
                std::make_unique<Field>(std::string(Flags::name(word)), [this, word](const std::string &value) {
                    _reach->config.panels().setFlag(word, value);
                }));

        field->placeholder(Dialect::PORTS_OWN)->mono();
        field->icon(Glyphs::Glyph::Edit, "Edit with check boxes", [this, family] { _reach->editFlags(family); });
        field->minWidth = 0.0;
        field->fixedWidth = FIELD_WIDTH;

        _wordFields[Flags::index(word)] = field;
    }
}

std::string GameplayPanel::summary() {
    const State::Cfg &cfg = State::get().cfg;
    std::string said;

    if (cfg.compatibility != Dialect::Compatibility::none && cfg.compatibilityIndex != 0) {
        const bool named =
                cfg.compatibilityIndex > 0 && std::cmp_less(cfg.compatibilityIndex, cfg.compatibilityNames.size());

        said = named ? cfg.compatibilityNames[static_cast<size_t>(cfg.compatibilityIndex)] : "custom compatibility";
    }

    for (const Flags::Word word : Flags::ALL) {
        const State::FlagField &field = cfg.flagFields[Flags::index(word)];

        // A compatibility mode already names what the compatibility words hold.
        const bool named = isCompatibility(word) && cfg.compatibility == Dialect::Compatibility::modes;

        if (cfg.features.has(Flags::feature(word)) && !field.text.empty() && !named) {
            addPart(said, Flags::name(word));
        }
    }

    for (std::size_t at = 0; at < Options::ALL.size(); ++at) {
        const Options::Option &option = Options::ALL[at];
        const State::OptionField &field = cfg.optionFields[at];

        if (!(cfg.features & Options::RULES).has(option.feature)) {
            continue;
        }

        if (isToggle(option) && field.on) {
            addPart(said, option.label);
        } else if (!isToggle(option) && !field.text.empty()) {
            addPart(said, option.label);
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
        const bool shown = cfg.features.has(Flags::feature(word));

        if (isCompatibility(word)) {
            compatibilityEntries = compatibilityEntries || shown;
        } else {
            entries = entries || shown;
        }

        bad = bad || (shown && field.bad);
    }

    for (std::size_t at = 0; at < Options::ALL.size(); ++at) {
        const State::OptionField &field = cfg.optionFields[at];
        const bool shown = (cfg.features & Options::RULES).has(Options::ALL[at].feature);

        if (isToggle(Options::ALL[at])) {
            toggles = toggles || shown;
        } else {
            entries = entries || shown;
        }

        bad = bad || (shown && field.bad);
    }

    const bool compatibility = cfg.compatibility != Dialect::Compatibility::none || compatibilityEntries;

    set_visible(toggles || entries || compatibility);

    if (!visible()) {
        return;
    }

    set_open(cfg.gameplayOpen);
    summarise(summary(), bad || cfg.complevelNeeded);

    _rules->set_visible(toggles || entries);
    _switches->set_visible(toggles);
    _entries->set_visible(entries);
    _compatibility->set_visible(compatibility);
    _compatibilityEntries->set_visible(compatibilityEntries);

    syncRules();
    syncCompatibility();
    syncWords();
}

void GameplayPanel::syncRules() {
    const State::Cfg &cfg = State::get().cfg;

    for (std::size_t at = 0; at < Options::ALL.size(); ++at) {
        const State::OptionField &state = cfg.optionFields[at];
        Widget *control = _options[at];

        if (control == nullptr) {
            continue;
        }

        syncOption(control, at);

        if (isToggle(Options::ALL[at])) {
            continue;
        }

        auto *field = static_cast<Field *>(control);

        if (field->text() != state.text) {
            field->set_text(state.text);
        }

        // A badge changes the height of the field, so it is set only when it changes.
        std::string badge;

        if (state.bad) {
            badge = std::to_string(state.least) + " to " + std::to_string(state.most);
        }

        if (_badges[at] != badge) {
            field->badge(badge, Pill::Kind::Warning);
            _badges[at] = std::move(badge);
        }
    }
}

void GameplayPanel::syncCompatibility() {
    const State::Cfg &cfg = State::get().cfg;

    _level->set_visible(cfg.compatibility != Dialect::Compatibility::none);
    _level->set_options(cfg.compatibilityNames);
    _level->set_badges(cfg.compatibilityBadges);
    _level->set_current(cfg.compatibilityIndex);
    _level->tooltip(cfg.compatibilityHint);

    _needed->set_visible(cfg.complevelNeeded);
    _needed->set_text(cfg.complevelName.empty()
                              ? "The profile records a demo, and this port records only with a complevel."
                              : "The profile records a demo, and this port does not record at this complevel.");

    _kept->set_visible(cfg.features.has(Dialect::Feature::keepsCompatflags));
}

void GameplayPanel::syncWords() {
    const State::Cfg &cfg = State::get().cfg;

    for (const Flags::Word word : Flags::ALL) {
        const size_t at = Flags::index(word);
        const State::FlagField &state = cfg.flagFields[at];
        Field *field = _wordFields[at];

        field->set_visible(cfg.features.has(Flags::feature(word)));

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
