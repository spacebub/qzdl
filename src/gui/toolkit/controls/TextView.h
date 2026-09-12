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

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "gui/draw/Theme.h"
#include "gui/draw/Typeface.h"
#include "gui/toolkit/Widget.h"

namespace toolkit {

// Read-only type the pointer can select and copy: the output of a run, and the
// command a launch would use. Held by a Scroll, which clips and offsets it.
class TextView : public Widget {
public:
    TextView();

    // Lines already broken, as a run's output arrives.
    void setRows(std::vector<std::string> rows);

    // One run, folded to the width it is given.
    void setRun(std::string run);

    TextView *face(int weight, float size);

    // What each row is drawn in, asked at paint so a change of theme is picked up.
    TextView *ink(std::function<BLRgba32(size_t row)> pick);

    [[nodiscard]] size_t rows() const { return _rows.size(); }

    void selectAll();
    void clearSelection();

    // What is selected, or nothing at all when no two spots differ.
    [[nodiscard]] std::string selection() const;

    double naturalHeight(Typeface &type, double width) override;

    void arrange(Typeface &type) override;

    void paint(const Painter &painter) override;

    bool press(const Pointer &at) override;
    void drag(const Pointer &at) override;

    bool key(const Key &pressed) override;

    [[nodiscard]] bool takesFocus() const override { return true; }

    void lostFocus() override;

private:
    // A place in the text: which row, and how far into it in bytes.
    struct Spot {
        size_t row = 0;
        size_t at = 0;

        auto operator<=>(const Spot &) const = default;
        bool operator==(const Spot &) const = default;
    };

    [[nodiscard]] const BLFont &font(Typeface &type) const;

    void refold(Typeface &type, double width);

    [[nodiscard]] Spot spotAt(Typeface &type, double x, double y) const;

    // The two spots in reading order.
    void span(Spot &from, Spot &to) const;

    // Holds a selection inside rows that have since changed under it.
    void clamp();

    void moveTo(const Spot &where, bool selecting);

    std::vector<std::string> _rows;

    // Where each row sits in `_run`, so a part of one reads back off the run.
    std::vector<std::pair<size_t, size_t>> _spans;

    std::string _run;
    bool _wrapped = false;

    // The width `_rows` was folded at, so a relayout to the same width costs nothing.
    double _folded = -1.0;

    int _weight = Typeface::mono;
    float _size = Theme::fontSmall;

    std::function<BLRgba32(size_t)> _ink;

    Spot _anchor;
    Spot _caret;
};

}
