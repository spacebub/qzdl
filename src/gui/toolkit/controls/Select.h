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
#include <vector>

#include "gui/draw/Anim.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/layout/Box.h"

namespace toolkit {

// One of a list, chosen from a panel that drops out of the box.
//
// The list lives on the root's popup layer while it is open rather than under the
// box, so nothing it covers has to know about it.
class Select : public Box {
public:
    Select(std::string label, std::function<void(int)> selected);

    void setOptions(std::vector<std::string> options);

    // Per option; short of the list or empty is none.
    void setBadges(std::vector<std::string> badges);

    void setCurrent(int index);

    [[nodiscard]] int current() const { return _current; }

    Select *placeholder(std::string text);

    // Offers the placeholder as a row of its own, which clears the value.
    Select *clearable(bool value = true);

    Select *tip(std::string text);

    [[nodiscard]] bool open() const { return _list != nullptr; }

    void close();

    void arrange(Typeface &type) override;

    double naturalWidth(Typeface &type) override;

    void paint(const Painter &painter) override;

    bool press(const Pointer &at) override;
    void release(const Pointer &at) override;
    void enter() override;
    void leave() override;

    [[nodiscard]] bool takesFocus() const override { return enabled(); }
    bool key(const Key &pressed) override;

    bool advance(double now) override;

    Widget *at(double x, double y) override;

private:
    void show();

    Label *_caption = nullptr;

    // The box, worked out at arrange time.
    BLRect _frame{};

    std::vector<std::string> _options;
    std::vector<std::string> _badges;

    std::string _placeholder = "(Default)";

    int _current = -1;
    bool _clearable = false;

    std::function<void(int)> _selected;

    // Owned by the popup layer while it is up.
    Widget *_list = nullptr;

    Anim::Tween _lit;
    Anim::Tween _turn;
};

}
