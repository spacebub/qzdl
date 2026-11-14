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
#include <optional>
#include <string_view>

#include "ttk/draw/Theme.h"
#include "ttk/draw/Typeface.h"
#include "ttk/toolkit/Root.h"
#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/layout/Box.h"
#include "ttk/toolkit/layout/Scroll.h"
#include "ttk/toolkit/layout/Spacer.h"
#include "ttk/toolkit/layout/Wrap.h"

#include "gui/dialogs/FlagsDialog.h"

using namespace ttk;

namespace dialogs {

namespace {

constexpr double COLUMN = 392.0;

// Leaves the page in view behind a port with many options.
constexpr double TALLEST = 720.0;
constexpr double CAPTION = 150.0;

std::string tip(const Flags::Option &option) {
    const std::string_view word = Flags::info(option.word).name;
    std::string out(option.description);

    if (option.choices.empty()) {
        out += " Sets " + std::to_string(option.mask) + " in " + std::string(word);
    } else {
        out += " In " + std::string(word) + ":";

        for (const Flags::Choice &choice : option.choices) {
            if (choice.value != 0) {
                out += (out.back() == ':' ? " " : ", ") + std::string(choice.label) + " "
                       + std::to_string(choice.value);
            }
        }
    }

    if (!option.cvar.empty()) {
        out += ", " + std::string(option.cvar);
    }

    return out + ".";
}

}

FlagsDialog::FlagsDialog(const Flags::Family family,
                         const Flags::Set set,
                         const std::array<std::string, Flags::WORDS> &typed,
                         std::function<void(Edited)> accepted)
    : _set(set), _accepted(std::move(accepted)) {
    wanted = 900.0;

    for (size_t at = 0; at < Flags::WORDS; ++at) {
        const auto word = static_cast<Flags::Word>(at);
        const Flags::WordInfo &info = Flags::info(word);

        if (info.family != family || !Flags::reads(set, word)) {
            continue;
        }

        _held.push_back(Held{
                .word = word,
                .typed = typed[at],
                .value = Flags::parse(typed[at]).value_or(info.start),
                .touched = false,
                .cleared = false,
        });
    }

    Box *column = card()->append(Box::column());

    column->pad(22.0)->spacing(14.0);

    const bool gameplay = family == Flags::Family::gameplay;

    heading(column, gameplay ? "Gameplay flags" : "Compatibility flags");
    body(column,
         std::string(gameplay ? "Each box sets one rule of play."
                              : "Each box brings back one behaviour of an older engine.")
                 + " Hover over one to see what it does and the bit it sets. A loaded save brings back the words "
                   "it was saved with.");

    Scroll *scroll = column->append(std::make_unique<Scroll>());

    scroll->stretch = 1.0;

    Box *groups = static_cast<Box *>(scroll->hold(Box::column()));

    // The bar takes a lane at the right, which the boxes keep clear of.
    groups->spacing(18.0)->pad(0.0, 0.0, Theme::lane + 4.0, 0.0);

    std::optional<Flags::Group> current;
    Wrap *rows = nullptr;

    for (const Flags::Option &option : Flags::options()) {
        const auto held = std::ranges::find(_held, option.word, &Held::word);

        if (held == _held.end() || (option.sets & Flags::bit(set)) == 0) {
            continue;
        }

        if (option.group != current) {
            current = option.group;

            Box *group = groups->append(Box::column());

            group->spacing(8.0);
            group->append(std::make_unique<Label>(std::string(Flags::title(option.group))))->section();

            rows = group->append(std::make_unique<Wrap>());
            rows->spacing(16.0, 6.0);
        }

        const size_t index = _controls.size();
        Control control{.option = &option, .held = static_cast<size_t>(held - _held.begin())};

        if (option.choices.empty()) {
            control.check =
                    rows->append(std::make_unique<Check>(std::string(option.label), [this, index](const bool on) {
                        const Control &changed = _controls[index];
                        const std::uint32_t value = _held[changed.held].value;

                        assign(changed.held, on ? value | changed.option->mask : value & ~changed.option->mask);
                    }));

            control.check->fixedWidth = COLUMN;
            control.check->hint = tip(option);
        } else {
            Box *line = rows->append(Box::row());

            line->spacing(8.0)->cross(Box::Place::Centre);
            line->fixedWidth = COLUMN;
            line->fixedHeight = Theme::control;

            Label *caption = line->append(std::make_unique<Label>(std::string(option.label)));

            caption->fixedWidth = CAPTION;
            caption->hint = tip(option);

            control.choice = line->append(std::make_unique<MultistateSwitch>([this, index](const int value) {
                const Control &changed = _controls[index];
                const std::uint32_t kept = _held[changed.held].value & ~changed.option->mask;

                assign(changed.held, kept | static_cast<std::uint32_t>(value));
            }));

            std::vector<MultistateSwitch::Choice> choices;

            choices.reserve(option.choices.size());

            for (const Flags::Choice &choice : option.choices) {
                choices.push_back({.value = static_cast<int>(choice.value), .label = std::string(choice.label)});
            }

            control.choice->set_options(std::move(choices));
            control.choice->hint = caption->hint;
        }

        _controls.push_back(control);
    }

    _values = column->append(std::make_unique<Label>());
    _values->font(Typeface::mono, Theme::fontSmall)->tone(&Theme::Palette::muted)->wrap();

    _unknown = column->append(
            std::make_unique<Label>("This port gives no meaning to some bits set here. They are kept as they are."));
    _unknown->font(400, Theme::fontSmall)->tone(&Theme::Palette::faint)->wrap();

    const bool kept = std::ranges::any_of(
            _held, [set](const Held &each) { return (Flags::info(each.word).kept & Flags::bit(set)) != 0; });

    if (kept) {
        column->append(std::make_unique<Label>(
                               "The port keeps these words in its own settings. A word left empty runs as it ran "
                               "last time, and 0 clears it."))
                ->font(400, Theme::fontSmall)
                ->tone(&Theme::Palette::faint)
                ->wrap();
    }

    Box *buttons = column->append(Box::row());

    buttons->spacing(8.0);
    buttons->fixedHeight = Theme::control;

    buttons->append(std::make_unique<Button>("Clear all", [this] { clear(); }))
            ->kind(Button::Kind::Ghost)
            ->tooltip("Leave every word here to the port");

    buttons->append(std::make_unique<Spacer>());

    buttons->append(std::make_unique<Button>("Cancel", [this] {
        if (dismissed) {
            dismissed();
        }
    }));

    buttons->append(std::make_unique<Button>("Save", [this] { commit(); }))->kind(Button::Kind::Primary);

    for (size_t at = 0; at < _held.size(); ++at) {
        show(at);
    }

    describe();
}

void FlagsDialog::opened() {
    if (!_controls.empty() && root() != nullptr) {
        const Control &first = _controls.front();

        root()->focus(first.check != nullptr ? static_cast<Widget *>(first.check) : first.choice);
    }
}

void FlagsDialog::arrange(Typeface &type) {
    // Fitted to the boxes once, so a port with few of them gets a small card.
    if (tall <= 0.0) {
        tall = std::min(card()->natural_height(type, wanted), TALLEST);
    }

    Dialog::arrange(type);
}

void FlagsDialog::assign(const size_t held, const std::uint32_t value) {
    Held &changed = _held[held];

    changed.value = value;
    changed.touched = true;

    show(held);
    describe();
}

void FlagsDialog::show(const size_t held) const {
    const std::uint32_t value = _held[held].value;

    for (const Control &control : _controls) {
        if (control.held != held) {
            continue;
        }

        if (control.check != nullptr) {
            control.check->set_checked((value & control.option->mask) == control.option->mask);
        } else {
            control.choice->set_current(static_cast<int>(value & control.option->mask));
        }
    }
}

void FlagsDialog::clear() {
    for (size_t at = 0; at < _held.size(); ++at) {
        Held &each = _held[at];

        each.value = Flags::info(each.word).start;
        each.touched = false;
        each.cleared = true;

        show(at);
    }

    describe();
}

void FlagsDialog::describe() const {
    std::string said;
    bool unknown = false;

    for (const Held &each : _held) {
        const std::string_view name = Flags::info(each.word).name;
        const bool empty = each.cleared || each.typed.empty();

        if (!said.empty()) {
            said += "  ·  ";
        }

        said += name;

        if (each.touched) {
            said += " " + std::to_string(each.value);
        } else if (empty) {
            said += " not set";
        } else if (!Flags::parse(each.typed)) {
            said += " is not a number";
        } else {
            said += " " + each.typed;
        }

        unknown = unknown || (each.value & ~Flags::known(_set, each.word)) != 0;
    }

    // A label keeps its box when its text changes, and a longer one may need another line.
    if (_values->text().size() != said.size() && root() != nullptr) {
        root()->relayout();
    }

    _values->set_text(said);
    _unknown->set_visible(unknown);
}

void FlagsDialog::commit() const {
    Edited edited;

    for (const Held &each : _held) {
        if (each.touched) {
            edited.emplace_back(each.word, std::to_string(each.value));
        } else if (each.cleared) {
            edited.emplace_back(each.word, std::string());
        }
    }

    // Copied out: accepting takes the dialog down, and the callable with it.
    const auto fire = _accepted;
    const std::function<void()> shut = dismissed;

    if (shut) {
        shut();
    }

    if (fire) {
        fire(std::move(edited));
    }
}

}
