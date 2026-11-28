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
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/layout/Scroll.h"
#include "ttk/util/Selection.h"

#include "gui/components/Reach.h"
#include "gui/draw/Band.h"

namespace components {

// The tree of the file browser. A click on a file picks it the way the file picker marks one, and a click on a
// folder folds it.
class BrowserRows : public ttk::Scroll {
public:
    static constexpr double ROW = 34.0;
    static constexpr double INDENT = 18.0;

    explicit BrowserRows(Reach *reach);

    void sync();

    // The file rows shown.
    [[nodiscard]] size_t files() const { return _files.size(); }

    void arrange(ttk::Typeface &type) override;

    [[nodiscard]] bool activates() const override { return false; }

    [[nodiscard]] ttk::Cursor cursor_at(double x, double y) const override;

    void paint(const ttk::Painter &painter) override;

    void hover(const ttk::Pointer &at) override;
    void leave() override;

    bool press(const ttk::Pointer &at) override;
    void release(const ttk::Pointer &at) override;

    bool key(const ttk::Key &pressed) override;

    bool advance(double now) override;

private:
    // A folder sliding open or shut. The lines below it move up by `hidden`, and its own lines slide under it.
    struct Cut {
        size_t line = 0;
        size_t lines = 0;
        double hidden = 0.0;
    };

    [[nodiscard]] std::vector<Cut> cuts() const;

    [[nodiscard]] static double top(size_t line, const std::vector<Cut> &cuts);

    // The line shown at content height `into`, or -1.
    [[nodiscard]] static int lineAt(double into, const std::vector<Cut> &cuts);

    // Whether the selection holds the picks and the anchor of the files shown.
    [[nodiscard]] bool mirrors() const;

    void fitReach();

    void paintRow(const ttk::Painter &painter, const BLRect &box, int line) const;

    // Picks the selected files. The picks not shown stay.
    void apply() const;

    // The line under `y`, or -1.
    [[nodiscard]] int lineUnder(double y) const;

    // The row button at `x`, numbered from the right, or -1.
    [[nodiscard]] int buttonAt(double x) const;

    [[nodiscard]] double rowEnd() const;

    Reach *_reach;

    // The file rows shown, by row index. The selection numbers them in this order.
    std::vector<int> _files;
    ttk::Selection _selection;

    // The path of the anchor, so it outlives a new listing.
    std::string _anchor;

    // By row index.
    Band::Folds _folds;
    int _listing = 0;

    int _over = -1;
    int _overButton = -1;

    // A release carries no keys, so those held at the press are kept for it.
    bool _ctrl = false;
    bool _shift = false;

    // The press went to the bar or the track, so the release is not a click.
    bool _scrolling = false;
};

// The file browser under the add-on list: a folding header with a filter and a menu, over the rows.
class BrowserBand : public ttk::Widget {
public:
    explicit BrowserBand(Reach *reach);

    // How far the band is open, from 0 folded to 1.
    [[nodiscard]] float shown() const { return _slide.value(); }

    void sync();

    void arrange(ttk::Typeface &type) override;

    void paint(const ttk::Painter &painter) override;

    [[nodiscard]] ttk::Cursor cursor_at(double x, double y) const override;

    void hover(const ttk::Pointer &at) override;
    void leave() override;

    bool press(const ttk::Pointer &at) override;
    void release(const ttk::Pointer &at) override;

    bool advance(double now) override;

private:
    [[nodiscard]] BLRect head() const;

    // The count of files, after the own folder fitted to the header.
    void fitCount(ttk::Typeface &type);

    void showMenu();

    Reach *_reach;

    ttk::Field *_filter = nullptr;
    BrowserRows *_rows = nullptr;

    int _rev = -1;
    std::string _said;

    bool _over = false;
    bool _overMenu = false;
    bool _menuOpen = false;

    ttk::Anim::Tween _slide;
};

}
