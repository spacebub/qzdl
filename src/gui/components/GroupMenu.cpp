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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ttk/draw/Glyphs.h"
#include "ttk/toolkit/Root.h"
#include "ttk/toolkit/overlays/Menu.h"

#include "gui/components/GroupMenu.h"

using namespace ttk;

namespace {

enum class GroupAction : std::uint8_t {
    Rename,
    MoveUp,
    MoveDown,
    Delete,
};

}

namespace components {

void showGroupMenu(Reach &reach,
                   const Widget &owner,
                   const GroupsBridge::List list,
                   const std::uint16_t id,
                   const std::string &name,
                   const BLRect &button,
                   std::function<void()> closed) {
    Root *root = owner.root();

    if (root == nullptr) {
        return;
    }

    const std::vector<Menu::Row> rows = {
            Menu::item(GroupAction::Rename, "Rename", Glyphs::Glyph::Edit),
            Menu::item(GroupAction::MoveUp, "Move up", Glyphs::Glyph::Up),
            Menu::item(GroupAction::MoveDown, "Move down", Glyphs::Glyph::Down),
            Menu::item(GroupAction::Delete, "Delete group", Glyphs::Glyph::Trash, true),
    };

    Widget *menu = root->layer(Root::POPUPS)
                           ->add(std::make_unique<Menu>(rows, [&reach, root, list, id, name](const int action) {
                               root->dismiss();

                               GroupsBridge &groups = reach.config.groups();

                               switch (static_cast<GroupAction>(action)) {
                                   case GroupAction::Rename:
                                       reach.prompt("Rename group",
                                                    "Name",
                                                    name,
                                                    "Rename",
                                                    [&groups, list, id](const std::string &named) {
                                                        groups.rename(list, id, named);
                                                    });
                                       break;
                                   case GroupAction::MoveUp:
                                       groups.move(list, id, -1);
                                       break;
                                   case GroupAction::MoveDown:
                                       groups.move(list, id, 1);
                                       break;
                                   case GroupAction::Delete:
                                       groups.remove(list, id);
                                       break;
                               }
                           }));

    const double tall = Menu::height_of(rows);
    const double below = button.y + button.h + 4.0;

    // Above the button when the window has no room under it.
    const double top = below + tall > root->content()->box().h ? button.y - tall - 4.0 : below;

    menu->place(BLRect{button.x + button.w - Menu::WIDTH, top, Menu::WIDTH, tall}, root->type());

    root->set_dismiss(
            [root, menu, gone = std::move(closed)] {
                const BLRect was = menu->box();

                root->layer(Root::POPUPS)->erase(menu);
                root->damage(was);
                gone();
            },
            &owner);

    menu->invalidate();
}

}
