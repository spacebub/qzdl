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

#include <string>
#include <utility>
#include <vector>

#include "ttk/draw/Glyphs.h"
#include "ttk/draw/Theme.h"
#include "ttk/draw/Typeface.h"
#include "ttk/toolkit/Root.h"
#include "ttk/toolkit/layout/Box.h"
#include "ttk/toolkit/layout/Spacer.h"
#include "ttk/util/Clipboard.h"

#include "core/launch/Command.h"
#include "core/launch/Dos.h"
#include "gui/components/CommandPanel.h"
#include "gui/components/Parts.h"
#include "gui/components/TokenChips.h"
#include "gui/model/ProfileBridge.h"
#include "gui/state/State.h"

using namespace ttk;

namespace components {

CommandPanel::CommandPanel(Reach *reach) : _reach(reach) {
    Box *line = append(Box::column());

    line->pad(16.0)->spacing(12.0);

    Box *head = line->append(Box::row());

    head->spacing(12.0)->cross(Box::Place::Centre);
    head->fixedHeight = Theme::control;

    panelTitle(head, "Command line");

    _budget = head->append(std::make_unique<Chip>("",
                                                  "DOSBox runs only the first eleven "
                                                  "commands it is given with -c and silently drops the rest."));

    _budget->plain();

    head->append(std::make_unique<Spacer>());

    _environment = head->append(std::make_unique<Label>("Environment"));
    _environment->tone(&Theme::Palette::muted);
    _environment->hint = "Set variables for every launch of this profile";
    _environment->on_click([this] {
        _reach->editEnvironment(State::get().cfg.environment, [this](std::vector<EnvVariable> edited) {
            _reach->config.profile().setEnvironment(std::move(edited));
        });
    });

    _override = head->append(std::make_unique<Toggle>(
            "Override", [this](const bool on) { _reach->config.profile().setCommandOverride(on); }));

    _override->hint = "Use a custom command instead of the generated one";

    _arguments = line->append(std::make_unique<ArgumentsField>(
            "Extra arguments",
            [this](const std::string &value) { _reach->config.profile().setPrefix(value); },
            [this](const std::string &value) { _reach->config.profile().setExtra(value); }));

    _command = line->append(std::make_unique<Field>(
            "The command", [this](const std::string &value) { _reach->config.profile().setCommand(value); }));

    _command->placeholder(Command::example())->mono();
    _command->icon(Glyphs::Glyph::Edit, "Edit", [this] { _reach->editCommand(); });

    _tokens = line->append(std::make_unique<TokenChips>());

    Panel *resolved = line->append(std::make_unique<Panel>());

    resolved->inset = true;
    resolved->hoverable = true;

    Box *inside = resolved->append(Box::row());

    inside->pad(12.0)->spacing(10.0)->cross(Box::Place::Start);

    _resolved = inside->append(std::make_unique<Label>());
    _resolved->stretch = 1.0;
    _resolved->font(Typeface::mono, Theme::fontSmall)->tone(&Theme::Palette::muted)->elide(4);
    _resolved->on_click([this] {
        _reach->showCommand();

        _reach->touch();
    });

    _copy = inside->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Copy, [this] {
        Clipboard::write(State::get().cfg.commandLine);
        _reach->notify.success("The command line is on the clipboard.");
    }));

    _copy->size(26.0)->tooltip("Copy");
    _copy->fixedWidth = 26.0;
    _copy->fixedHeight = 26.0;
}

void CommandPanel::sync() const {
    const State::Cfg &cfg = State::get().cfg;

    _override->set_checked(cfg.commandOverride);

    const std::string environment = cfg.environment.empty()
                                            ? std::string("Environment")
                                            : "Environment (" + std::to_string(cfg.environment.size()) + ")";

    // A label keeps its box when its text changes, and the header sizes this one by its text.
    if (_environment->text() != environment) {
        _environment->set_text(environment);

        if (_environment->root() != nullptr) {
            _environment->root()->relayout();
        }
    }

    _arguments->set_visible(!cfg.commandOverride);
    _command->set_visible(cfg.commandOverride);
    _tokens->set_visible(cfg.commandOverride);

    if (!cfg.commandOverride) {
        _arguments->set(cfg.prefix, cfg.extra);
    }

    if (cfg.commandOverride && _command->text() != cfg.command) {
        _command->set_text(cfg.command);
    }

    // clang-format off
    _resolved->set_text(!cfg.commandTrouble.empty()  ? cfg.commandTrouble
                       : !cfg.commandLine.empty()   ? cfg.commandLine
                       : cfg.dosPort && cfg.dosbox.empty() && cfg.systemDosbox.empty()
                           ? "A DOS source port, and no DOSBox to run it in. Set one in "
                             "Settings."
                           : "Nothing to run yet.");
    // clang-format on

    // clang-format off
    _resolved->tone(!cfg.commandTrouble.empty()  ? &Theme::Palette::danger
                    : ProfileBridge::launchable() ? &Theme::Palette::muted
                                                  : &Theme::Palette::faint);
    // clang-format on

    _copy->set_enabled(!cfg.commandLine.empty());

    // Only a launch that runs DOSBox spends anything, generated or typed.
    _budget->set_visible(cfg.dosCommands > 0);
    _budget->set_tight(cfg.dosCommands >= Dos::COMMANDS);
    _budget->set_text(std::to_string(cfg.dosCommands) + " / " + std::to_string(Dos::COMMANDS) + " DOSBox commands");
}

}
