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
#pragma once

#include <memory>
#include <string>

#include "gui/bridge/Bridge.h"

// The shelf: filtered profiles and games, and launching a game on its own.
class LibraryBridge : public Bridge {
public:
    using Bridge::Bridge;

    void bind();

    void pushShelf();

    // Bumps the key the library's play hints hang off, only when their inputs changed.
    void pushGameRev();

private:
    // Updated in place; see Models::reconcile.
    std::shared_ptr<slint::VectorModel<ui::ProfileCard>> _shelfProfiles
        = std::make_shared<slint::VectorModel<ui::ProfileCard>>();
    std::shared_ptr<slint::VectorModel<ui::NameRow>> _shelfGames
        = std::make_shared<slint::VectorModel<ui::NameRow>>();

    std::string _filter;
    std::string _gameMark;
    int _gameRev{0};
};
