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
#include "gui/draw/Theme.h"
#include "gui/toolkit/Widget.h"

namespace toolkit {

// One of a short list, as a row of words in a trough.
class Segmented : public Widget {
public:
    struct Choice {
        std::string key;
        std::string label;
        bool badge = false;
    };

    explicit Segmented(std::function<void(const std::string &)> selected);

    void setOptions(std::vector<Choice> options);
    void setCurrent(std::string key);

    [[nodiscard]] const std::string &current() const { return _current; }

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
    std::string _current;

    std::function<void(const std::string &)> _selected;

    int _over = -1;

    Anim::Tween _markX;
    Anim::Tween _markWidth;
    bool _marked = false;
};

}
