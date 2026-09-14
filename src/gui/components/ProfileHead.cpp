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
#include <cstdint>
#include <string>
#include <vector>

#include "gui/components/ProfileHead.h"
#include "gui/components/Tones.h"
#include "gui/draw/Glyphs.h"
#include "gui/draw/Theme.h"
#include "gui/model/ProfileBridge.h"
#include "gui/services/Filters.h"
#include "gui/state/State.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/overlays/Menu.h"

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

}

namespace components {

using namespace toolkit;

ProfileHead::ProfileHead(Reach *reach) : Box(Flow::Row), _reach(reach) {
    fixedHeight = 74.0;
    spacing(14.0)->pad(Theme::bleed, 0.0, Theme::bleed, 0.0)->cross(Place::Centre);

    _chooser = append(std::make_unique<ProfileChooser>(reach));
    _chooser->stretch = 1.0;
    _chooser->fixedHeight = 74.0;

    _terminal = append(std::make_unique<GlyphButton>(Glyphs::Glyph::Terminal, [this] {
        _reach->runs.show(State::get().cfg.profileKey);
    }));

    _terminal->size(Theme::control)->outlined();
    _terminal->fixedWidth = Theme::control;
    _terminal->fixedHeight = Theme::control;

    _cog = append(std::make_unique<GlyphButton>(Glyphs::Glyph::Cog, [this] { showMenu(); }));
    _cog->size(Theme::control)->outlined()->spin(90.0);
    _cog->fixedWidth = Theme::control;
    _cog->fixedHeight = Theme::control;

    _launch = append(std::make_unique<Button>("Launch", [this] {
        _reach->config.profile().launch();
    }));

    _launch->kind(Button::Kind::Primary)->glyph(Glyphs::Glyph::Play);
}

std::string ProfileHead::summary() {
    const State::Cfg &cfg = State::get().cfg;

    if (cfg.commandOverride) {
        return cfg.commandTrouble.empty() ? "Launches the command written on this page"
                                          : cfg.commandTrouble;
    }

    if (!ProfileBridge::launchable()) {
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

void ProfileHead::sync() const {
    const State::Cfg &cfg = State::get().cfg;
    const std::vector<std::string> &logged = State::get().runs.logged;

    _chooser->setSaid(summary(), ProfileBridge::launchable());
    _chooser->setStatus(statusOf(_reach->runs.stateOf(cfg.profileKey)),
                        _reach->runs.reasonOf(cfg.profileKey));

    _terminal->setVisible(cfg.captureOutput && !cfg.dosPort && !cfg.autoClose);
    _terminal->setEnabled(std::ranges::find(logged, cfg.profileKey) != logged.end());
    _terminal->tooltip(_terminal->enabled() ? "Show what this profile printed"
                                            : "Nothing has been launched from this profile yet");

    _launch->setEnabled(ProfileBridge::launchable());
}

void ProfileHead::showMenu() const {
    if (root() == nullptr) {
        return;
    }

    const State::Cfg &cfg = State::get().cfg;
    const bool busy = _reach->runs.alive(cfg.profileKey);

    const std::vector<Menu::Row> rows = {
        Menu::item(ProfileMenuAction::Rename, "Rename", Glyphs::Glyph::Edit, false, busy),
        Menu::item(ProfileMenuAction::Duplicate, "Duplicate", Glyphs::Glyph::Extract),
        Menu::item(ProfileMenuAction::Clear, "Reset", Glyphs::Glyph::Refresh),
        Menu::rule(),
        Menu::item(ProfileMenuAction::CopyConfig, "Copy port config", Glyphs::Glyph::Copy,
                   false, cfg.port.empty() || busy),
        Menu::rule(),
        Menu::item(ProfileMenuAction::LoadZdl, "Import .zdl", Glyphs::Glyph::Download),
        Menu::item(ProfileMenuAction::SaveZdl, "Save as .zdl", Glyphs::Glyph::Save),
        Menu::rule(),
        Menu::item(ProfileMenuAction::Delete, "Delete", Glyphs::Glyph::Trash, true, busy),
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
                                ProfileBridge::removalNote(), "Delete", true, [this] {
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

    Widget const *held = menu;

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
