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
#include <string>
#include <utility>
#include <vector>

#include "ttk/draw/Glyphs.h"
#include "ttk/draw/Theme.h"
#include "ttk/draw/Typeface.h"
#include "ttk/util/Desktop.h"
#include "ttk/util/Format.h"

#include "gui/components/AddonList.h"
#include "gui/components/GroupMenu.h"
#include "gui/draw/Band.h"
#include "gui/draw/DropMark.h"
#include "gui/services/Filters.h"
#include "gui/state/State.h"

using namespace ttk;

namespace {

// How long a dropped row takes to walk to its gap.
constexpr double SETTLING = 0.16;

constexpr double DOUBLE_CLICK = 0.4;

// How far a press on a row with sections moves before the row is carried.
constexpr double SLACK = 5.0;

// The band at either end that scrolls a carry, and the fastest it scrolls, in pixels a second.
constexpr double EDGE = 24.0;
constexpr double EDGE_SPEED = 720.0;

// The space above the header of every section but the first.
constexpr double SECTION_GAP = 8.0;

// The buttons on a hovered row, numbered from the right.
constexpr int REMOVE = 0;
constexpr int FOLDER = 1;
constexpr int READ = 2;
constexpr int BUTTONS = 3;

// The one button on a named header.
constexpr int MENU = 0;
constexpr double BUTTON = Band::BUTTON;

Glyphs::Glyph eye() {
    static const Glyphs::Glyph glyph = Glyphs::add({
            .outline = "M 1.5 7 C 3.5 3.2 10.5 3.2 12.5 7 C 10.5 10.8 3.5 10.8 1.5 7 Z",
            .solid = "M 5 7 A 2 2 0 1 0 9 7 A 2 2 0 1 0 5 7 Z",
            .box = 14.0F,
            .solidBox = 14.0F,
            .pen = 1.4F,
    });

    return glyph;
}

void checkBox(const Painter &painter, const BLRect &line, const double x, const bool filled, const Glyphs::Glyph mark) {
    const Theme::Palette &palette = Theme::palette();
    const BLRect check{x, line.y + ((line.h - 18.0) / 2.0), 18.0, 18.0};

    painter.round(check, 5.0, filled ? palette.accent : palette.field);
    painter.outline(check, 5.0, 1.0, filled ? palette.accent : palette.border);

    if (filled) {
        const double tick = Glyphs::span(0.85F);

        Glyphs::draw(painter.context(),
                     mark,
                     BLPoint{check.x + ((check.w - tick) / 2.0), check.y + ((check.h - tick) / 2.0)},
                     0.85F,
                     palette.accentText);
    }
}

void rowButton(
        const Painter &painter, const BLRect &line, const int button, const Glyphs::Glyph glyph, const BLRgba32 tone) {
    const double span = Glyphs::span(1.0F);
    const double middle = line.x + line.w - 18.0 - (BUTTON * button);

    Glyphs::draw(
            painter.context(), glyph, BLPoint{middle - (span / 2.0), line.y + ((line.h - span) / 2.0)}, 1.0F, tone);
}

}

namespace components {

AddonList::AddonList(Reach *reach) : _reach(reach) {
    _takesPointer = true;
}

double AddonList::rowHeight() {
    return State::get().cfg.showPaths ? 46.0 : 34.0;
}

Cursor AddonList::cursor_at(const double x, const double y) const {
    const int row = rowAt(y);

    if (row < 0 || overLane(x)) {
        return Cursor::Default;
    }

    const State::Cfg &cfg = State::get().cfg;
    const State::AddonLine &line = cfg.addonLines[static_cast<size_t>(row)];

    if (line.file < 0) {
        const bool named = cfg.fileSections[static_cast<size_t>(line.section)].group != 0;

        return named || checkAt(x, 0.0) ? Cursor::Pointer : Cursor::Default;
    }

    const bool sections = grouped();

    if (!sections && x < _box.x + 22.0) {
        return Cursor::Resize;
    }

    if (checkAt(x, inset()) || buttonAt(x) >= 0) {
        return Cursor::Pointer;
    }

    return sections ? Cursor::Grab : Cursor::Default;
}

bool AddonList::grouped() {
    return !State::get().cfg.fileSections.empty();
}

double AddonList::inset() {
    return grouped() ? 22.0 : 0.0;
}

void AddonList::arrange(Typeface & /*type*/) {
    set_reach(lineTop(State::get().cfg.addonLines.size()));
}

void AddonList::sync() {
    const State::Cfg &cfg = State::get().cfg;

    _folds.settled();

    if (cfg.filesRev == _filesRev) {
        return;
    }

    _filesRev = cfg.filesRev;

    set_reach(lineTop(cfg.addonLines.size()));
    invalidate();
}

void AddonList::paint(const Painter &painter) {
    const Theme::Palette &palette = Theme::palette();
    const State::Cfg &cfg = State::get().cfg;
    const std::vector<State::AddonLine> &lines = cfg.addonLines;

    if (lines.empty()) {
        const BLFont &heading = painter.font(600, Theme::fontMedium);
        const double wide = _box.w - 48.0;
        double y = _box.y + ((_box.h - 70.0) / 2.0);

        painter.label(heading,
                      BLRect{_box.x + 24.0, y, wide, painter.line_height(heading)},
                      Align::Start,
                      "Nothing loaded",
                      palette.muted);

        y += painter.line_height(heading) + 6.0;

        painter.paragraph(painter.font(400, Theme::fontSmall),
                          BLRect{_box.x + 24.0, y, wide, 0.0},
                          "WADs, PK3s, DEH and BEX patches, demos and configs go here. They "
                          "are passed to the source port in the order they are listed.",
                          palette.faint);

        paintDrop(painter);

        return;
    }

    painter.push(_box);

    const double step = rowHeight();
    const double wide = _box.w - (scrollable() ? Theme::lane : 0.0);
    const double top = _box.y - offset();

    if (_lit >= 0) {
        painter.round(groupBox(_lit, top, wide), Theme::radiusSmall - 2.0, palette.accentSoft);
    }

    for (size_t index = 0; index < lines.size(); ++index) {
        const BLRect line{_box.x, top + lineTop(index) + shiftOf(index, step), wide, lineHeight(index)};

        if (std::cmp_equal(index, _carrying) || !painter.needed(line)) {
            continue;
        }

        // The rows of a group sliding open or shut show only as far as it has slid.
        const int section = lines[index].section;
        const bool cut =
                !_folds.empty() && section >= 0 && shown(cfg.fileSections[static_cast<size_t>(section)]) < 1.0F;

        if (cut) {
            painter.push(groupBox(section, top, wide));
        }

        paintLine(painter, line, static_cast<int>(index));

        if (cut) {
            painter.pop();
        }
    }

    // Last, so the carried row passes over the rows it crosses.
    if (_carrying >= 0) {
        const double y = top + lineTop(static_cast<size_t>(_carrying)) + _carryY.value();
        const BLRect line{_box.x, y, wide, step};

        if (painter.needed(line)) {
            paintLine(painter, line, _carrying);
        }
    }

    painter.pop();

    Scroll::paint(painter);
    paintDrop(painter);
}

void AddonList::paintLine(const Painter &painter, const BLRect &line, const int index) const {
    if (State::get().cfg.addonLines[static_cast<size_t>(index)].file < 0) {
        paintHeader(painter, line, index);

        return;
    }

    const Theme::Palette &palette = Theme::palette();

    if (index == _carrying) {
        painter.round(line, Theme::radiusSmall - 2.0, palette.mutedSoft);
    } else if (index == _over) {
        painter.round(line, Theme::radiusSmall - 2.0, palette.hover);
    }

    paintFile(painter, line, index);
}

void AddonList::paintHeader(const Painter &painter, const BLRect &line, const int index) const {
    const int at = State::get().cfg.addonLines[static_cast<size_t>(index)].section;
    const State::Section &section = State::get().cfg.fileSections[static_cast<size_t>(at)];
    const bool named = section.group != 0;
    const bool over = index == _over;
    const std::string count =
            section.count > 0 ? std::to_string(section.loaded) + " of " + std::to_string(section.count) : "empty";

    Band::paint(painter,
                line,
                {
                        .name = section.name,
                        .count = count,
                        .indent = 52.0,
                        .open = shown(section),
                        .named = named,
                        .over = over,
                        .menu = named && (over || at == _menuFor),
                        .overMenu = over && _overButton == MENU,
                        .lit = at == _lit,
                });

    checkBox(painter,
             line,
             line.x + 24.0,
             section.loaded > 0,
             section.loaded == section.count ? Glyphs::Glyph::Check : Glyphs::Glyph::Minus);
}

void AddonList::paintFile(const Painter &painter, const BLRect &line, const int index) const {
    const Theme::Palette &palette = Theme::palette();
    const State::Cfg &cfg = State::get().cfg;
    const int file = cfg.addonLines[static_cast<size_t>(index)].file;
    const State::FileRow &row = cfg.files[static_cast<size_t>(file)];
    const bool over = index == _over;

    if (grouped()) {
        painter.label(painter.font(400, Theme::fontTiny),
                      BLRect{line.x + 14.0, line.y, 30.0, line.h},
                      Align::Centre,
                      std::to_string(file + 1),
                      palette.faint);
    } else {
        const double grip = Glyphs::span(1.0F);

        Glyphs::draw(painter.context(),
                     Glyphs::Glyph::Grip,
                     BLPoint{line.x + ((22.0 - grip) / 2.0), line.y + ((line.h - grip) / 2.0)},
                     1.0F,
                     index == _overGrip ? palette.muted : palette.border);
    }

    checkBox(painter, line, line.x + inset() + 24.0, row.loaded, Glyphs::Glyph::Check);

    const double left = line.x + inset() + 52.0;
    double right = line.x + line.w - (over ? 4.0 + (BUTTON * BUTTONS) : 12.0);

    if (!grouped() && !row.group.empty()) {
        const BLFont &face = painter.font(600, Theme::fontTiny);
        const double end = over ? right - 12.0 : right;
        const double wide = std::min(painter.width(face, row.group), (end - left) / 3.0);

        painter.label(face, BLRect{end - wide, line.y, wide, line.h}, Align::Start, row.group, palette.faint);

        right = end - wide - 12.0;
    }

    const BLFont &face = painter.font(400, Theme::fontBody);
    const double taken = std::min(painter.width(face, row.name), right - left - 60.0);

    // clang-format off
    painter.label(face,
                  BLRect{left, cfg.showPaths ? line.y + 6.0 : line.y,
                         right - left, cfg.showPaths ? 20.0 : line.h},
                  Align::Start, row.name,
                  row.missing   ? palette.danger
                  : row.leftOut ? palette.warning
                  : row.loaded  ? palette.text
                                : palette.faint);
    // clang-format on

    if (!row.loaded) {
        // Blend2D strikes nothing through. The line is drawn.
        const double middle = (cfg.showPaths ? line.y + 16.0 : line.y + (line.h / 2.0));

        painter.fill(BLRect{left, middle, taken, 1.0}, palette.faint);
    }

    if (row.missing || row.leftOut) {
        painter.label(
                painter.font(600, Theme::fontTiny),
                BLRect{left + taken + 7.0, cfg.showPaths ? line.y + 6.0 : line.y, 60.0, cfg.showPaths ? 20.0 : line.h},
                Align::Start,
                row.missing ? "missing" : "left out",
                row.missing ? palette.danger : palette.warning);
    }

    if (cfg.showPaths) {
        const BLFont &mono = painter.font(Typeface::mono, Theme::fontTiny);
        const double unit = painter.width(mono, "M");
        const int room = unit > 0.0 ? static_cast<int>((right - left) / unit) : 0;

        painter.label(mono,
                      BLRect{left, line.y + 25.0, right - left, 16.0},
                      Align::Start,
                      Format::fit_path(row.directory, room),
                      palette.faint);
    }

    if (over) {
        for (int button = 0; button < BUTTONS; ++button) {
            rowButton(painter,
                      line,
                      button,
                      button == REMOVE   ? Glyphs::Glyph::Trash
                      : button == FOLDER ? Glyphs::Glyph::Folder
                                         : eye(),
                      button != _overButton ? palette.muted
                      : button == REMOVE    ? palette.danger
                                            : palette.text);
        }
    }
}

void AddonList::paintDrop(const Painter &painter) const {
    if (!_filesOver) {
        return;
    }

    DropMark::paint(painter, _box, Theme::radiusSmall);

    if (const BLRect line = lineBox(); line.h > 0.0) {
        painter.round(line, 1.0, Theme::palette().accent);
    }
}

BLRect AddonList::lineBox() const {
    if (State::get().cfg.addonLines.empty() || _gap < 0) {
        return {};
    }

    // Kept clear of the outline at either end of the list, where it would vanish into the stroke.
    const double clear = 1.0 + DropMark::WIDTH + 2.0;
    const double gap = _box.y - offset() + lineTop(static_cast<size_t>(_gap)) - 1.0;
    const double y = std::clamp(gap, _box.y + clear, _box.y + _box.h - clear - DropMark::WIDTH);
    const double wide = _box.w - (scrollable() ? Theme::lane : 0.0);

    return BLRect{_box.x + 6.0, y, wide - 12.0, DropMark::WIDTH};
}

bool AddonList::drag_files(const Pointer &at) {
    if (!_filesOver) {
        _filesOver = true;

        DropMark::invalidate(*this, _box, Theme::radiusSmall);
    }

    if (grouped()) {
        if (const int lit = sectionAt(at.y); lit != _lit) {
            _lit = lit;

            invalidate();
        }
    } else if (const int gap = gapAt(at.y); gap != _gap) {
        invalidate(lineBox());
        _gap = gap;
        invalidate(lineBox());
    }

    return true;
}

void AddonList::drag_files_ended() {
    if (!_filesOver) {
        return;
    }

    invalidate(lineBox());
    DropMark::invalidate(*this, _box, Theme::radiusSmall);

    if (std::exchange(_lit, -1) >= 0) {
        invalidate();
    }

    _filesOver = false;
    _gap = -1;
}

bool AddonList::drop_files(const std::vector<std::string> &paths, const Pointer &at) {
    const Filters::Dropped dropped = Filters::split(paths, Filters::addon(), true);

    if (!dropped.left.empty()) {
        _reach->notify.warning(Filters::leftOut(dropped.left, "an add-on", "add-ons"));
    }

    if (!dropped.taken.empty() && grouped()) {
        _reach->config.lists().addFiles(dropped.taken,
                                        State::get().cfg.fileSections[static_cast<size_t>(sectionAt(at.y))].group);
    } else if (!dropped.taken.empty()) {
        _reach->config.lists().insertFiles(dropped.taken, gapAt(at.y));
    }

    _reach->touch();

    return true;
}

int AddonList::sectionAt(const double y) const {
    return State::get().cfg.addonLines[static_cast<size_t>(lineAt(y))].section;
}

int AddonList::gapAt(const double y) const {
    const int count = static_cast<int>(State::get().cfg.addonLines.size());

    return std::clamp(lineIn(y - _box.y + offset() + (rowHeight() / 2.0)), 0, count);
}

void AddonList::hover(const Pointer &at) {
    const int row = overLane(at.x) ? -1 : rowAt(at.y);
    const bool grip = at.x < _box.x + 22.0;
    const bool header = row >= 0 && State::get().cfg.addonLines[static_cast<size_t>(row)].file < 0;
    const bool menu = header && Band::onMenu(headerBox(row), at.x, at.y);
    const int button = menu ? MENU : row >= 0 && !header ? buttonAt(at.x) : -1;

    if (row != _over || grip != (_overGrip == row) || button != _overButton) {
        _over = row;
        _overGrip = grip ? row : -1;
        _overButton = button;

        invalidate();
    }
}

void AddonList::leave() {
    Widget::leave();

    _over = -1;
    _overGrip = -1;
    _overButton = -1;
}

bool AddonList::press(const Pointer &at) {
    _scrolling = Scroll::press(at);

    if (_scrolling) {
        return true;
    }

    land();

    const int row = rowAt(at.y);

    if (row < 0) {
        return false;
    }

    _pointerY = at.y;
    _grabX = at.x;
    _grabY = at.y + offset();

    const bool body = State::get().cfg.addonLines[static_cast<size_t>(row)].file >= 0 && !checkAt(at.x, inset())
                      && buttonAt(at.x) < 0;

    _held = (grouped() ? body : at.x < _box.x + 22.0) ? row : -1;

    return true;
}

void AddonList::pickUp(const int row) {
    _dragging = true;
    _carrying = row;
    _target = row;
    _edgeAt = now();
    _edgeGoal = offset();

    _carryY.set(0.0F);
    wake();
}

void AddonList::drag(const Pointer &at) {
    Scroll::drag(at);

    _pointerY = at.y;

    if (_held >= 0 && std::hypot(at.x - _grabX, at.y + offset() - _grabY) >= SLACK) {
        // A drop with sections rebuilds the lines, so the hovered index would name another row.
        if (grouped()) {
            _over = -1;
        }

        pickUp(std::exchange(_held, -1));
    }

    if (_carrying < 0) {
        return;
    }

    aim();
    invalidate();
    wake();
}

void AddonList::aim() {
    _carryY.set(static_cast<float>(_pointerY + offset() - _grabY));

    if (grouped()) {
        _lit = sectionAt(_pointerY);
    } else {
        _target = lineAt(_pointerY);
    }
}

void AddonList::edgeScroll(const double now) {
    const double into = std::max(_box.y + EDGE - _pointerY, _pointerY - (_box.y + _box.h - EDGE));
    const double since = now - std::exchange(_edgeAt, now);

    if (into <= 0.0 || !scrollable()) {
        _edgeGoal = offset();

        return;
    }

    const double speed = std::min(into / EDGE, 1.0) * EDGE_SPEED;
    const double way = _pointerY < _box.y + (_box.h / 2.0) ? -1.0 : 1.0;

    _edgeGoal = std::clamp(_edgeGoal + (way * speed * since), 0.0, reach() - _box.h);

    scroll_to(_edgeGoal);
    aim();
}

void AddonList::release(const Pointer &at) {
    Scroll::release(at);

    if (std::exchange(_scrolling, false)) {
        return;
    }

    _held = -1;

    const State::Cfg &cfg = State::get().cfg;

    if (_carrying >= 0 && grouped() && !joinGroup()) {
        return;
    }

    if (_carrying >= 0) {
        _dragging = false;

        // The row walks to its gap. Only then does the list change.
        _carryY.run(static_cast<float>((_target - _carrying) * rowHeight()), now(), SETTLING, Anim::Curve::CubicOut);

        _landing = now() + SETTLING + 0.02;

        wake();

        return;
    }

    const int row = rowAt(at.y);

    if (row < 0) {
        return;
    }

    const int file = cfg.addonLines[static_cast<size_t>(row)].file;

    if (file < 0) {
        clickHeader(row, at);

        return;
    }

    const std::string &path = cfg.files[static_cast<size_t>(file)].file;

    switch (buttonAt(at.x)) {
        case REMOVE:
            _reach->config.lists().removeFile(file);
            return;
        case FOLDER:
            Desktop::open(Format::directory_of(path));
            return;
        case READ:
            _reach->showReadmes(path);
            return;
        default:
            break;
    }

    if (checkAt(at.x, inset())) {
        _reach->config.lists().setFileEnabled(file, !cfg.files[static_cast<size_t>(file)].loaded);

        return;
    }

    if (row == _clickedRow && now() - _clickedAt < DOUBLE_CLICK) {
        _clickedRow = -1;
        _reach->showReadmes(path);

        return;
    }

    _clickedRow = row;
    _clickedAt = now();
}

bool AddonList::joinGroup() {
    const State::Cfg &cfg = State::get().cfg;
    const State::AddonLine carried = cfg.addonLines[static_cast<size_t>(_carrying)];
    const int group = cfg.fileSections[static_cast<size_t>(std::exchange(_lit, -1))].group;
    const double from = lineTop(static_cast<size_t>(_carrying)) + _carryY.value();

    _dragging = false;

    if (group != cfg.fileSections[static_cast<size_t>(carried.section)].group) {
        _reach->config.lists().setFileGroup(carried.file, group);
    }

    const auto landed = std::ranges::find(cfg.addonLines, carried.file, &State::AddonLine::file);

    _carrying = landed != cfg.addonLines.end() ? static_cast<int>(landed - cfg.addonLines.begin()) : -1;
    _target = _carrying;

    if (_carrying < 0) {
        _carryY.set(0.0F);
        invalidate();

        return false;
    }

    _carryY.set(static_cast<float>(from - lineTop(static_cast<size_t>(_carrying))));

    return true;
}

void AddonList::clickHeader(const int line, const Pointer &at) {
    const int index = State::get().cfg.addonLines[static_cast<size_t>(line)].section;
    const State::Section &section = State::get().cfg.fileSections[static_cast<size_t>(index)];
    const int group = section.group;

    if (checkAt(at.x, 0.0)) {
        _reach->config.lists().tickGroup(group);

        return;
    }

    if (group == 0) {
        return;
    }

    if (!Band::onMenu(headerBox(line), at.x, at.y)) {
        _folds.turn(group, section.collapsed, now(), [this, group](const bool folded) {
            _reach->config.groups().fold(GroupsBridge::List::Files, group, folded);
        });

        wake();

        return;
    }

    const BLRect button = Band::menuBox(headerBox(line));

    _menuFor = index;

    invalidate();

    showGroupMenu(*_reach, *this, GroupsBridge::List::Files, group, section.name, button, [this] {
        _menuFor = -1;

        invalidate();
    });
}

bool AddonList::advance(const double now) {
    const bool gliding = Scroll::advance(now);
    const bool folding = _folds.advance(now);

    _carryY.advance(now);

    if (_dragging) {
        edgeScroll(now);
    }

    if (!_folds.empty()) {
        set_reach(lineTop(State::get().cfg.addonLines.size()));
    }

    invalidate();

    if (_landing > 0.0 && now >= _landing) {
        land();

        return gliding;
    }

    return gliding || folding || _carryY.live() || _landing > 0.0 || _dragging;
}

void AddonList::land() {
    _landing = 0.0;

    if (_carrying < 0) {
        return;
    }

    if (_target != _carrying) {
        _reach->config.lists().moveFile(_carrying, _target);
    }

    _carrying = -1;
    _target = -1;

    _carryY.set(0.0F);

    invalidate();
}

int AddonList::buttonAt(const double x) const {
    const double from = rowEnd() - x;

    return from > 0.0 && from < 4.0 + (BUTTON * BUTTONS) ? std::max(0, static_cast<int>((from - 4.0) / BUTTON)) : -1;
}

double AddonList::rowEnd() const {
    return _box.x + _box.w - (scrollable() ? Theme::lane : 0.0);
}

bool AddonList::overLane(const double x) const {
    return x >= rowEnd();
}

bool AddonList::checkAt(const double x, const double inset) const {
    return x >= _box.x + inset + 24.0 && x < _box.x + inset + 42.0;
}

double AddonList::lineTop(const size_t index) const {
    const State::Cfg &cfg = State::get().cfg;
    const double step = rowHeight();

    if (cfg.fileSections.empty()) {
        return static_cast<double>(index) * step;
    }

    const int section = index < cfg.addonLines.size() ? cfg.addonLines[index].section
                                                      : static_cast<int>(cfg.fileSections.size()) - 1;
    const int headers =
            section + (std::cmp_greater(index, cfg.fileSections[static_cast<size_t>(section)].first) ? 1 : 0);
    double top = (static_cast<double>(index) * step) - (headers * (step - Band::LINE)) + (section * SECTION_GAP);

    // A group sliding open or shut above moves the line up by what it hides.
    for (int at = 0; at < section && !_folds.empty(); ++at) {
        const State::Section &above = cfg.fileSections[static_cast<size_t>(at)];

        top -= body(above) * (1.0 - shown(above));
    }

    return top;
}

int AddonList::lineIn(const double y) const {
    const std::vector<State::Section> &sections = State::get().cfg.fileSections;
    const double step = rowHeight();

    if (sections.empty()) {
        return static_cast<int>(y / step);
    }

    size_t at = 0;

    while (at + 1 < sections.size() && lineTop(static_cast<size_t>(sections[at + 1].first)) - SECTION_GAP <= y) {
        ++at;
    }

    const double rows = lineTop(static_cast<size_t>(sections[at].first)) + Band::LINE;

    return y < rows ? sections[at].first : sections[at].first + 1 + static_cast<int>((y - rows) / step);
}

double AddonList::lineHeight(const size_t index) {
    const State::Cfg &cfg = State::get().cfg;

    return index < cfg.addonLines.size() && cfg.addonLines[index].file < 0 ? Band::LINE : rowHeight();
}

BLRect AddonList::headerBox(const int line) const {
    return BLRect{_box.x, _box.y - offset() + lineTop(static_cast<size_t>(line)), rowEnd() - _box.x, Band::LINE};
}

BLRect AddonList::groupBox(const int at, const double top, const double wide) const {
    const State::Section &section = State::get().cfg.fileSections[static_cast<size_t>(at)];

    return BLRect{_box.x,
                  top + lineTop(static_cast<size_t>(section.first)),
                  wide,
                  Band::LINE + (body(section) * shown(section))};
}

float AddonList::shown(const State::Section &section) const {
    return _folds.open(section.group, section.collapsed);
}

double AddonList::body(const State::Section &section) {
    return section.collapsed ? 0.0 : section.count * rowHeight();
}

int AddonList::lineAt(const double y) const {
    const int count = static_cast<int>(State::get().cfg.addonLines.size());

    return std::clamp(lineIn(y - _box.y + offset()), 0, std::max(0, count - 1));
}

int AddonList::rowAt(const double y) const {
    const double into = y - _box.y + offset();
    const int row = lineIn(into);
    const bool shown = row >= 0 && std::cmp_less(row, State::get().cfg.addonLines.size());

    return shown && into >= lineTop(static_cast<size_t>(row)) ? row : -1;
}

double AddonList::shiftOf(const size_t index, const double step) const {
    const int at = static_cast<int>(index);

    if (_carrying < 0 || at == _carrying) {
        return 0.0;
    }

    if (_carrying < _target && at > _carrying && at <= _target) {
        return -step;
    }

    if (_carrying > _target && at >= _target && at < _carrying) {
        return step;
    }

    return 0.0;
}

}
