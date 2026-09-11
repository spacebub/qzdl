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

#include "gui/components/Frame.h"
#include "gui/components/LogDock.h"
#include "gui/components/TitleBar.h"
#include "gui/draw/Theme.h"

namespace components {

using namespace toolkit;

void Frame::arrange(Typeface &type) {
    const double width = _box.w;
    const double height = _box.h;

    _bar->place(BLRect{_box.x, _box.y, width, Theme::barHeight}, type);

    const double dock = LogDock::wanted();
    const double page = std::min(width - (Theme::pageMargin * 2.0), Theme::pageWidth);
    const double top = _box.y + Theme::barHeight + Theme::pageTop;
    const double room = _box.y + height - top - (dock > 0.0 ? dock + 20.0 : Theme::pageMargin);

    _pages->place(BLRect{_box.x + ((width - page) / 2.0), top, page, std::max(0.0, room)}, type);

    _dock->place(BLRect{_box.x + ((width - page) / 2.0), _box.y + height - dock - 12.0, page, dock},
                 type);
}

}
