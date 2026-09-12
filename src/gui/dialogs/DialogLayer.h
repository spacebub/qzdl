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

#include <functional>
#include <memory>

#include "gui/toolkit/overlays/Dialog.h"

namespace dialogs {

// Where a dialog goes up: under the title bar, stacked one over another.
//
// It holds no dialog of its own and knows of none by name. One shown over another
// hides it until it goes, so a dialog can open a second over itself and get it back.
class DialogLayer : public toolkit::Widget {
public:
    toolkit::Dialog *show(std::unique_ptr<toolkit::Dialog> dialog);

    // Takes the dialog down unless it says it dealt with the dismissal itself.
    void close();

    // Takes it down whatever it says, for a dialog whose reason for being up is gone.
    void dismiss();

    // Called with a dialog as it goes, for whoever was holding on to it.
    std::function<void(toolkit::Dialog *)> closed;

    // The one that is up, or nothing.
    [[nodiscard]] toolkit::Dialog *top() const;

    void sync() const;

    // True while one is up, which is what stops the window being dragged by it.
    [[nodiscard]] bool covered() const { return !children().empty(); }

    void arrange(Typeface &type) override;
};

}
