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
#include <utility>

#include "ttk/draw/Glyphs.h"
#include "ttk/draw/Typeface.h"
#include "ttk/toolkit/Root.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/layout/Scroll.h"
#include "ttk/util/Clipboard.h"

#include "gui/components/LogDock.h"
#include "gui/state/State.h"

using namespace ttk;

namespace {

constexpr double STRIP = 34.0;
constexpr double PANEL = 300.0;

BLRgba32 dotTone(const State::RunState status) {
    const Theme::Palette &palette = Theme::palette();

    if (status == State::RunState::Launching) {
        return palette.accent;
    }

    if (status == State::RunState::Running) {
        return palette.success;
    }

    if (status == State::RunState::Stopping || status == State::RunState::Failed) {
        return palette.danger;
    }

    return palette.faint;
}

}

namespace components {

LogDock::LogDock(Reach *reach) : _reach(reach) {
    _takesPointer = true;

    set_visible(false);

    _copy = append(std::make_unique<GlyphButton>(Glyphs::Glyph::Extract, [this] {
        Clipboard::write(_reach->runs.text());
        _reach->notify.success("The output is on the clipboard.");
    }));
    _copy->size(26.0)->tooltip("Copy all of it");

    _fold = append(std::make_unique<GlyphButton>(Glyphs::Glyph::Down, [this] { _reach->runs.hide(); }));
    _fold->size(26.0)->tooltip("Fold it away");

    _scroll = append(std::make_unique<Scroll>());

    _output = static_cast<TextView *>(_scroll->hold(std::make_unique<TextView>()));

    _output->face(Typeface::mono, Theme::fontTiny)->ink([](const size_t row) {
        const State::RunsState &runs = State::get().runs;
        const Theme::Palette &palette = Theme::palette();

        return row < runs.lines.size() && runs.lines[row].own ? palette.accent : palette.muted;
    });
}

double LogDock::wanted() const {
    if (_tabs.empty()) {
        return 0.0;
    }

    return STRIP + (_open.empty() ? 0.0 : PANEL + 8.0);
}

void LogDock::sync() {
    const State::RunsState &runs = State::get().runs;

    // Only what changes the layout is in the mark, the label included since the
    // tab is measured from it. A new line or state repaints without a relayout.
    std::string mark = runs.showing;

    for (const std::string &key : runs.docked) {
        mark += '\n' + key + '\t' + _reach->runs.titleOf(key);
    }

    if (mark != _mark) {
        _mark = std::move(mark);

        _open = runs.showing;

        _tabs.clear();

        for (const std::string &key : runs.docked) {
            _tabs.push_back(Tab{.key = key, .label = _reach->runs.titleOf(key)});
        }

        set_visible(!_tabs.empty());

        _copy->set_visible(!_open.empty());
        _fold->set_visible(!_open.empty());
        _scroll->set_visible(!_open.empty());

        if (root() != nullptr) {
            root()->relayout();
        }

        invalidate();
    }

    _copy->set_enabled(!runs.lines.empty());

    // Only what changed since is handed over: the log is thousands of rows, and a
    // batch usually only adds to its end. The view follows the end until somebody
    // scrolls back.
    const bool restarted = runs.showing != _showing || runs.lineGeneration != _generation;
    const size_t gone = restarted ? 0 : std::min(runs.linesDropped - _dropped, _lines);

    if (restarted || gone > 0 || runs.lines.size() != _lines - gone) {
        const size_t from = restarted ? 0 : _lines - gone;

        _showing = runs.showing;
        _generation = runs.lineGeneration;
        _dropped = runs.linesDropped;
        _lines = runs.lines.size();

        std::vector<std::string> rows;

        rows.reserve(runs.lines.size() - from);

        for (size_t row = from; row < runs.lines.size(); ++row) {
            rows.push_back(runs.lines[row].line);
        }

        if (restarted) {
            _output->set_rows(std::move(rows));
        } else {
            _output->drop_rows(gone);
            _output->add_rows(std::move(rows));
        }

        if (root() != nullptr) {
            ttk::Typeface &type = root()->type();

            _scroll->refit(type, static_cast<double>(gone) * _output->row_height(type));

            if (restarted) {
                _scroll->scroll_to(_scroll->reach());
            }
        }
    }

    if (runs.rev != _rev) {
        _rev = runs.rev;

        // The tabs carry a dot per state, which is all that moved.
        invalidate(BLRect{_box.x, _box.y + _box.h - STRIP, _box.w, STRIP});
    }
}

void LogDock::arrange(Typeface &type) {
    const bool open = !_open.empty();
    const BLRect panel{_box.x, _box.y, _box.w, PANEL};

    if (open) {
        _fold->place(BLRect{panel.x + panel.w - 12.0 - 26.0, panel.y + 12.0, 26.0, 26.0}, type);
        _copy->place(BLRect{panel.x + panel.w - 12.0 - 26.0 - 27.0, panel.y + 12.0, 26.0, 26.0},
                     type);

        const BLRect inner{panel.x + 12.0, panel.y + 50.0, panel.w - 24.0,
                           panel.h - 50.0 - 12.0};

        // A view at the end stays at the end through a resize.
        const bool ending = _scroll->at_end();

        _scroll->place(BLRect{inner.x + 8.0, inner.y + 8.0, inner.w - 16.0, inner.h - 16.0},
                       type);

        if (ending) {
            _scroll->scroll_to(_scroll->reach());
        }
    }

    const BLFont &face = type.at(400, Theme::fontSmall);
    const double top = _box.y + _box.h - STRIP;

    double x = _box.x;

    for (Tab &tab : _tabs) {
        const double wide = std::min(240.0, type.width(face, tab.label) + 74.0);

        tab.box = BLRect{x, top, wide, STRIP};
        tab.shut = BLRect{x + wide - 27.0, top + ((STRIP - 22.0) / 2.0), 22.0, 22.0};

        x += wide + 8.0;
    }
}

void LogDock::paint(const Painter &painter) {
    const Theme::Palette &palette = Theme::palette();
    const bool open = !_open.empty();

    if (open) {
        const BLRect panel{_box.x, _box.y, _box.w, PANEL};

        painter.round(panel, Theme::radius, palette.surface);
        painter.outline(panel, Theme::radius, 1.0, palette.border);

        painter.label(painter.font(palette.headingWeight, Theme::fontBody),
                      BLRect{panel.x + 12.0, panel.y + 12.0, panel.w - 24.0 - 60.0, 26.0},
                      Align::Start, _reach->runs.titleOf(_open), palette.text);

        const BLRect inner{panel.x + 12.0, panel.y + 50.0, panel.w - 24.0,
                           panel.h - 50.0 - 12.0};

        painter.round(inner, Theme::radius, palette.sunken);
        painter.outline(inner, Theme::radius, 1.0, palette.border);
    }

    for (size_t index = 0; index < _tabs.size(); ++index) {
        const Tab &tab = _tabs[index];
        const bool showing = tab.key == _open;
        const State::RunState status = _reach->runs.stateOf(tab.key);

        painter.round(tab.box, Theme::radiusSmall,
                      showing                       ? palette.raised
                      : std::cmp_equal(index, _over) ? palette.surface
                                                        : palette.sunken);
        painter.outline(tab.box, Theme::radiusSmall, 1.0,
                        showing ? palette.accent : palette.border);

        painter.circle(BLPoint{tab.box.x + 11.0 + 3.5, tab.box.y + (tab.box.h / 2.0)}, 3.5,
                       dotTone(status));

        painter.label(painter.font(400, Theme::fontSmall),
                      BLRect{tab.box.x + 26.0, tab.box.y, tab.shut.x - tab.box.x - 30.0,
                             tab.box.h},
                      Align::Start, tab.label, showing ? palette.text : palette.muted);

        const double side = Glyphs::span(1.2F);

        Glyphs::draw(painter.context(), Glyphs::Glyph::Close,
                     BLPoint{tab.shut.x + ((tab.shut.w - side) / 2.0),
                             tab.shut.y + ((tab.shut.h - side) / 2.0)},
                     1.2F, _overShut && std::cmp_equal(index, _over) ? palette.danger
                                                                        : palette.muted);
    }

    Widget::paint(painter);
}

bool LogDock::press(const Pointer &at) {
    return at.y >= _box.y + _box.h - STRIP;
}

void LogDock::release(const Pointer &at) {
    for (const Tab &tab : _tabs) {
        if (at.x < tab.box.x || at.x >= tab.box.x + tab.box.w) {
            continue;
        }

        const State::RunState status = _reach->runs.stateOf(tab.key);

        if (at.x >= tab.shut.x && at.x < tab.shut.x + tab.shut.w) {
            // A second press forces.
            if (status == State::RunState::Stopping || !_reach->runs.alive(tab.key)) {
                _reach->runs.close(tab.key);
            } else {
                _reach->ask("Stop " + _reach->runs.titleOf(tab.key) + "?",
                          "The game is still running. Closing this takes it with it, and "
                          "anything it has not saved goes.",
                          "Stop it", true,
                          [this, key = tab.key] { _reach->runs.close(key); });
            }

            return;
        }

        _reach->runs.toggle(tab.key);

        return;
    }
}

void LogDock::hover(const Pointer &at) {
    int over = -1;
    bool shut = false;

    for (size_t index = 0; index < _tabs.size(); ++index) {
        const Tab &tab = _tabs[index];

        if (at.x >= tab.box.x && at.x < tab.box.x + tab.box.w && at.y >= tab.box.y) {
            over = static_cast<int>(index);
            shut = at.x >= tab.shut.x && at.x < tab.shut.x + tab.shut.w;
        }
    }

    if (over != _over || shut != _overShut) {
        _over = over;
        _overShut = shut;

        invalidate();
    }
}

void LogDock::leave() {
    Widget::leave();

    _over = -1;
    _overShut = false;
}

}
