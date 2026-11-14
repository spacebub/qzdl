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
#include <utility>

#include "ttk/draw/Glyphs.h"
#include "ttk/draw/Theme.h"
#include "ttk/system/Text.h"
#include "ttk/toolkit/Root.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/layout/Spacer.h"

#include "gui/dialogs/EnvironmentDialog.h"

using namespace ttk;

namespace dialogs {

namespace {

constexpr double GAP = 8.0;
constexpr size_t SHOWN = 6;

bool settable(const std::string &name) {
    const std::string trimmed = Text::trim(name);

    return trimmed.empty() || trimmed.find_first_of("= \t") == std::string::npos;
}

}

EnvironmentDialog::EnvironmentDialog(std::vector<EnvVariable> variables,
                                     std::function<void(std::vector<EnvVariable>)> accepted)
    : _variables(std::move(variables)), _accepted(std::move(accepted)) {
    wanted = 600.0;

    Box *column = card()->append(Box::column());

    column->pad(22.0)->spacing(16.0);

    heading(column, "Environment variables");
    body(column,
         "Every launch of this profile runs with these set. A name the system already has takes the value here.");

    _scroll = column->append(std::make_unique<Scroll>());
    _rows = static_cast<Box *>(_scroll->hold(Box::column()));
    _rows->spacing(GAP);

    _empty = column->append(std::make_unique<Label>("No variables yet."));
    _empty->tone(&Theme::Palette::faint);

    _problem = column->append(std::make_unique<Label>("A name cannot hold a space or an equals sign."));
    _problem->tone(&Theme::Palette::danger);

    Box *row = column->append(Box::row());

    row->spacing(8.0);
    row->fixedHeight = Theme::control;

    row->append(std::make_unique<Button>("Add variable", [this] {
           _variables.emplace_back();

           rebuild();

           _names.back()->take_focus();
           _revealLast = true;
       }))->glyph(Glyphs::Glyph::Plus);

    row->append(std::make_unique<Spacer>());

    row->append(std::make_unique<Button>("Cancel", [this] {
        if (dismissed) {
            dismissed();
        }
    }));

    _accept = row->append(std::make_unique<Button>("Save", [this] { commit(); }));
    _accept->kind(Button::Kind::Primary);

    rebuild();
}

bool EnvironmentDialog::advance(const double now) {
    if (_drop) {
        _variables.erase(_variables.begin() + static_cast<std::ptrdiff_t>(*_drop));
        _drop.reset();

        rebuild();
    }

    // The base class runs the animation that grows the card into place.
    return Dialog::advance(now);
}

void EnvironmentDialog::arrange(Typeface &type) {
    Dialog::arrange(type);

    // The scroller learns how far the new row reaches only once it is laid out.
    if (_revealLast) {
        _revealLast = false;
        _scroll->scroll_to(_scroll->reach());
    }
}

void EnvironmentDialog::opened() {
    if (!_names.empty()) {
        _names.front()->take_focus();
    }
}

void EnvironmentDialog::rebuild() {
    _rows->clear();
    _names.clear();

    for (size_t index = 0; index < _variables.size(); ++index) {
        Box *row = _rows->append(Box::row());

        row->spacing(GAP)->cross(Box::Place::Centre);
        row->fixedHeight = Theme::control;

        Field *name = row->append(std::make_unique<Field>("", [this, index](const std::string &value) {
            _variables[index].name = value;

            check();
        }));

        name->placeholder("NAME")->mono();
        name->minWidth = 0.0;
        name->fixedWidth = 200.0;
        name->set_text(_variables[index].name);

        Field *value = row->append(std::make_unique<Field>(
                "", [this, index](const std::string &text) { _variables[index].value = text; }));

        value->placeholder("Value")->mono();
        value->minWidth = 0.0;
        value->stretch = 1.0;
        value->set_text(_variables[index].value);
        value->accepted = [this] { commit(); };

        // Dropped on the next frame: rebuilding now would destroy the button inside its own click.
        row->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Trash,
                                                  [this, index] {
                                                      _drop = index;
                                                      wake();
                                                  }))
                ->size(26.0)
                ->tooltip("Remove");

        _names.push_back(name);
    }

    const size_t shown = std::min(_variables.size(), SHOWN);

    _scroll->set_visible(!_variables.empty());
    _scroll->fixedHeight = shown == 0 ? 0.0 : (static_cast<double>(shown) * (Theme::control + GAP)) - GAP;

    // Past the cap the bar takes a lane at the right, which the rows keep clear of.
    _rows->pad(0.0, 0.0, _variables.size() > SHOWN ? Theme::lane + 4.0 : 0.0, 0.0);

    _empty->set_visible(_variables.empty());

    check();

    // Rows added under a scroller ask for no layout of their own.
    if (root() != nullptr) {
        root()->relayout();
    }
}

void EnvironmentDialog::check() const {
    const bool fine = std::ranges::all_of(_variables, [](const EnvVariable &each) { return settable(each.name); });

    _problem->set_visible(!fine);
    _accept->set_enabled(fine);
}

void EnvironmentDialog::commit() const {
    std::vector<EnvVariable> kept;

    for (const EnvVariable &each : _variables) {
        if (!settable(each.name)) {
            return;
        }

        std::string name = Text::trim(each.name);

        if (name.empty()) {
            continue;
        }

        // The last one wins, as it would in a shell.
        std::erase_if(kept, [&name](const EnvVariable &earlier) { return earlier.name == name; });
        kept.push_back(EnvVariable{.name = std::move(name), .value = each.value});
    }

    // Copied out: accepting takes the dialog down, and the callable with it.
    const auto fire = _accepted;
    const std::function<void()> shut = dismissed;

    if (shut) {
        shut();
    }

    if (fire) {
        fire(std::move(kept));
    }
}

}
