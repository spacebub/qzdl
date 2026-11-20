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
#include <string>
#include <vector>

#include "ttk/draw/Anim.h"
#include "ttk/toolkit/layout/Scroll.h"

#include "gui/components/Reach.h"

namespace components {

// The add-on list: a row per file, reordered by its grip.
class AddonList : public ttk::Scroll {
public:
    explicit AddonList(Reach *reach);

    [[nodiscard]] static double rowHeight();

    [[nodiscard]] ttk::Cursor cursor_at(double x, double y) const override;

    void arrange(ttk::Typeface &type) override;

    void sync();

    void paint(const ttk::Painter &painter) override;

    void hover(const ttk::Pointer &at) override;
    void leave() override;

    bool press(const ttk::Pointer &at) override;
    void drag(const ttk::Pointer &at) override;
    void release(const ttk::Pointer &at) override;

    bool advance(double now) override;

    bool drag_files(const ttk::Pointer &at) override;
    void drag_files_ended() override;
    bool drop_files(const std::vector<std::string> &paths, const ttk::Pointer &at) override;

private:
    // The gap between rows nearest `y`, from 0 before the first to the row count after the last.
    [[nodiscard]] int gapAt(double y) const;

    // Outlines the list and marks the gap the dragged files would go in.
    void paintDrop(const ttk::Painter &painter) const;

    // Where the mark for `_gap` is drawn. Empty while there is none.
    [[nodiscard]] BLRect lineBox() const;

    // The one place the list changes.
    void land();

    [[nodiscard]] int rowAt(double y) const;

    // The right edge of a row, short of the scroll bar.
    [[nodiscard]] double rowEnd() const;

    [[nodiscard]] bool overLane(double x) const;

    // Rows the carried one passed close up behind it.
    [[nodiscard]] double shiftOf(size_t index, double step) const;

    Reach *_reach;

    int _filesRev = -1;

    int _over = -1;
    int _overGrip = -1;
    bool _overShut = false;
    bool _overFolder = false;

    int _carrying = -1;
    int _target = -1;
    bool _dragging = false;

    // The press went to the bar or the track, so the release is not a click.
    bool _scrolling = false;

    // The last click on a name, the first half of a double click.
    int _clickedRow = -1;
    double _clickedAt = 0.0;

    double _grabY = 0.0;
    double _landing = 0.0;

    ttk::Anim::Tween _carryY;

    // Files from another program are over the list, to go in at `_gap`.
    bool _filesOver = false;
    int _gap = -1;
};

}
