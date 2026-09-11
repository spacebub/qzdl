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

#include "gui/toolkit/controls/Fact.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/layout/Box.h"

namespace toolkit {

Fact::Fact(std::string label, std::string value) : Box(Flow::Column) {
    spacing(3.0);

    _caption = append(std::make_unique<Label>(std::move(label)));
    _caption->section();

    _value = append(std::make_unique<Label>(std::move(value)));
    _value->font(600, Theme::fontBody)->tone(Theme::of().text);
}

void Fact::setValue(std::string value) {
    _value->setText(std::move(value));
}

Fact *Fact::path(const bool value) {
    _value->path(value);

    return this;
}

Fact *Fact::onClick(std::string tip, std::function<void()> clicked) {
    _value->onClick(std::move(clicked));
    _value->hint = std::move(tip);

    return this;
}

}
