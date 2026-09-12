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

#include "gui/dialogs/DialogLayer.h"
#include "gui/draw/Theme.h"
#include "gui/toolkit/Root.h"

namespace dialogs {

using namespace toolkit;

Dialog *DialogLayer::show(std::unique_ptr<Dialog> dialog) {
    close();

    Dialog *raw = dialog.get();

    raw->dismissed = [this] { close(); };

    add(std::move(dialog));

    if (root() != nullptr) {
        root()->relayout();
    }

    raw->opened();

    return raw;
}

Dialog *DialogLayer::top() const {
    return children().empty() ? nullptr : static_cast<Dialog *>(children().back().get());
}

void DialogLayer::sync() const {
    if (Dialog *up = top(); up != nullptr) {
        up->sync();
    }
}

void DialogLayer::close() {
    Dialog *up = top();

    if (up == nullptr || !up->closing()) {
        return;
    }

    dismiss();
}

void DialogLayer::dismiss() {
    Dialog *up = top();

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

void DialogLayer::arrange(Typeface &type) {
    // Never over the title bar: the window stays draggable by it.
    const BLRect under{_box.x, _box.y + Theme::barHeight, _box.w,
                       std::max(0.0, _box.h - Theme::barHeight)};

    for (const Ptr &child : children()) {
        child->place(under, type);
    }
}

}
