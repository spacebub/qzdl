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

#include "gui/components/sheets/SheetLayer.h"
#include "gui/draw/Theme.h"
#include "gui/toolkit/Root.h"

namespace components {

using namespace toolkit;

Sheet *SheetLayer::show(std::unique_ptr<Sheet> sheet) {
    close();

    Sheet *raw = sheet.get();

    raw->dismissed = [this] { close(); };

    add(std::move(sheet));

    if (root() != nullptr) {
        root()->relayout();
    }

    raw->opened();

    return raw;
}

Sheet *SheetLayer::top() const {
    return children().empty() ? nullptr : static_cast<Sheet *>(children().back().get());
}

void SheetLayer::sync() const {
    if (Sheet *up = top(); up != nullptr) {
        up->sync();
    }
}

void SheetLayer::close() {
    Sheet *up = top();

    if (up == nullptr || !up->closing()) {
        return;
    }

    dismiss();
}

void SheetLayer::dismiss() {
    Sheet *up = top();

    if (up == nullptr) {
        return;
    }

    const BLRect was = up->box();

    if (closed) {
        closed(up);
    }

    erase(up);

    if (root() != nullptr) {
        root()->damage(was);
    }
}

void SheetLayer::arrange(Typeface &type) {
    // Never over the title bar: the window stays draggable by it.
    const BLRect under{_box.x, _box.y + Theme::barHeight, _box.w,
                       std::max(0.0, _box.h - Theme::barHeight)};

    for (const Ptr &child : children()) {
        child->place(under, type);
    }
}

}
