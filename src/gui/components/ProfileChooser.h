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

#include <string>

#include <blend2d/blend2d.h>

#include "gui/components/Reach.h"
#include "gui/toolkit/controls/StatusIndicator.h"

namespace components {

// The profile at the top of the page, and the list it is picked from.
class ProfileChooser : public toolkit::Widget {
public:
    explicit ProfileChooser(Reach *reach);

    void setSaid(std::string said, bool ready);

    void setStatus(toolkit::StatusIndicator::Status status, std::string reason);

    void arrange(Typeface &type) override;

    void paint(const toolkit::Painter &painter) override;

    // The pill inside is its own target, so the head keeps its wash while it is hovered.
    void within(bool inside) override;

    bool press(const toolkit::Pointer &at) override;
    void release(const toolkit::Pointer &at) override;

private:
    void show();

    // The title screen, scaled and with its corners off, kept as a sprite.
    void cutThumb();

    // How far the pill sits off the right edge.
    static constexpr double MARGIN = 10.0;

    static constexpr int THUMB_WIDE = 116;
    static constexpr int THUMB_TALL = 66;

    Reach *_reach;

    toolkit::StatusIndicator *_status = nullptr;

    std::string _said;
    std::string _name;
    std::string _artKey;
    int _artRev = -1;
    bool _ready = true;
    bool _open = false;

    BLImage _shot;
    BLImage _thumb;

    Widget *_list = nullptr;
};

}
