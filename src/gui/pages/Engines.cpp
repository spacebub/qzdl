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

#include "core/config/Session.h"
#include "gui/app/App.h"
#include "gui/draw/Glyphs.h"
#include "gui/pages/Engines.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/Fact.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/controls/Segmented.h"
#include "gui/toolkit/layout/Box.h"
#include "gui/toolkit/layout/Panel.h"
#include "gui/toolkit/layout/Scroll.h"
#include "gui/toolkit/layout/Spacer.h"
#include "gui/util/Desktop.h"
#include "gui/util/Format.h"

namespace {

constexpr double BLEED = 16.0;
constexpr double GUTTER = 16.0;
constexpr double NARROWEST = 330.0;
constexpr double INSTALLED_ROW = 152.0;
constexpr double BROWSE_ROW = 186.0;
constexpr double SETTLING = 0.19;
constexpr double SLACK = 6.0;

std::string say(const size_t number, const std::string &thing) {
    return std::to_string(number) + " " + thing + (number == 1 ? "" : "s");
}

}

namespace pages {

using namespace toolkit;

// One port on either shelf.
class EnginesPage::Card : public Panel {
public:
    Card() {
        _takesPointer = true;
        hoverable = true;

        _row = append(Box::row());
        _row->spacing(8.0);
    }

    // The installed shelf carries the drag; the browse shelf does not.
    bool draggable = false;

    std::function<void()> pressedDown;
    std::function<void(double, double)> dragStarted;
    std::function<void(double, double)> dragMoved;
    std::function<void()> dragEnded;
    std::function<void()> opened;

    std::string name;
    std::string blurb;
    std::string file;
    std::string status;

    // The pills along the top, and what each of them says when rested on.
    std::vector<State::BadgeSpec> tags;
    std::vector<std::string> tagHints;

    // Under the buttons: what is known, or what went wrong.
    std::string told;
    bool trouble = false;

    // Shown as a bar instead, while something is being fetched.
    bool working = false;
    double progress = 0.0;

    [[nodiscard]] Box *buttons() const { return _row; }

    void arrange(Typeface &type) override {
        _row->place(BLRect{_box.x + 16.0, _box.y + _box.h - 16.0 - Theme::controlSmall,
                           _box.w - 32.0, Theme::controlSmall},
                    type);
    }

    void paint(const Painter &painter) override {
        lit = holdsPointer() || _carrying;

        Panel::paint(painter);

        const Theme::Palette &palette = Theme::of();
        const BLFont &face = painter.font(600, Theme::fontMedium);

        double right = _box.x + _box.w - 16.0;

        for (auto tag = tags.rbegin(); tag != tags.rend(); ++tag) {
            const BLRgba32 tone = toneOf(tag->kind);
            const BLRgba32 wash = washOf(tag->kind);
            const BLRgba32 ink = palette.dark ? tone : Theme::darker(tone, 0.35);
            const BLFont &small = painter.font(600, Theme::fontSmall);

            double wide = painter.width(small, tag->text) + 22.0;

            if (tag->dot) {
                wide += 14.0;
            }

            const BLRect pill{right - wide, _box.y + 16.0, wide, 22.0};
            const size_t which = tags.size() - 1
                - static_cast<size_t>(tag - tags.rbegin());

            if (_pills.size() != tags.size()) {
                _pills.assign(tags.size(), BLRect{});
            }

            _pills[which] = pill;

            painter.round(pill, 11.0, wash);
            painter.outline(pill, 11.0, 1.0, Theme::alpha(ink, 0.3));

            double x = pill.x + 11.0;

            if (tag->dot) {
                painter.circle(BLPoint{x + 4.0, pill.y + (pill.h / 2.0)}, 4.0, ink);

                x += 14.0;
            }

            painter.label(small, BLRect{x, pill.y, pill.x + pill.w - 11.0 - x, pill.h},
                          Align::Start, tag->text, ink);

            right -= wide + 8.0;
        }

        painter.label(face, BLRect{_box.x + 16.0, _box.y + 16.0, right - _box.x - 24.0, 22.0},
                      Align::Start, name,
                      trouble && status == "missing" ? palette.danger : palette.text);

        if (!blurb.empty()) {
            painter.paragraph(painter.font(400, Theme::fontSmall),
                              BLRect{_box.x + 16.0, _box.y + 46.0, _box.w - 32.0, 0.0}, blurb,
                              palette.faint);
        }

        if (!file.empty()) {
            const BLFont &mono = painter.font(Typeface::mono, Theme::fontTiny);
            const double unit = painter.width(mono, "M");
            const int room = unit > 0.0 ? static_cast<int>((_box.w - 32.0) / unit) : 0;

            painter.label(mono, BLRect{_box.x + 16.0, _box.y + 46.0, _box.w - 32.0, 18.0},
                          Align::Start, Format::fitPath(file, room), palette.faint);
        }

        const double line = _row->box().y - 26.0;

        if (working) {
            const BLRect track{_box.x + 16.0, line + 5.0, _box.w - 32.0, 6.0};

            painter.round(track, 3.0, palette.sunken);
            painter.round(BLRect{track.x, track.y, track.w * std::clamp(progress, 0.0, 1.0),
                                 track.h},
                          3.0, palette.accent);
        } else if (!told.empty()) {
            painter.label(painter.font(400, Theme::fontSmall),
                          BLRect{_box.x + 16.0, line, _box.w - 32.0, 16.0}, Align::Start, told,
                          trouble ? palette.danger : palette.faint);
        }
    }

    bool press(const Pointer &at) override {
        _pressX = at.x;
        _pressY = at.y;
        _carrying = false;
        _armed = true;

        if (pressedDown) {
            pressedDown();
        }

        return true;
    }

    void drag(const Pointer &at) override {
        if (!draggable || !_armed) {
            return;
        }

        if (!_carrying) {
            if (std::abs(at.x - _pressX) < SLACK && std::abs(at.y - _pressY) < SLACK) {
                return;
            }

            _carrying = true;

            if (dragStarted) {
                dragStarted(_pressX, _pressY);
            }
        }

        if (dragMoved) {
            dragMoved(at.x, at.y);
        }
    }

    void release(const Pointer &at) override {
        const bool carried = _carrying;

        _armed = false;
        _carrying = false;

        if (carried) {
            if (dragEnded) {
                dragEnded();
            }

            return;
        }

        if (holds(at.x, at.y) && opened && at.y < _row->box().y) {
            opened();
        }
    }

    [[nodiscard]] bool carrying() const { return _carrying; }

    // Nothing on a card is a link; an installed one is only carried.
    [[nodiscard]] Cursor cursorAt(double /*x*/, double /*y*/) const override {
        return _carrying ? Cursor::Grabbing : Cursor::Default;
    }

    void hover(const Pointer &at) override {
        std::string said;

        for (size_t which = 0; which < _pills.size() && which < tagHints.size(); ++which) {
            const BLRect &pill = _pills[which];

            if (at.x >= pill.x && at.x < pill.x + pill.w && at.y >= pill.y
                && at.y < pill.y + pill.h) {
                said = tagHints[which];

                break;
            }
        }

        hint = said;
    }

    void leave() override {
        Panel::leave();

        hint.clear();
    }

    // The buttons are on the card: the light stays while the pointer is on them.
    void within(bool /*inside*/) override {
        invalidate();
    }

    // The place in the grid it was last given. A card walks to a new one; it does
    // not walk because the grid scrolled or the window changed size.
    [[nodiscard]] int slot() const { return _slot; }

    void setSlot(const int at) { _slot = at; }

    // The walk to a new place, while the cards shuffle around a carried one.
    void slideFrom(const double x, const double y, const double now) {
        _slideX.set(static_cast<float>(x));
        _slideY.set(static_cast<float>(y));

        _slideX.run(0.0F, now, 0.19, Anim::Curve::CubicOut);
        _slideY.run(0.0F, now, 0.19, Anim::Curve::CubicOut);

        animate();
    }

    [[nodiscard]] double slideX() const { return _slideX.value(); }
    [[nodiscard]] double slideY() const { return _slideY.value(); }

    [[nodiscard]] bool sliding() const { return _slideX.live() || _slideY.live(); }

    bool advance(const double now) override {
        // The slide is in the box here, so the shelf lays it out again; what it was
        // has to be damaged before that, since the live list is not ordered.
        const BLRect was = _box;

        _slideX.advance(now);
        _slideY.advance(now);

        if (sliding()) {
            invalidate(was);
            invalidate();
        }

        return sliding();
    }

private:
    static BLRgba32 toneOf(const std::string &kind) {
        const Theme::Palette &palette = Theme::of();

        if (kind == "danger") {
            return palette.danger;
        }

        if (kind == "success") {
            return palette.success;
        }

        if (kind == "warning") {
            return palette.warning;
        }

        return kind == "muted" ? palette.muted : palette.accent;
    }

    static BLRgba32 washOf(const std::string &kind) {
        const Theme::Palette &palette = Theme::of();

        if (kind == "danger") {
            return palette.dangerSoft;
        }

        if (kind == "success") {
            return palette.successSoft;
        }

        if (kind == "warning") {
            return palette.warningSoft;
        }

        return kind == "muted" ? palette.mutedSoft : palette.accentSoft;
    }

    Box *_row = nullptr;

    double _pressX = 0.0;
    double _pressY = 0.0;

    bool _armed = false;
    bool _carrying = false;

    int _slot = -1;

    // Where the pills were last drawn, so one can be rested on.
    std::vector<BLRect> _pills;

    Anim::Tween _slideX;
    Anim::Tween _slideY;
};

// The cards, the tile that adds one, and the note under the browse shelf.
class EnginesPage::Shelf : public Widget {
public:
    explicit Shelf(EnginesPage *view) : _view(view) { _takesPointer = true; }

    // The shelf owns the grid, so the scroller moves it.
    double naturalHeight(Typeface & /*type*/, const double width) override {
        _view->measure(width);

        const int count = static_cast<int>(_view->_cards.size());
        const bool here = EnginesPage::installed();
        const int rows = here ? (count + _view->_columns) / _view->_columns
                              : (count + _view->_columns - 1) / _view->_columns;

        const double shelf = rows * (_view->_rowHeight + GUTTER);

        return here ? shelf + 26.0 : shelf + 2.0 + 76.0 + 34.0;
    }

    void arrange(Typeface &type) override {
        _view->measure(_box.w);

        const int count = static_cast<int>(_view->_cards.size());

        for (int index = 0; index < count; ++index) {
            Card *card = _view->_cards[static_cast<size_t>(index)];
            const int at = _view->slot(index);

            const double carryX = index == _view->_origin ? _view->_carryX.value() : 0.0;
            const double carryY = index == _view->_origin ? _view->_carryY.value() : 0.0;

            const BLRect was = card->box();
            const BLRect cell{_view->cellX(at), _view->cellY(at), _view->_cell,
                              _view->_rowHeight};

            // The walk is between two places in the grid, worked out in the grid as
            // it stands now: comparing against where the card was drawn would make
            // a scroll, which moves every cell, look like a reorder.
            if (const int was_at = card->slot();
                index != _view->_origin && was_at >= 0 && was_at != at) {
                card->slideFrom(_view->cellX(was_at) - cell.x, _view->cellY(was_at) - cell.y,
                                card->now());
            }

            card->setSlot(at);

            const BLRect now{cell.x + carryX + card->slideX(),
                             cell.y + carryY + card->slideY(), cell.w, cell.h};

            card->place(now, type);

            if (was.x != now.x || was.y != now.y || was.w != now.w) {
                card->invalidate(was);
                card->invalidate(now);
            }
        }

        if (_view->_kept != nullptr) {
            const double room = std::max(NARROWEST, _box.w - (BLEED * 2.0));
            const int rows = (count + _view->_columns - 1) / _view->_columns;

            _view->_kept->parent()->parent()->place(
                BLRect{_box.x + BLEED, _box.y + (rows * (_view->_rowHeight + GUTTER)) + 20.0, room,
                       76.0},
                type);
        }
    }

    void paint(const Painter &painter) override {
        if (EnginesPage::installed()) {
            paintAdder(painter);
        }

        const int carried = _view->_origin;

        for (const Ptr &child : children()) {
            if (carried >= 0 && std::cmp_less(carried, _view->_cards.size())
                && child.get() == _view->_cards[static_cast<size_t>(carried)]) {
                continue;
            }

            if (painter.needed(child->box())) {
                child->paint(painter);
            }
        }

        // Last, so a card in hand is over the ones it is being carried past.
        if (carried >= 0 && std::cmp_less(carried, _view->_cards.size())) {
            _view->_cards[static_cast<size_t>(carried)]->paint(painter);
        }
    }

    void hover(const Pointer &at) override {
        const BLRect adder = _view->adderBox();
        const bool over = EnginesPage::installed() && at.x >= adder.x && at.x < adder.x + adder.w
            && at.y >= adder.y && at.y < adder.y + adder.h;

        if (over != _lit) {
            _lit = over;

            invalidate(adder);
        }
    }

    void leave() override {
        Widget::leave();

        _lit = false;
    }

    bool press(const Pointer &at) override {
        const BLRect adder = _view->adderBox();

        return EnginesPage::installed() && at.x >= adder.x && at.x < adder.x + adder.w
            && at.y >= adder.y && at.y < adder.y + adder.h;
    }

    void release(const Pointer &at) override {
        const BLRect adder = _view->adderBox();

        if (EnginesPage::installed() && at.x >= adder.x && at.x < adder.x + adder.w && at.y >= adder.y
            && at.y < adder.y + adder.h) {
            _view->_app->picker().open("add-port", "Add a source port", App::portFilters(),
                                       false, false, false, "src", "DOS program",
                                       "It is started inside DOSBox instead of being run as it "
                                       "is");
        }
    }

private:
    void paintAdder(const Painter &painter) const {
        const Theme::Palette &palette = Theme::of();
        const BLRect box = _view->adderBox();

        if (!painter.needed(box)) {
            return;
        }

        painter.round(box, Theme::radius,
                      _lit ? Theme::mix(palette.sunken, palette.hover, 0.85) : palette.sunken);
        painter.outline(box, Theme::radius, 1.0, _lit ? palette.accent : palette.border);

        const double side = Glyphs::span(1.6F);

        Glyphs::draw(painter.context(), "plus",
                     BLPoint{box.x + ((box.w - side) / 2.0), box.y + 34.0}, 1.6F,
                     _lit ? palette.accent : palette.faint);

        painter.label(painter.font(600, Theme::fontBody),
                      BLRect{box.x + 20.0, box.y + 62.0, box.w - 40.0, 20.0}, Align::Centre,
                      "Add one you already have", _lit ? palette.text : palette.muted);

        painter.paragraph(painter.font(400, Theme::fontSmall),
                          BLRect{box.x + 20.0, box.y + 88.0, box.w - 40.0, 0.0},
                          "Point ZDL4 at a source port on this machine. It can be marked as a "
                          "DOS program while it is picked.",
                          palette.faint);
    }

    EnginesPage *_view;

    bool _lit = false;
};

EnginesPage::EnginesPage(App *app) : _app(app) {
    Box *column = append(Box::column());

    column->spacing(16.0);

    Box *header = column->append(Box::row());

    header->fixedHeight = 52.0;
    header->cross(Box::Place::Centre);
    header->pad(BLEED, 0.0, BLEED, 0.0);

    Box *head = header->append(Box::column());

    head->spacing(3.0)->align(Box::Place::Centre);
    head->stretch = 1.0;

    _title = head->append(std::make_unique<Label>("Engines"));
    _title->font(Theme::of().headingWeight, Theme::fontDisplay)->tone(Theme::of().text);

    _note = head->append(std::make_unique<Label>());
    _note->font(400, Theme::fontSmall)->tone(Theme::of().faint);

    Box *tools = header->append(Box::row());

    tools->spacing(14.0)->cross(Box::Place::Centre);

    _recheck = tools->append(std::make_unique<Button>("Check again", [this] {
        _app->engines().refresh(true);
    }));

    _recheck->glyph("refresh")->compact()
        ->tip("Ask every project what it has released. GitHub takes only so many questions an "
              "hour, which is why nothing is asked again on its own");

    _which = tools->append(std::make_unique<Segmented>([this](const std::string &key) {
        State::get().nav.engines = key;

        _app->touch();
    }));

    _which->setOptions({{.key = "installed", .label = "Installed"},
                        {.key = "browse", .label = "Get more"}});

    _scroll = column->append(std::make_unique<Scroll>());
    _scroll->stretch = 1.0;

    _grid = static_cast<Shelf *>(_scroll->hold(std::make_unique<Shelf>(this)));
    _grid->minWidth = NARROWEST + (BLEED * 2.0);

    // Where to start, not where to stay.
    if (Session::get().config().ports.empty()) {
        State::get().nav.engines = "browse";
    }
}

bool EnginesPage::installed() {
    return State::get().nav.engines == "installed";
}

void EnginesPage::measure(const double width) {
    const double room = std::max(NARROWEST, width - (BLEED * 2.0));

    _columns = std::max(1, static_cast<int>(std::floor((room + GUTTER) / (NARROWEST + GUTTER))));
    _cell = (room - ((_columns - 1) * GUTTER)) / _columns;
    _rowHeight = installed() ? INSTALLED_ROW : BROWSE_ROW;
}

double EnginesPage::cellX(const int index) const {
    return _grid->box().x + BLEED + ((index % _columns) * (_cell + GUTTER));
}

double EnginesPage::cellY(const int index) const {
    const int row = index / _columns;

    return _grid->box().y + 2.0 + (row * (_rowHeight + GUTTER));
}

int EnginesPage::placeAt(const double x, const double y) const {
    const int count = static_cast<int>(_cards.size());

    if (count == 0) {
        return 0;
    }

    const int row = static_cast<int>(
        std::floor((y - _grid->box().y - 2.0) / (_rowHeight + GUTTER)));
    const int column = std::clamp(
        static_cast<int>(std::floor((x - _grid->box().x - BLEED) / (_cell + GUTTER))), 0,
        _columns - 1);

    return std::clamp((row * _columns) + column, 0, count - 1);
}

int EnginesPage::slot(const int index) const {
    if (_origin < 0 || index == _origin) {
        return index;
    }

    if (_origin < _target && index > _origin && index <= _target) {
        return index - 1;
    }

    if (_origin > _target && index >= _target && index < _origin) {
        return index + 1;
    }

    return index;
}

void EnginesPage::grabbed(const int index, const double x, const double y) {
    _dragging = true;
    _origin = index;
    _target = index;
    _grabX = x - cellX(index);
    _grabY = y - cellY(index);

    _carryX.set(0.0F);
    _carryY.set(0.0F);
}

void EnginesPage::carried(const double x, const double y) {
    _target = placeAt(x, y);

    _carryX.set(static_cast<float>(x - _grabX - cellX(_origin)));
    _carryY.set(static_cast<float>(y - _grabY - cellY(_origin)));

    animate();
}

void EnginesPage::dropped() {
    _dragging = false;

    if (_origin < 0) {
        return;
    }

    _carryX.run(static_cast<float>(cellX(_target) - cellX(_origin)), now(), SETTLING,
                Anim::Curve::CubicOut);
    _carryY.run(static_cast<float>(cellY(_target) - cellY(_origin)), now(), SETTLING,
                Anim::Curve::CubicOut);

    _landing = now() + SETTLING + 0.02;

    animate();
}

void EnginesPage::land() {
    _landing = 0.0;

    if (_origin < 0) {
        return;
    }

    if (_target != _origin) {
        _app->config().lists().movePort(_origin, _target);
    }

    _origin = -1;
    _target = -1;
    _carrying.clear();

    _carryX.set(0.0F);
    _carryY.set(0.0F);

    invalidate();
}

BLRect EnginesPage::adderBox() const {
    const int count = static_cast<int>(_cards.size());

    return BLRect{cellX(count), cellY(count), _cell, _rowHeight};
}

void EnginesPage::rebuild() {
    _grid->clear();
    _cards.clear();
    _kept = nullptr;

    const bool here = installed();

    _rowHeight = here ? INSTALLED_ROW : BROWSE_ROW;

    if (here) {
        const std::vector<State::NameRow> &ports = App::state().cfg.ports;

        for (size_t index = 0; index < ports.size(); ++index) {
            const State::NameRow &port = ports[index];
            Card *card = _grid->append(std::make_unique<Card>());

            card->draggable = true;
            card->name = port.name;
            card->file = port.file;
            card->status = port.missing ? "missing" : "";
            card->trouble = port.missing;

            if (port.dosbox) {
                card->tags.push_back(State::BadgeSpec{"DOS", "muted", false});
                card->tagHints.emplace_back();
            }

            if (port.fetched) {
                card->tags.push_back(State::BadgeSpec{"Managed", "", false});
                card->tagHints.emplace_back("Downloaded and updated by ZDL4");
            }

            if (port.detected && !port.missing) {
                card->tags.push_back(State::BadgeSpec{"Detected", "success", false});
                card->tagHints.emplace_back();
            }

            if (port.missing) {
                card->tags.push_back(State::BadgeSpec{"Missing", "danger", true});
                card->tagHints.emplace_back();
            }

            const int at = static_cast<int>(index);
            const std::string name = port.name;
            const std::string file = port.file;
            const bool dosbox = port.dosbox;
            const bool fetched = port.fetched;

            const auto editing = [this, at, name, file, dosbox] {
                _app->edit("Edit " + name, "port", App::portFilters(), "src", name, file,
                           true, dosbox,
                           [this, at](const std::string &named, const std::string &path,
                                      const bool dos) {
                               _app->config().lists().updatePort(at, named, path, dos);
                           });
            };

            card->opened = editing;

            card->buttons()->append(std::make_unique<Button>("Edit", editing))
                ->glyph("edit")->compact()->tip("Rename it or point it at another file");

            GlyphButton *folder = card->buttons()->append(
                std::make_unique<GlyphButton>("folder", [file] {
                    Desktop::open(Format::directoryOf(file));
                }));

            folder->outlined()->tip("Open the directory it is in");
            folder->fixedWidth = Theme::controlSmall;

            GlyphButton *bin = card->buttons()->append(
                std::make_unique<GlyphButton>("trash", [this, at, name, fetched] {
                    _app->ask("Remove \"" + name + "\"?",
                              fetched ? "Everything ZDL4 unpacked for it is deleted and it goes "
                                        "out of every profile that named it."
                                      : "It goes out of this list and out of every profile that "
                                        "named it. The file itself is left where it is.",
                              "Remove", true,
                              [this, at] { _app->engines().forget(at); });
                }));

            bin->outlined()->tone(Theme::of().muted, Theme::of().danger)
                ->tip(fetched ? "Delete what was fetched and take it out of the list"
                              : "Take it out of the list. The file itself is left where it is");
            bin->fixedWidth = Theme::controlSmall;

            card->buttons()->append(std::make_unique<Spacer>());

            card->pressedDown = [this] { land(); };

            card->dragStarted = [this, index, name](const double x, const double y) {
                _carrying = name;

                grabbed(static_cast<int>(index), x, y);
            };

            card->dragMoved = [this](const double x, const double y) { carried(x, y); };
            card->dragEnded = [this] { dropped(); };

            _cards.push_back(card);
        }

        return;
    }

    const std::vector<State::EngineRow> &rows = App::state().ports.rows;

    for (size_t index = 0; index < rows.size(); ++index) {
        const State::EngineRow &row = rows[index];
        Card *card = _grid->append(std::make_unique<Card>());

        const bool working = row.status == "fetching" || row.status == "unpacking";
        const bool present = row.status == "installed";
        const bool behind = present && !row.have.empty() && !row.version.empty()
            && row.version != row.have;

        card->name = row.name;
        card->blurb = row.blurb;
        card->working = working;
        card->progress = row.status == "unpacking" ? 1.0 : row.progress;
        card->trouble = row.status == "failed";

        if (row.dos) {
            card->tags.push_back(State::BadgeSpec{"DOS", "muted", false});
        }

        if (row.status != "ready" && row.status != "waiting" && !working) {
            card->tags.push_back(State::BadgeSpec{
                behind                      ? "Update"
                : present                   ? "Installed"
                : row.status == "checking"  ? "Checking"
                : row.status == "failed"    ? "Failed"
                : row.status == "elsewhere" ? "Its own site"
                                            : "No downloads available",
                behind                     ? ""
                : present                  ? "success"
                : row.status == "failed"   ? "danger"
                : row.status == "checking" ? ""
                                           : "warning",
                false});
        }

        card->told = !row.error.empty()             ? row.error
                   : row.status == "elsewhere"      ? "Fetched from its own site"
                   : behind                         ? row.have + " here · " + row.version
                                                          + " released"
                   : present                        ? (row.have.empty() ? std::string()
                                                                        : row.have + " here")
                   : row.version.empty()            ? std::string()
                   : !row.sizeText.empty()          ? row.version + " · " + row.sizeText
                                                    : row.version;

        const int at = static_cast<int>(index);
        const std::string name = row.name;
        const std::string file = row.file;
        const std::string homepage = row.homepage;

        if (working) {
            card->buttons()->append(std::make_unique<Button>("Stop", [this, at] {
                _app->engines().cancel(at);
            }))->glyph("cross")->compact();
        } else if (row.status != "elsewhere" && row.status != "unavailable") {
            Button *fetch = card->buttons()->append(
                std::make_unique<Button>(behind      ? "Update"
                                         : present   ? "Fetch again"
                                                     : "Install",
                                         [this, at] { _app->engines().install(at); }));

            fetch->glyph("download")->compact()
                ->kind(present && !behind ? Button::Kind::Default : Button::Kind::Primary)
                ->busy(row.asking)
                ->tip(present ? "Fetch the latest build over the one that is here"
                              : "Fetch it and add it to the source ports");
        }

        if (present && !working) {
            GlyphButton *folder = card->buttons()->append(
                std::make_unique<GlyphButton>("folder", [file] {
                    Desktop::open(Format::directoryOf(file));
                }));

            folder->outlined()->tip("Open the directory it was unpacked into");
            folder->fixedWidth = Theme::controlSmall;

            GlyphButton *bin = card->buttons()->append(
                std::make_unique<GlyphButton>("trash", [this, at, name] {
                    _app->ask("Remove " + name + "?",
                              "Everything ZDL4 unpacked for it is deleted and it is taken out "
                              "of the source ports. Profiles pointing at it are left without a "
                              "port.",
                              "Remove it", true, [this, at] { _app->engines().remove(at); });
                }));

            bin->outlined()->tone(Theme::of().muted, Theme::of().danger)
                ->tip("Delete what was fetched and take it out of the port list");
            bin->fixedWidth = Theme::controlSmall;
        }

        if (!present || working) {
            card->buttons()->append(std::make_unique<Button>("Project page", [homepage] {
                Desktop::open(homepage);
            }))->kind(Button::Kind::Ghost)->compact();
        }

        card->buttons()->append(std::make_unique<Spacer>());

        _cards.push_back(card);
    }

    Panel *where = _grid->append(std::make_unique<Panel>());

    where->fixedHeight = 76.0;

    Box *inside = where->append(Box::column());

    inside->pad(16.0, 0.0)->align(Box::Place::Centre);

    _kept = inside->append(std::make_unique<Fact>("Where they are kept",
                                                  App::state().ports.directory));
    _kept->path()->onClick("Open the directory",
                           [] { Desktop::open(App::state().ports.directory); });
}

void EnginesPage::sync() {
    const bool here = installed();

    _which->setCurrent(State::get().nav.engines);

    _recheck->setVisible(!here);
    _recheck->busy(App::state().ports.checking);

    _note->setText(here ? say(App::state().cfg.ports.size(), "source port")
                              + " · what the profiles are run with"
                   : !App::state().ports.trouble.empty()
                       ? App::state().ports.trouble
                       : "Fetched, unpacked and set up here · what each project has released is "
                         "looked up on GitHub");

    _note->tone(!here && !App::state().ports.trouble.empty() ? Theme::of().danger
                                                             : Theme::of().faint);

    if (!here && !_asked) {
        _asked = true;

        _app->engines().refresh(false);
    }

    // Rebuilt only when what the cards are made of has moved.
    std::string mark = State::get().nav.engines + '\n' + std::to_string(App::state().cfg.rev);

    if (here) {
        for (const State::NameRow &port : App::state().cfg.ports) {
            mark += '\n' + port.name + '\t' + port.file + (port.missing ? "\tgone" : "");
        }
    } else {
        for (const State::EngineRow &row : App::state().ports.rows) {
            mark += '\n' + row.name + '\t' + row.status + '\t' + row.version + '\t' + row.have
                + '\t' + row.error + (row.asking ? "\task" : "");
        }
    }

    if (mark == _mark) {
        // A moving bar is the one thing that changes without rebuilding.
        for (size_t index = 0; index < _cards.size() && !here; ++index) {
            const State::EngineRow &row = App::state().ports.rows[index];

            if (_cards[index]->working) {
                _cards[index]->progress = row.status == "unpacking" ? 1.0 : row.progress;

                _cards[index]->invalidate();
            }
        }

        return;
    }

    _mark = std::move(mark);

    rebuild();

    if (root() != nullptr) {
        root()->relayout();
    }
}

bool EnginesPage::advance(const double now) {
    _carryX.advance(now);
    _carryY.advance(now);

    if (root() != nullptr) {
        _grid->arrange(root()->type());
    }

    if (_landing > 0.0 && now >= _landing) {
        land();

        return false;
    }

    if (_carryX.live() || _carryY.live() || _landing > 0.0 || _dragging) {
        return true;
    }

    // A neighbour still walking to its gap needs the grid laid out under it.
    return std::ranges::any_of(_cards, [](const Card *card) { return card->sliding(); });
}

}
