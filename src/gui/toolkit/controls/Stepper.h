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
#include <string>

#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/layout/Box.h"

namespace toolkit {

// A number between two buttons. Stepping below the range clears it.
class Stepper : public Box {
public:
    Stepper(std::string label, std::function<void(int)> stepped);

    Stepper *range(int from, int to);
    Stepper *clearable(int offValue, std::string placeholder);
    Stepper *tooltip(std::string text);

    void setValue(int value);

    [[nodiscard]] int value() const { return _value; }

    void arrange(Typeface &type) override;

    double naturalWidth(Typeface &type) override;

    void paint(const Painter &painter) override;

private:
    void step(int by);

    [[nodiscard]] bool unset() const { return _clearable && _value < _from; }

    Label *_caption = nullptr;
    GlyphButton *_less = nullptr;
    GlyphButton *_more = nullptr;

    BLRect _frame{};

    int _from = 1;
    int _to = 9;
    int _value = 1;

    bool _clearable = false;
    int _off = 0;
    std::string _placeholder = "Off";

    std::function<void(int)> _stepped;
};

}
