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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "ttk/draw/Glyphs.h"
#include "ttk/draw/Theme.h"
#include "ttk/draw/Typeface.h"
#include "ttk/toolkit/Root.h"
#include "ttk/toolkit/overlays/Menu.h"
#include "ttk/util/Desktop.h"
#include "ttk/util/Format.h"

#include "gui/components/BrowserBand.h"
#include "gui/components/GroupMenu.h"
#include "gui/components/Parts.h"
#include "gui/draw/Band.h"
#include "gui/model/BrowserBridge.h"
#include "gui/services/Filters.h"
#include "gui/state/State.h"

using namespace ttk;

namespace {

constexpr const char *CAPTION = "FILE BROWSER";

constexpr double FILTER = 180.0;
constexpr double FILTER_HEIGHT = 26.0;

// The shortest rule left beside an own folder.
constexpr double RULE = 40.0;

// The buttons on a hovered row, numbered from the right.
constexpr int FOLDER = 0;
constexpr int READ = 1;
constexpr int BUTTONS = 2;
constexpr double BUTTON = Band::BUTTON;

enum class FolderAction : std::uint8_t {
    Choose,
    Clear,
};

const State::BrowserRow &rowOf(const int index) {
    return State::get().browser.rows[static_cast<size_t>(index)];
}

}

namespace components {

BrowserRows::BrowserRows(Reach *reach) : _reach(reach) {
    _takesPointer = true;
}

bool BrowserRows::mirrors() const {
    if (_selection.size() != _files.size()) {
        return false;
    }

    for (size_t at = 0; at < _files.size(); ++at) {
        if (_selection.selected(static_cast<int>(at)) != (rowOf(_files[at]).picks > 0)) {
            return false;
        }
    }

    const int anchor = _selection.anchor();

    return anchor < 0 ? _anchor.empty() : rowOf(_files[static_cast<size_t>(anchor)]).path == _anchor;
}

void BrowserRows::sync() {
    const State::BrowserState &state = State::get().browser;

    // The slides hold row numbers, which a new listing gives to other rows.
    if (std::exchange(_listing, state.listing) != state.listing) {
        _folds = {};
    }

    _folds.settled();
    _files.clear();

    for (const int index : state.shown) {
        if (!rowOf(index).folder) {
            _files.push_back(index);
        }
    }

    // Rebuilt only when the picks moved, so a Shift click after another still runs from the same anchor.
    if (!mirrors()) {
        _selection.reset(_files.size());

        for (size_t at = 0; at < _files.size(); ++at) {
            const State::BrowserRow &row = rowOf(_files[at]);

            if (row.picks > 0) {
                _selection.set(static_cast<int>(at), true);
            }

            if (row.path == _anchor) {
                _selection.set_anchor(static_cast<int>(at));
            }
        }
    }

    fitReach();
    invalidate();
}

void BrowserRows::arrange(Typeface & /*type*/) {
    fitReach();
}

std::vector<BrowserRows::Cut> BrowserRows::cuts() const {
    const State::BrowserState &state = State::get().browser;
    std::vector<Cut> out;

    _folds.each([&](const int index, const float open) {
        const auto line = std::ranges::lower_bound(state.shown, index);

        if (line == state.shown.end() || *line != index) {
            return;
        }

        const auto end = std::ranges::lower_bound(line, state.shown.end(), static_cast<int>(rowOf(index).end));
        const auto lines = static_cast<size_t>(end - line - 1);

        out.push_back(Cut{
                .line = static_cast<size_t>(line - state.shown.begin()),
                .lines = lines,
                .hidden = static_cast<double>(lines) * ROW * (1.0 - open),
        });
    });

    return out;
}

double BrowserRows::top(const size_t line, const std::vector<Cut> &cuts) {
    double y = static_cast<double>(line) * ROW;

    for (const Cut &cut : cuts) {
        y -= line > cut.line ? cut.hidden : 0.0;
    }

    return y;
}

int BrowserRows::lineAt(const double into, const std::vector<Cut> &cuts) {
    const auto lines = std::views::iota(size_t{0}, State::get().browser.shown.size());

    // The search needs the tops in order, but a line sliding under its folder rises above it. It counts at the folder.
    const auto settled = [&](const size_t at) {
        double y = top(at, cuts);

        for (const Cut &cut : cuts) {
            y = at > cut.line && at <= cut.line + cut.lines ? std::max(y, top(cut.line, cuts)) : y;
        }

        return y;
    };
    const auto after = std::ranges::partition_point(lines, [&](const size_t at) { return settled(at) <= into; });

    if (after == lines.begin()) {
        return -1;
    }

    size_t line = *after - 1;

    // A line still under its folder gives way to the folder.
    for (const Cut &cut : cuts) {
        if (line > cut.line && line <= cut.line + cut.lines && into < top(cut.line, cuts) + ROW) {
            line = cut.line;
        }
    }

    return into < top(line, cuts) + ROW ? static_cast<int>(line) : -1;
}

void BrowserRows::fitReach() {
    set_reach(top(State::get().browser.shown.size(), cuts()));
}

Cursor BrowserRows::cursor_at(const double x, const double y) const {
    return x < rowEnd() && lineUnder(y) >= 0 ? Cursor::Pointer : Cursor::Default;
}

void BrowserRows::paint(const Painter &painter) {
    const State::BrowserState &state = State::get().browser;

    if (state.shown.empty()) {
        const char *said = state.folder.empty() ? "Choose a folder here or in Settings"
                           : state.rows.empty() ? "No add-ons in this folder"
                                                : "Nothing matches";

        painter.label(painter.font(400, Theme::fontSmall),
                      BLRect{_box.x + 24.0, _box.y, _box.w - 48.0, ROW},
                      Align::Start,
                      said,
                      Theme::palette().muted);

        return;
    }

    const std::vector<Cut> cut = cuts();
    const double origin = _box.y - offset();

    painter.push(_box);

    for (auto line = static_cast<size_t>(std::max(0, lineAt(offset(), cut))); line < state.shown.size(); ++line) {
        const BLRect box{_box.x, origin + top(line, cut), rowEnd() - _box.x, ROW};

        if (box.y >= _box.y + _box.h) {
            break;
        }

        // Skipped before the clips: a long folder sliding shut passes hundreds of lines above the rows.
        if (!painter.needed(box)) {
            continue;
        }

        int clips = 0;

        for (const Cut &sliding : cut) {
            if (line > sliding.line && line <= sliding.line + sliding.lines) {
                const double under = origin + top(sliding.line, cut) + ROW;

                painter.push(BLRect{_box.x, under, _box.w, static_cast<double>(sliding.lines) * ROW});
                ++clips;
            }
        }

        if (painter.needed(box)) {
            paintRow(painter, box, static_cast<int>(line));
        }

        for (; clips > 0; --clips) {
            painter.pop();
        }
    }

    painter.pop();

    Scroll::paint(painter);
}

void BrowserRows::paintRow(const Painter &painter, const BLRect &box, const int line) const {
    const Theme::Palette &palette = Theme::palette();
    const State::BrowserState &state = State::get().browser;
    const int index = state.shown[static_cast<size_t>(line)];
    const State::BrowserRow &row = rowOf(index);
    const bool over = line == _over;
    const bool picked = !row.folder && row.picks > 0;
    const double span = Glyphs::span(1.0F);
    const double glyph = box.y + ((box.h - span) / 2.0);
    const double lane = box.x + 24.0 + (static_cast<double>(row.depth) * INDENT) + ((18.0 - span) / 2.0);
    const double left = box.x + 52.0 + (static_cast<double>(row.depth + (row.folder ? 1 : 0)) * INDENT);
    double right = box.x + box.w - (over && !row.folder ? 4.0 + (BUTTON * BUTTONS) : 12.0);

    if (picked || over) {
        painter.round(box, Theme::radiusSmall - 2.0, picked ? palette.accentSoft : palette.hover);
    }

    if (row.folder) {
        const bool folded = row.folded && state.filter.empty();
        const float open = state.filter.empty() ? _folds.open(index, row.folded) : 1.0F;

        Glyphs::draw(painter.context(),
                     Glyphs::Glyph::Chevron,
                     BLPoint{lane, glyph},
                     1.0F,
                     over ? palette.text : palette.faint,
                     open * 180.0F);
        Glyphs::draw(painter.context(), Glyphs::Glyph::Folder, BLPoint{lane + INDENT, glyph}, 1.0F, palette.muted);

        if (folded && row.picks > 0) {
            const BLFont &small = painter.font(400, Theme::fontSmall);
            const std::string count = std::to_string(row.picks) + " picked";
            const double wide = painter.width(small, count);

            painter.label(small, BLRect{left, box.y, right - left, box.h}, Align::End, count, palette.accent);

            right -= wide + 12.0;
        }
    } else if (picked) {
        Glyphs::draw(painter.context(), Glyphs::Glyph::Check, BLPoint{lane, glyph}, 1.0F, palette.accent);
    }

    if (over && !row.folder) {
        for (int button = 0; button < BUTTONS; ++button) {
            const double middle = box.x + box.w - 18.0 - (BUTTON * button);

            Glyphs::draw(painter.context(),
                         button == FOLDER ? Glyphs::Glyph::Folder : eyeGlyph(),
                         BLPoint{middle - (span / 2.0), glyph},
                         1.0F,
                         button == _overButton ? palette.text : palette.muted);
        }
    }

    painter.row(painter.font(400, Theme::fontBody), BLRect{left, box.y, right - left, box.h}, row.name, palette.text);
}

void BrowserRows::hover(const Pointer &at) {
    const int line = at.x < rowEnd() ? lineUnder(at.y) : -1;
    const int button =
            line >= 0 && !rowOf(State::get().browser.shown[static_cast<size_t>(line)]).folder ? buttonAt(at.x) : -1;

    if (line != _over || button != _overButton) {
        const std::vector<Cut> cut = cuts();

        for (const int lit : {_over, line}) {
            if (lit >= 0) {
                invalidate(BLRect{_box.x, _box.y - offset() + top(static_cast<size_t>(lit), cut), _box.w, ROW}, _box);
            }
        }

        _over = line;
        _overButton = button;
    }
}

void BrowserRows::leave() {
    Widget::leave();

    _over = -1;
    _overButton = -1;
}

bool BrowserRows::press(const Pointer &at) {
    _scrolling = Scroll::press(at);
    _ctrl = at.ctrl;
    _shift = at.shift;

    if (root() != nullptr) {
        root()->focus(this);
    }

    return true;
}

void BrowserRows::release(const Pointer &at) {
    Scroll::release(at);

    const int line = lineUnder(at.y);

    if (std::exchange(_scrolling, false) || line < 0) {
        return;
    }

    const int index = State::get().browser.shown[static_cast<size_t>(line)];
    const State::BrowserRow &row = rowOf(index);

    if (row.folder) {
        if (State::get().browser.filter.empty()) {
            _folds.turn(index, row.folded, now(), [index](const bool folded) { BrowserBridge::fold(index, folded); });
            wake();
        }

        return;
    }

    switch (buttonAt(at.x)) {
        case FOLDER:
            Desktop::open(Format::directory_of(row.path));
            return;
        case READ:
            _reach->showReadmes(row.path);
            return;
        default:
            break;
    }

    // A listing starts with no anchor, and a Shift click without one toggles like any other.
    const bool ranged = _shift && _selection.anchor() >= 0;
    const auto file = static_cast<int>(std::ranges::lower_bound(_files, index) - _files.begin());

    _selection.click(file, _ctrl || !ranged, ranged);
    _anchor = rowOf(_files[static_cast<size_t>(_selection.anchor())]).path;

    apply();
}

bool BrowserRows::key(const Key &pressed) {
    if (!pressed.ctrl || pressed.code != 'a') {
        return false;
    }

    if (_selection.select_all()) {
        apply();
    }

    return true;
}

bool BrowserRows::advance(const double now) {
    const bool gliding = Scroll::advance(now);
    const bool folding = _folds.advance(now);

    if (!_folds.empty()) {
        fitReach();
        invalidate();
    }

    return gliding || folding;
}

void BrowserRows::apply() const {
    const std::vector<State::BrowserRow> &rows = State::get().browser.rows;
    std::vector<std::string> paths;
    size_t next = 0;

    for (size_t at = 0; at < rows.size(); ++at) {
        const bool shown = next < _files.size() && std::cmp_equal(_files[next], at);
        const bool wanted =
                shown ? _selection.selected(static_cast<int>(next)) : !rows[at].folder && rows[at].picks > 0;

        next += shown ? 1 : 0;

        if (wanted) {
            paths.push_back(rows[at].path);
        }
    }

    _reach->config.browser().applyPicks(paths);
}

int BrowserRows::lineUnder(const double y) const {
    return lineAt(y - _box.y + offset(), cuts());
}

int BrowserRows::buttonAt(const double x) const {
    const double from = rowEnd() - x;

    return from > 0.0 && from < 4.0 + (BUTTON * BUTTONS) ? std::max(0, static_cast<int>((from - 4.0) / BUTTON)) : -1;
}

double BrowserRows::rowEnd() const {
    return _box.x + _box.w - (scrollable() ? Theme::lane : 0.0);
}

BrowserBand::BrowserBand(Reach *reach) : _reach(reach) {
    _takesPointer = true;

    _filter = append(std::make_unique<Field>("", [](const std::string &value) { BrowserBridge::setFilter(value); }));
    _filter->compact()->leading_glyph(Glyphs::Glyph::Search)->placeholder("Filter");

    _rows = append(std::make_unique<BrowserRows>(reach));
}

void BrowserBand::sync() {
    const State::BrowserState &state = State::get().browser;

    if (state.rev == _rev) {
        return;
    }

    if (_rev < 0) {
        _slide.set(state.open ? 1.0F : 0.0F);
    } else {
        // The timing of every fold in the toolkit.
        _slide.toward(state.open ? 1.0F : 0.0F, now(), 0.22, Anim::Curve::CubicOut);

        if (_slide.live()) {
            wake();
        }
    }

    _rev = state.rev;

    _filter->set_visible(state.open);
    _rows->set_visible(state.open || _slide.value() > 0.0F);
    _rows->sync();

    if (root() != nullptr) {
        fitCount(root()->type());
    }

    invalidate(head());
}

void BrowserBand::arrange(Typeface &type) {
    const BLRect line = head();

    _filter->place(
            BLRect{Band::menuBox(line).x - FILTER, line.y + ((line.h - FILTER_HEIGHT) / 2.0), FILTER, FILTER_HEIGHT},
            type);
    _rows->place(BLRect{_box.x, _box.y + Band::LINE, _box.w, std::max(0.0, _box.h - Band::LINE)}, type);

    fitCount(type);
}

void BrowserBand::fitCount(Typeface &type) {
    const State::BrowserState &state = State::get().browser;
    const size_t all = state.files;

    _said = state.folder.empty() || !state.open ? std::string()
            : state.filter.empty()              ? say(all, "file")
                                                : std::to_string(_rows->files()) + " of " + std::to_string(all);

    if (!state.ownFolder) {
        return;
    }

    const BLFont &small = type.at(400, Theme::fontTiny);
    const double end = _filter->visible() ? _filter->box().x - 4.0 : Band::menuBox(head()).x;
    const double taken = 24.0 + type.width_tracked(type.at(600, Theme::fontTiny), CAPTION, 0.9F) + 8.0;
    const double room = end - _box.x - taken - type.width(small, _said) - RULE;
    const double wide = type.width(small, state.folder);
    std::string folder = state.folder;

    if (wide > room) {
        folder = Format::fit_path(folder,
                                  static_cast<int>(std::max(0.0, room) * static_cast<double>(folder.size()) / wide));
    }

    _said = folder + "  " + _said;
}

void BrowserBand::paint(const Painter &painter) {
    const BLRect line = head();
    const bool open = _filter->visible();
    const bool dots = _over || _menuOpen;

    Band::paint(painter,
                open ? BLRect{line.x, line.y, _filter->box().x - 4.0 - line.x, line.h} : line,
                {
                        .caption = CAPTION,
                        .count = _said,
                        .indent = 24.0,
                        // The band opens upward, so its chevron turns the other way from a group.
                        .open = 1.0F - shown(),
                        .named = true,
                        .over = _over,
                        .menu = dots && !open,
                        .overMenu = _overMenu,
                        .lit = false,
                });

    if (dots && open) {
        Band::paintMenu(painter, line, _overMenu);
    }

    Widget::paint(painter);
}

Cursor BrowserBand::cursor_at(const double /*x*/, const double y) const {
    return y < _box.y + Band::LINE ? Cursor::Pointer : Cursor::Default;
}

void BrowserBand::hover(const Pointer &at) {
    const bool over = at.y < _box.y + Band::LINE;
    const bool menu = Band::onMenu(head(), at.x, at.y);

    if (over != _over || menu != _overMenu) {
        _over = over;
        _overMenu = menu;

        invalidate(head());
    }
}

void BrowserBand::leave() {
    Widget::leave();

    _over = false;
    _overMenu = false;
}

bool BrowserBand::press(const Pointer &at) {
    return at.y < _box.y + Band::LINE;
}

void BrowserBand::release(const Pointer &at) {
    if (Band::onMenu(head(), at.x, at.y)) {
        showMenu();
    } else if (at.y < _box.y + Band::LINE) {
        _reach->config.browser().setOpen(!State::get().browser.open);
    }
}

bool BrowserBand::advance(const double now) {
    _slide.advance(now);
    _rows->set_visible(State::get().browser.open || _slide.value() > 0.0F);

    // The split under the add-on list moves with the slide. The window is laid out once it ends, which matches the
    // pointer again.
    if (root() != nullptr && _slide.live()) {
        parent()->place(parent()->box(), root()->type());
        parent()->invalidate();
    } else if (root() != nullptr) {
        root()->relayout();
    }

    return _slide.live();
}

BLRect BrowserBand::head() const {
    return BLRect{_box.x, _box.y, _box.w, Band::LINE};
}

void BrowserBand::showMenu() {
    const bool own = State::get().browser.ownFolder;
    const std::vector<Menu::Row> rows = {
            Menu::item(FolderAction::Choose, "Choose folder", Glyphs::Glyph::Folder),
            Menu::item(FolderAction::Clear, "Clear folder", Glyphs::Glyph::Close, false, !own),
    };

    _menuOpen = true;

    invalidate(head());

    dropMenu(
            *this,
            rows,
            Band::menuBox(head()),
            [this](const int action) {
                if (static_cast<FolderAction>(action) == FolderAction::Clear) {
                    _reach->config.browser().setOwnFolder({});

                    return;
                }

                _reach->files.open("Select the folder to list",
                                   {},
                                   true,
                                   false,
                                   false,
                                   LastDir::WAD,
                                   Picked::first([this](const std::string &path) {
                                       _reach->config.browser().setOwnFolder(path);
                                   }));
            },
            [this] {
                _menuOpen = false;

                invalidate(head());
            });
}

}
