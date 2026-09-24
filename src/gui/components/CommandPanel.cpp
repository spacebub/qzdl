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
#include <string>
#include <utility>

#include "ttk/draw/Glyphs.h"
#include "ttk/draw/Theme.h"
#include "ttk/draw/Typeface.h"
#include "ttk/toolkit/layout/Box.h"
#include "ttk/toolkit/layout/Spacer.h"
#include "ttk/util/Clipboard.h"

#include "core/launch/Dos.h"
#include "gui/components/CommandPanel.h"
#include "gui/components/Parts.h"
#include "gui/model/ProfileBridge.h"
#include "gui/state/State.h"

using namespace ttk;

namespace {

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

// A panel no taller than what is in it, up to a ceiling: the command line is
// usually one line and a box four deep leaves a hole under it.
class Hug : public ttk::Panel {
public:
    explicit Hug(const double most) : _most(most) {}

    double natural_height(Typeface &type, const double width) override {
        return std::min(_most, Panel::natural_height(type, width));
    }

private:
    double _most;
};

}

namespace components {

CommandPanel::CommandPanel(Reach *reach) : _reach(reach) {
    Box *line = append(Box::column());

    line->pad(16.0)->spacing(12.0);

    Box *head = line->append(Box::row());

    head->spacing(12.0)->cross(Box::Place::Centre);
    head->fixedHeight = Theme::control;

    panelTitle(head, "Command line");

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
    _resolved->font(Typeface::mono, Theme::fontSmall)->tone(&Theme::Palette::muted)->wrap();
    _resolved->on_click([this] {
        _reach->showCommand();

        _reach->touch();
    });

    _copy = inside->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Extract, [this] {
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

    _extra->set_visible(!cfg.commandOverride);
    _command->set_visible(cfg.commandOverride);
    _tokens->set_visible(cfg.commandOverride);

    if (!cfg.commandOverride && _extra->text() != cfg.extra) {
        _extra->set_text(cfg.extra);
    }

    if (cfg.commandOverride && _command->text() != cfg.command) {
        _command->set_text(cfg.command);
    }

    _resolved->set_text(!cfg.commandTrouble.empty()  ? cfg.commandTrouble
                       : !cfg.commandLine.empty()   ? cfg.commandLine
                       : cfg.dosPort && cfg.dosbox.empty() && cfg.systemDosbox.empty()
                           ? "A DOS source port, and no DOSBox to run it in. Set one in "
                             "Settings."
                           : "Nothing to run yet.");

    _resolved->tone(!cfg.commandTrouble.empty()  ? &Theme::Palette::danger
                    : ProfileBridge::launchable() ? &Theme::Palette::muted
                                                  : &Theme::Palette::faint);

    _copy->set_enabled(!cfg.commandLine.empty());

    // Only a launch that runs DOSBox spends anything, generated or typed.
    _budget->set_visible(cfg.dosCommands > 0);
    _budget->set_tight(cfg.dosCommands >= Dos::COMMANDS);
    _budget->set_text(std::to_string(cfg.dosCommands) + " / " + std::to_string(Dos::COMMANDS)
                     + " DOSBox commands");
}

}
