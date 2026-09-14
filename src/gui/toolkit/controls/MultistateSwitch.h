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
#include <optional>
#include <string>
#include <vector>

#include "gui/draw/Anim.h"
#include "gui/draw/Theme.h"
#include "gui/toolkit/Widget.h"

namespace toolkit {

// One of a short list, as a row of words in a trough.
class MultistateSwitch : public Widget {
public:
    // `value` is the page's own enum, cast to an int: a word spelled three times
    // over -- here, in the handler and in the table that reads it back -- compiles
    // just as well when one of the three is wrong.
    struct Choice {
        int value = 0;
        std::string label;
        bool badge = false;

        bool operator==(const Choice &other) const = default;
    };

    explicit MultistateSwitch(std::function<void(int)> selected);

    void setOptions(std::vector<Choice> options);
    void setCurrent(int value);

    [[nodiscard]] std::optional<int> current() const { return _current; }

    double naturalWidth(Typeface &type) override;
    double naturalHeight(Typeface & /*type*/, double /*width*/) override { return Theme::control; }

    void arrange(Typeface &type) override;

    void paint(const Painter &painter) override;

    bool press(const Pointer &at) override;
    void release(const Pointer &at) override;
    void hover(const Pointer &at) override;
    void leave() override;

    bool advance(double now) override;

private:
    // The box each word sits in, worked out at paint and layout time alike.
    std::vector<BLRect> lanes(Typeface &type) const;

    std::vector<Choice> _options;
    std::optional<int> _current;

    std::function<void(int)> _selected;

    int _over = -1;

    Anim::Tween _markX;
    Anim::Tween _markWidth;
    bool _marked = false;
};

}
