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

#include "gui/app/App.h"
#include "gui/components/LogDock.h"
#include "gui/draw/Glyphs.h"
#include "gui/draw/Typeface.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/layout/Scroll.h"
#include "gui/util/Clipboard.h"

namespace {

constexpr double STRIP = 34.0;
constexpr double PANEL = 300.0;

BLRgba32 dotTone(const std::string &status) {
    const Theme::Palette &palette = Theme::of();

    if (status == "launching") {
        return palette.accent;
    }

    if (status == "running") {
        return palette.success;
    }

    if (status == "stopping" || status == "failed") {
        return palette.danger;
    }

    return palette.faint;
}

}

namespace components {

using namespace toolkit;

LogDock::LogDock(App *app) : _app(app) {
    _takesPointer = true;

    _copy = append(std::make_unique<GlyphButton>("extract", [this] {
        Clipboard::write(_app->runs().text());
        _app->notify().success("The output is on the clipboard.");
    }));
    _copy->size(26.0)->tip("Copy all of it");

    _fold = append(std::make_unique<GlyphButton>("down", [this] { _app->runs().hide(); }));
    _fold->size(26.0)->tip("Fold it away");

    _scroll = append(std::make_unique<Scroll>());
}

double LogDock::wanted() {
    if (App::state().runs.docked.empty()) {
        return 0.0;
    }

    return STRIP + (App::state().runs.showing.empty() ? 0.0 : PANEL + 8.0);
}

void LogDock::sync() {
    const State::RunsState &runs = App::state().runs;

    setVisible(!runs.docked.empty());

    // Only what changes the layout is in the mark; a new line or a new state
    // repaints a part of the dock without laying the window out again.
    std::string mark = runs.showing;

    for (const std::string &key : runs.docked) {
        mark += '\n' + key;
    }

    _tabs.clear();

    for (const std::string &key : runs.docked) {
        _tabs.push_back(Tab{
            .key = key,
            .label = _app->runs().titleOf(key),
            .alive = _app->runs().alive(key),
        });
    }

    const bool open = !runs.showing.empty();

    _copy->setVisible(open);
    _fold->setVisible(open);
    _scroll->setVisible(open);
    _copy->setEnabled(!runs.lines.empty());

    // Follows the end until somebody scrolls back.
    if (runs.lines.size() != _lines) {
        _lines = runs.lines.size();

        if (root() != nullptr) {
            _scroll->setReach(static_cast<double>(_lines)
                              * root()->type().lineHeight(
                                  root()->type().at(Typeface::mono, Theme::fontTiny)));
        }

        if (_tailing) {
            _scroll->scrollTo(_scroll->reach());
        }

        _scroll->invalidate();
    }

    if (runs.rev != _rev) {
        _rev = runs.rev;

        // The tabs carry a dot per state, which is all that moved.
        invalidate(BLRect{_box.x, _box.y + _box.h - STRIP, _box.w, STRIP});
    }

    if (mark != _mark) {
        _mark = std::move(mark);

        if (root() != nullptr) {
            root()->relayout();
        }

        invalidate();
    }
}

double LogDock::lineHeight(const Painter &painter) {
    return painter.lineHeight(painter.font(Typeface::mono, Theme::fontTiny));
}

void LogDock::arrange(Typeface &type) {
    const bool open = !App::state().runs.showing.empty();
    const BLRect panel{_box.x, _box.y, _box.w, PANEL};

    if (open) {
        _fold->place(BLRect{panel.x + panel.w - 12.0 - 26.0, panel.y + 12.0, 26.0, 26.0}, type);
        _copy->place(BLRect{panel.x + panel.w - 12.0 - 26.0 - 27.0, panel.y + 12.0, 26.0, 26.0},
                     type);

        const BLRect inner{panel.x + 12.0, panel.y + 50.0, panel.w - 24.0,
                           panel.h - 50.0 - 12.0};

        _scroll->place(BLRect{inner.x + 8.0, inner.y + 8.0, inner.w - 16.0, inner.h - 16.0},
                       type);

        _scroll->setReach(static_cast<double>(App::state().runs.lines.size())
                          * type.lineHeight(type.at(Typeface::mono, Theme::fontTiny)));
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
    const Theme::Palette &palette = Theme::of();
    const State::RunsState &runs = App::state().runs;
    const bool open = !runs.showing.empty();

    if (open) {
        const BLRect panel{_box.x, _box.y, _box.w, PANEL};

        painter.round(panel, Theme::radius, palette.surface);
        painter.outline(panel, Theme::radius, 1.0, palette.border);

        painter.label(painter.font(palette.headingWeight, Theme::fontBody),
                      BLRect{panel.x + 12.0, panel.y + 12.0, panel.w - 24.0 - 60.0, 26.0},
                      Align::Start, _app->runs().titleOf(runs.showing), palette.text);

        const BLRect inner{panel.x + 12.0, panel.y + 50.0, panel.w - 24.0,
                           panel.h - 50.0 - 12.0};

        painter.round(inner, Theme::radius, palette.sunken);
        painter.outline(inner, Theme::radius, 1.0, palette.border);

        const BLFont &face = painter.font(Typeface::mono, Theme::fontTiny);
        const double step = painter.lineHeight(face);
        const BLRect view = _scroll->box();

        painter.push(view);

        double y = view.y - _scroll->offset();

        for (const State::LogRow &row : runs.lines) {
            if (y + step >= view.y && y <= view.y + view.h) {
                painter.label(face, BLRect{view.x, y, view.w, step}, Align::Start, row.line,
                              row.own ? palette.accent : palette.muted);
            }

            y += step;
        }

        painter.pop();
    }

    for (size_t index = 0; index < _tabs.size(); ++index) {
        const Tab &tab = _tabs[index];
        const bool showing = tab.key == runs.showing;
        const std::string status = _app->runs().stateOf(tab.key);

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

        Glyphs::draw(painter.context(), "cross",
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

        const std::string status = _app->runs().stateOf(tab.key);

        if (at.x >= tab.shut.x && at.x < tab.shut.x + tab.shut.w) {
            // A second press forces.
            if (status == "stopping" || !_app->runs().alive(tab.key)) {
                _app->runs().close(tab.key);
            } else {
                _app->ask("Stop " + _app->runs().titleOf(tab.key) + "?",
                          "The game is still running. Closing this takes it with it, and "
                          "anything it has not saved goes.",
                          "Stop it", true,
                          [this, key = tab.key] { _app->runs().close(key); });
            }

            return;
        }

        _tailing = true;

        _app->runs().toggle(tab.key);

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
