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

#include "gui/draw/Theme.h"
#include "gui/toolkit/Widget.h"

namespace toolkit {

// A small box of type that explains itself on hover and does nothing else: the
// tokens a custom command is written with, or a count beside a panel's title.
class Chip : public Widget {
public:
    Chip(std::string text, std::string about);

    void setText(std::string text);

    // Drawn in the warning tone, for a budget with nothing left in it.
    void setTight(bool value);

    // The proportional face, rather than the fixed width one a token wants.
    Chip *plain();

    double naturalWidth(Typeface &type) override;
    double naturalHeight(Typeface &type, double width) override;

    void paint(const Painter &painter) override;

private:
    [[nodiscard]] const BLFont &face(Typeface &type) const;

    std::string _text;

    bool _mono = true;
    bool _tight = false;
};

}
