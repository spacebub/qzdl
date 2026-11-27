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
#include "gui/draw/Band.h"
#include "gui/state/State.h"

namespace components {

// The add-on list: a row per file, reordered by its grip. With sections, a header per group, and a row carried
// onto a group joins it.
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

    // The list scrolls its rows inside its own box, so ttk cannot see a group menu lose its header.
    bool wheel(double steps, const ttk::Pointer &at) override;

    bool advance(double now) override;

    bool drag_files(const ttk::Pointer &at) override;
    void drag_files_ended() override;
    bool drop_files(const std::vector<std::string> &paths, const ttk::Pointer &at) override;

private:
    [[nodiscard]] static bool grouped();

    // How far a file row is indented past the arrows of the headers.
    [[nodiscard]] static double inset();

    void paintLine(const ttk::Painter &painter, const BLRect &line, int index) const;
    void paintHeader(const ttk::Painter &painter, const BLRect &line, int index) const;
    void paintFile(const ttk::Painter &painter, const BLRect &line, int index) const;

    void pickUp(int row);

    // Follows the pointer of a carry: the gap in load order, or the group under it.
    void aim();

    // Scrolls while a carry rests near the top or bottom edge.
    void edgeScroll(double now);

    // Files the carried row in the lit group and sets it to settle on its new line. False when it has none.
    bool joinGroup();

    void clickHeader(int line, const ttk::Pointer &at);

    // The section of the line under `y`, the last one below the lines.
    [[nodiscard]] int sectionAt(double y) const;

    // The gap between rows nearest `y`, from 0 before the first to the row count after the last.
    [[nodiscard]] int gapAt(double y) const;

    // Outlines the list and marks the gap the dragged files would go in.
    void paintDrop(const ttk::Painter &painter) const;

    // Where the mark for `_gap` is drawn. Empty while there is none.
    [[nodiscard]] BLRect lineBox() const;

    // The one place the list changes.
    void land();

    // The lines drawn. An empty Ungrouped shows only while a row is carried, as a place to drop it.
    [[nodiscard]] size_t shownLines() const;

    void fitReach();

    // The top of line `index` in content coordinates, the reach at the line count.
    [[nodiscard]] double lineTop(size_t index) const;

    // The line at content height `y`, unclamped. A gap counts as the header below it.
    [[nodiscard]] int lineIn(double y) const;

    [[nodiscard]] static double lineHeight(size_t index);

    // The header line `line` on screen.
    [[nodiscard]] BLRect headerBox(int line) const;

    // The header of section `at` and as much of its rows as show, for a list drawn from `top`.
    [[nodiscard]] BLRect groupBox(int at, double top, double wide) const;

    // How far the section shows, from 0 folded to 1 open.
    [[nodiscard]] float shown(const State::Section &section) const;

    // The height of the rows of the section, all of them open.
    [[nodiscard]] static double body(const State::Section &section);

    // The line under `y`, clamped to the lines.
    [[nodiscard]] int lineAt(double y) const;

    [[nodiscard]] int rowAt(double y) const;

    // The right edge of a row, short of the scroll bar.
    [[nodiscard]] double rowEnd() const;

    [[nodiscard]] bool overLane(double x) const;

    // Whether `x` is on the check box of a row indented by `inset`.
    [[nodiscard]] bool checkAt(double x, double inset) const;

    // The row button at `x`, numbered from the right, or -1.
    [[nodiscard]] int buttonAt(double x) const;

    // Rows the carried one passed close up behind it.
    [[nodiscard]] double shiftOf(size_t index, double step) const;

    Reach *_reach;

    int _filesRev = -1;

    int _over = -1;
    int _overGrip = -1;
    int _overButton = -1;

    int _carrying = -1;
    int _target = -1;
    bool _dragging = false;

    // A row pressed by its grip, or by its body with sections, carried once the pointer leaves the slack.
    int _held = -1;
    double _grabX = 0.0;

    // The section a carry or files from another program would land in.
    int _lit = -1;

    // The section whose menu is open, which keeps its button shown.
    int _menuFor = -1;

    double _pointerY = 0.0;
    double _edgeAt = 0.0;
    double _edgeGoal = 0.0;

    // The press went to the bar or the track, so the release is not a click.
    bool _scrolling = false;

    // The last click on a name, the first half of a double click.
    int _clickedRow = -1;
    double _clickedAt = 0.0;

    // In content coordinates, so the carried row stays under the pointer while the list scrolls.
    double _grabY = 0.0;
    double _landing = 0.0;

    ttk::Anim::Tween _carryY;

    Band::Folds _folds;

    // Files from another program are over the list, to go in at `_gap`.
    bool _filesOver = false;
    int _gap = -1;
};

}
