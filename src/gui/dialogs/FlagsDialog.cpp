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
#include <utility>

#include "ttk/draw/Theme.h"
#include "ttk/draw/Typeface.h"
#include "ttk/toolkit/Root.h"
#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/layout/Scroll.h"
#include "ttk/toolkit/layout/Spacer.h"

#include "gui/dialogs/FlagsDialog.h"

using namespace ttk;

namespace dialogs {

namespace {

constexpr double COLUMN = 392.0;
constexpr double GAP = 16.0;

// A choice takes a row of its own, as its switch needs more than a column.
constexpr double ROW = (COLUMN * 2.0) + GAP;
constexpr double CAPTION = 150.0;

// Leaves the page in view behind a port with many options.
constexpr double TALLEST = 720.0;

std::string tip(const Flags::Option &option) {
    const std::string_view word = Flags::name(option.word);
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
                         const PortLine line,
                         const std::array<std::string, Flags::WORDS> &typed,
                         std::function<void(Flags::Edits)> accepted)
    : _line(line), _accepted(std::move(accepted)) {
    wanted = 900.0;

    hold(family, typed);

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

    addOptions(groups);
    addFooter(column);

    for (size_t at = 0; at < _held.size(); ++at) {
        show(at);
    }

    describe();
}

void FlagsDialog::hold(const Flags::Family family, const std::array<std::string, Flags::WORDS> &typed) {
    for (const Flags::Word word : Flags::ALL) {
        const Flags::WordInfo &info = Flags::info(word);

        if (info.family == family && Flags::reads(_line, word)) {
            const std::string &text = typed[Flags::index(word)];

            _held.push_back(Held{
                    .typed = text,
                    .value = Flags::parse(text).value_or(info.start),
                    .word = word,
                    .edit = Edit::none,
            });
        }
    }
}

void FlagsDialog::addOptions(Box *groups) {
    std::optional<Flags::Group> current;
    Wrap *rows = nullptr;

    for (const Flags::Option &option : Flags::options()) {
        const auto held = std::ranges::find(_held, option.word, &Held::word);

        if (held == _held.end() || !has(option.lines, _line)) {
            continue;
        }

        if (option.group != current) {
            current = option.group;

            Box *group = groups->append(Box::column());

            group->spacing(8.0);
            group->append(std::make_unique<Label>(std::string(Flags::title(option.group))))->section();

            rows = group->append(std::make_unique<Wrap>());
            rows->spacing(GAP, 6.0);
        }

        const auto at = static_cast<size_t>(held - _held.begin());

        if (option.choices.empty()) {
            addCheck(rows, option, at);
        } else {
            addChoice(rows, option, at);
        }
    }
}

void FlagsDialog::addCheck(Wrap *rows, const Flags::Option &option, const size_t held) {
    const size_t index = _controls.size();
    Control &control = _controls.emplace_back(Control{.option = &option, .held = held});

    control.check = rows->append(std::make_unique<Check>(std::string(option.label), [this, index](const bool on) {
        const Control &changed = _controls[index];
        const std::uint32_t value = _held[changed.held].value;

        assign(changed.held, on ? value | changed.option->mask : value & ~changed.option->mask);
    }));

    control.check->fixedWidth = COLUMN;
    control.check->hint = tip(option);
}

void FlagsDialog::addChoice(Wrap *rows, const Flags::Option &option, const size_t held) {
    const size_t index = _controls.size();
    Control &control = _controls.emplace_back(Control{.option = &option, .held = held});

    Box *row = rows->append(Box::row());

    row->spacing(8.0)->cross(Box::Place::Centre);
    row->fixedWidth = ROW;
    row->fixedHeight = Theme::control;

    Label *caption = row->append(std::make_unique<Label>(std::string(option.label)));

    caption->fixedWidth = CAPTION;
    caption->hint = tip(option);

    control.choice = row->append(std::make_unique<MultistateSwitch>([this, index](const int value) {
        const Control &changed = _controls[index];
        const std::uint32_t rest = _held[changed.held].value & ~changed.option->mask;

        assign(changed.held, rest | static_cast<std::uint32_t>(value));
    }));

    std::vector<MultistateSwitch::Choice> choices;

    choices.reserve(option.choices.size());

    for (const Flags::Choice &choice : option.choices) {
        choices.push_back({.value = static_cast<int>(choice.value), .label = std::string(choice.label)});
    }

    control.choice->set_options(std::move(choices));
    control.choice->hint = caption->hint;
}

void FlagsDialog::addFooter(Box *column) {
    _values = column->append(std::make_unique<Label>());
    _values->font(Typeface::mono, Theme::fontSmall)->tone(&Theme::Palette::muted)->wrap();

    _unknown = column->append(
            std::make_unique<Label>("This port gives no meaning to some bits set here. They are kept as they are."));
    _unknown->font(400, Theme::fontSmall)->tone(&Theme::Palette::faint)->wrap();

    if (std::ranges::any_of(_held, [this](const Held &each) { return has(Flags::info(each.word).kept, _line); })) {
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
    _held[held].value = value;
    _held[held].edit = Edit::number;

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
        _held[at].value = Flags::info(_held[at].word).start;
        _held[at].edit = Edit::cleared;

        show(at);
    }

    describe();
}

void FlagsDialog::describe() const {
    std::string said;
    bool unknown = false;

    for (const Held &each : _held) {
        if (!said.empty()) {
            said += "  ·  ";
        }

        said += Flags::name(each.word);

        if (each.edit == Edit::number) {
            said += " " + std::to_string(each.value);
        } else if (each.edit == Edit::cleared || each.typed.empty()) {
            said += " not set";
        } else if (!Flags::parse(each.typed)) {
            said += " is not a number";
        } else {
            said += " " + each.typed;
        }

        unknown = unknown || (each.value & ~Flags::known(_line, each.word)) != 0;
    }

    // A label keeps its box when its text changes, and a longer one may need another line.
    if (_values->text().size() != said.size() && root() != nullptr) {
        root()->relayout();
    }

    _values->set_text(said);
    _unknown->set_visible(unknown);
}

void FlagsDialog::commit() const {
    Flags::Edits edits;

    for (const Held &each : _held) {
        if (each.edit == Edit::number) {
            edits.emplace_back(each.word, std::to_string(each.value));
        } else if (each.edit == Edit::cleared) {
            edits.emplace_back(each.word, std::string());
        }
    }

    // Copied out: accepting takes the dialog down, and the callable with it.
    const auto fire = _accepted;
    const std::function<void()> shut = dismissed;

    if (shut) {
        shut();
    }

    if (fire) {
        fire(std::move(edits));
    }
}

}
