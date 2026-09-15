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
#include "gui/services/Filters.h"
#include "gui/components/Parts.h"
#include "gui/components/EngineCard.h"
#include "gui/draw/Glyphs.h"
#include "gui/pages/EnginesPage.h"
#include "gui/state/State.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/Fact.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/controls/MultistateSwitch.h"
#include "gui/toolkit/layout/Box.h"
#include "gui/toolkit/layout/Panel.h"
#include "gui/toolkit/layout/Scroll.h"
#include "gui/toolkit/layout/Spacer.h"
#include "gui/util/Desktop.h"
#include "gui/util/Format.h"

namespace {

// The engine cards sit closer together than the library's, which is why this is not
// Theme::gutter.
constexpr double GUTTER = 16.0;
constexpr double NARROWEST = 330.0;

// The gap above the first row.
constexpr double TOP = 2.0;
constexpr double INSTALLED_ROW = 152.0;
constexpr double BROWSE_ROW = 186.0;

}

namespace pages {

using namespace toolkit;


// The cards, the tile that adds one, and the note under the browse shelf.
class EnginesPage::Shelf : public Widget {
public:
    explicit Shelf(EnginesPage *view) : _view(view) { _takesPointer = true; }

    // The browse cards carry a line more than the installed ones.
    [[nodiscard]] static double rowHeight() {
        return installed() ? INSTALLED_ROW : BROWSE_ROW;
    }

    // The shelf owns the grid, so the scroller moves it.
    double naturalHeight(Typeface & /*type*/, const double width) override {
        _view->_reorder.setRowHeight(rowHeight());
        _view->_reorder.measure(width);

        const int count = static_cast<int>(_view->_cards.size());
        const bool here = installed();
        const int rows = here ? _view->_reorder.rowsWithAdder(count)
                              : _view->_reorder.rowsFor(count);

        const double shelf = rows * (_view->_reorder.rowHeight() + GUTTER);

        return here ? shelf + 26.0 : shelf + 2.0 + 76.0 + 34.0;
    }

    void arrange(Typeface &type) override {
        _view->_reorder.setRowHeight(rowHeight());
        _view->_reorder.place(_box);

        const int count = static_cast<int>(_view->_cards.size());

        for (int index = 0; index < count; ++index) {
            components::EngineCard *card = _view->_cards[static_cast<size_t>(index)];
            const int at = _view->_reorder.slot(index);

            const double carryX = index == _view->_reorder.origin() ? _view->_reorder.carryX() : 0.0;
            const double carryY = index == _view->_reorder.origin() ? _view->_reorder.carryY() : 0.0;

            const BLRect was = card->box();
            const BLRect cell{_view->_reorder.cellX(at), _view->_reorder.cellY(at),
                              _view->_reorder.cell(), _view->_reorder.rowHeight()};

            // The walk is between two places in the grid, worked out in the grid as
            // it stands now: comparing against where the card was drawn would make
            // a scroll, which moves every cell, look like a reorder.
            if (const int was_at = card->slot();
                index != _view->_reorder.origin() && was_at >= 0 && was_at != at) {
                card->slideFrom(_view->_reorder.cellX(was_at) - cell.x,
                                _view->_reorder.cellY(was_at) - cell.y,
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
            const double room = std::max(NARROWEST, _box.w - (Theme::bleed * 2.0));
            const int rows = _view->_reorder.rowsFor(count);

            _view->_kept->parent()->parent()->place(
                BLRect{_box.x + Theme::bleed, _box.y + (rows * (_view->_reorder.rowHeight() + GUTTER)) + 20.0, room,
                       76.0},
                type);
        }
    }

    void paint(const Painter &painter) override {
        if (installed()) {
            paintAdder(painter);
        }

        const int carried = _view->_reorder.origin() >= 0 ? _view->_reorder.origin()
                                                          : _view->_settling;

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
        const bool over = installed() && at.x >= adder.x && at.x < adder.x + adder.w
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

        return installed() && at.x >= adder.x && at.x < adder.x + adder.w
            && at.y >= adder.y && at.y < adder.y + adder.h;
    }

    void release(const Pointer &at) override {
        const BLRect adder = _view->adderBox();

        if (installed() && at.x >= adder.x && at.x < adder.x + adder.w && at.y >= adder.y
            && at.y < adder.y + adder.h) {
            _view->_reach->picker.open(FilePicker::Action::AddPort, "Add a source port", Filters::port(),
                                       false, false, false, FilePicker::Slot::Src, "DOS program",
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

        Glyphs::draw(painter.context(), Glyphs::Glyph::Plus,
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

EnginesPage::EnginesPage(Reach *reach) : _reach(reach) {
    _reorder.setMetrics(ReorderGrid::Metrics{.bleed = Theme::bleed,
                                             .gutter = GUTTER,
                                             .top = TOP,
                                             .narrowest = NARROWEST,
                                             .rowHeight = INSTALLED_ROW,
                                             .step = Theme::cardStep});

    Box *column = append(Box::column());

    column->spacing(16.0);

    Box *header = column->append(Box::row());

    header->fixedHeight = 52.0;
    header->cross(Box::Place::Centre);
    header->pad(Theme::bleed, 0.0, Theme::bleed, 0.0);

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
        _reach->engines.refresh(true);
    }));

    _recheck->glyph(Glyphs::Glyph::Refresh)->compact()
        ->tooltip("Ask every project what it has released. GitHub takes only so many questions an "
              "hour, which is why nothing is asked again on its own");

    _which = tools->append(std::make_unique<MultistateSwitch>([this](const int value) {
        State::get().nav.engines = static_cast<State::EnginesTab>(value);

        _reach->touch();
    }));

    _which->setOptions(
        {{.value = static_cast<int>(State::EnginesTab::Installed), .label = "Installed"},
         {.value = static_cast<int>(State::EnginesTab::Browse), .label = "Get more"}});

    _scroll = column->append(std::make_unique<Scroll>());
    _scroll->stretch = 1.0;

    _grid = static_cast<Shelf *>(_scroll->hold(std::make_unique<Shelf>(this)));
    _grid->minWidth = NARROWEST + (Theme::bleed * 2.0);

    // Where to start, not where to stay.
    if (Session::get().config().ports.empty()) {
        State::get().nav.engines = State::EnginesTab::Browse;
    }
}

bool EnginesPage::installed() {
    return State::get().nav.engines == State::EnginesTab::Installed;
}

void EnginesPage::land() {
    const int from = _reorder.origin();
    const int to = _reorder.target();

    if (from < 0) {
        _reorder.landed();

        return;
    }

    // Before the grid forgets the drag, and for the cards the rebuild puts in
    // place of these.
    _settle = _reorder.offsets(_cards);
    _settling = to;

    _reorder.landed();

    if (to != from) {
        _reach->config.lists().movePort(from, to);
    }

    wake();
    invalidate();
}

void EnginesPage::settle(const double now) {
    if (_settle.empty()) {
        return;
    }

    if (_settle.size() != _cards.size()) {
        _settling = -1;
        _settle.clear();

        return;
    }

    for (size_t index = 0; index < _cards.size(); ++index) {
        if (const BLPoint &from = _settle[index]; from.x != 0.0 || from.y != 0.0) {
            _cards[index]->slideFrom(from.x, from.y, now);
        }
    }

    _settle.clear();
}

BLRect EnginesPage::adderBox() const {
    const int count = static_cast<int>(_cards.size());

    return BLRect{_reorder.cellX(count), _reorder.cellY(count), _reorder.cell(),
                  _reorder.rowHeight()};
}

void EnginesPage::rebuild() {
    // A rebuild takes a carried card out of the pointer's hand, and no release
    // follows it.
    _reorder.landed();

    _grid->clear();
    _cards.clear();
    _kept = nullptr;

    const bool here = installed();

    _reorder.setRowHeight(Shelf::rowHeight());

    if (here) {
        const std::vector<State::NameRow> &ports = State::get().cfg.ports;

        for (size_t index = 0; index < ports.size(); ++index) {
            const State::NameRow &port = ports[index];
            components::EngineCard *card = _grid->append(std::make_unique<components::EngineCard>());

            card->draggable = true;
            card->name = port.name;
            card->file = port.file;
            card->missing = port.missing;
            card->trouble = port.missing;

            if (port.dosbox) {
                card->tags.push_back(State::BadgeSpec{.text = "DOS", .kind = State::BadgeKind::Muted, .dot = false});
                card->tagHints.emplace_back();
            }

            if (port.fetched) {
                card->tags.push_back(State::BadgeSpec{.text = "Managed", .kind = State::BadgeKind::None, .dot = false});
                card->tagHints.emplace_back("Downloaded and updated by ZDL4");
            }

            if (port.detected && !port.missing) {
                card->tags.push_back(State::BadgeSpec{.text = "Detected", .kind = State::BadgeKind::Success, .dot = false});
                card->tagHints.emplace_back();
            }

            if (port.missing) {
                card->tags.push_back(State::BadgeSpec{.text = "Missing", .kind = State::BadgeKind::Danger, .dot = true});
                card->tagHints.emplace_back();
            }

            const int at = static_cast<int>(index);
            const std::string name = port.name;
            const std::string file = port.file;
            const bool dosbox = port.dosbox;
            const bool fetched = port.fetched;

            const auto editing = [this, at, name, file, dosbox] {
                _reach->edit("Edit " + name, dialogs::EntryDialog::Kind::Port, Filters::port(), FilePicker::Slot::Src, name, file,
                           true, dosbox,
                           [this, at](const std::string &named, const std::string &path,
                                      const bool dos) {
                               _reach->config.lists().updatePort(at, named, path, dos);
                           });
            };

            card->opened = editing;

            card->buttons()->append(std::make_unique<Button>("Edit", editing))
                ->glyph(Glyphs::Glyph::Edit)->compact()->tooltip("Rename or point at another file");

            GlyphButton *folder = card->buttons()->append(
                std::make_unique<GlyphButton>(Glyphs::Glyph::Folder, [file] {
                    Desktop::open(Format::directoryOf(file));
                }));

            folder->outlined()->tooltip("Show in file explorer");
            folder->fixedWidth = Theme::controlSmall;

            GlyphButton *bin = card->buttons()->append(
                std::make_unique<GlyphButton>(Glyphs::Glyph::Trash, [this, at, name, fetched] {
                    _reach->ask("Remove \"" + name + "\"?",
                              fetched ? "Everything ZDL4 unpacked is deleted and it goes "
                                        "out of every profile that named it."
                                      : "It goes out of this list and out of every profile that "
                                        "named it. The file itself is left where it is.",
                              "Remove", true,
                              [this, at] { _reach->engines.forget(at); });
                }));

            bin->outlined()->tone(Theme::of().muted, Theme::of().danger)
                ->tooltip(fetched ? "Delete what was fetched and remove from the list"
                              : "Remove from the list. The file itself is left where it is");
            bin->fixedWidth = Theme::controlSmall;

            card->buttons()->append(std::make_unique<Spacer>());

            card->dragStarted = [this, index](const double x, const double y) {
                _reorder.grabbed(static_cast<int>(index), x, y);
            };

            card->dragMoved = [this](const double x, const double y) {
                _reorder.carried(static_cast<int>(_cards.size()), x, y);
            };

            card->dragEnded = [this] { land(); };

            _cards.push_back(card);
        }

        return;
    }

    const std::vector<State::EngineRow> &rows = State::get().ports.rows;

    for (size_t index = 0; index < rows.size(); ++index) {
        const State::EngineRow &row = rows[index];
        components::EngineCard *card = _grid->append(std::make_unique<components::EngineCard>());

        const bool working = row.status == State::EngineState::Fetching || row.status == State::EngineState::Unpacking;
        const bool present = row.status == State::EngineState::Installed;
        const bool behind = present && !row.have.empty() && !row.version.empty()
            && row.version != row.have;

        card->name = row.name;
        card->blurb = row.blurb;
        card->working = working;
        card->progress = row.status == State::EngineState::Unpacking ? 1.0 : row.progress;
        card->trouble = row.status == State::EngineState::Failed;

        if (row.dos) {
            card->tags.push_back(State::BadgeSpec{.text = "DOS", .kind = State::BadgeKind::Muted, .dot = false});
        }

        if (row.status != State::EngineState::Ready && row.status != State::EngineState::Waiting && !working) {
            card->tags.push_back(State::BadgeSpec{
                .text = behind                      ? "Update"
                      : present                     ? "Installed"
                      : row.status == State::EngineState::Checking    ? "Checking"
                      : row.status == State::EngineState::Failed      ? "Failed"
                      : row.status == State::EngineState::Elsewhere   ? "Its own site"
                                                    : "No downloads available",
                .kind = behind                      ? State::BadgeKind::None
                      : present                     ? State::BadgeKind::Success
                      : row.status == State::EngineState::Failed      ? State::BadgeKind::Danger
                      : row.status == State::EngineState::Checking    ? State::BadgeKind::None
                                                    : State::BadgeKind::Warning,
                .dot = false});
        }

        card->told = !row.error.empty()             ? row.error
                   : row.status == State::EngineState::Elsewhere      ? "Fetched from its own site"
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
                _reach->engines.cancel(at);
            }))->glyph(Glyphs::Glyph::Close)->compact();
        } else if (row.status != State::EngineState::Elsewhere && row.status != State::EngineState::Unavailable) {
            Button *fetch = card->buttons()->append(
                std::make_unique<Button>(behind      ? "Update"
                                         : present   ? "Fetch again"
                                                     : "Install",
                                         [this, at] { _reach->engines.install(at); }));

            fetch->glyph(Glyphs::Glyph::Download)->compact()
                ->kind(present && !behind ? Button::Kind::Default : Button::Kind::Primary)
                ->busy(row.asking)
                ->tooltip(present ? "Fetch the latest build over the one that is here"
                              : "Fetch it and add it to the source ports");
        }

        if (present && !working) {
            GlyphButton *folder = card->buttons()->append(
                std::make_unique<GlyphButton>(Glyphs::Glyph::Folder, [file] {
                    Desktop::open(Format::directoryOf(file));
                }));

            folder->outlined()->tooltip("Show in file explorer");
            folder->fixedWidth = Theme::controlSmall;

            GlyphButton *bin = card->buttons()->append(
                std::make_unique<GlyphButton>(Glyphs::Glyph::Trash, [this, at, name] {
                    _reach->ask("Remove " + name + "?",
                              "Everything ZDL4 unpacked for it is deleted and it is removed "
                              "from the list. Profiles pointing at it are left without a port.",
                              "Remove it", true, [this, at] { _reach->engines.remove(at); });
                }));

            bin->outlined()->tone(Theme::of().muted, Theme::of().danger)
                ->tooltip("Delete what was fetched and remove from the list");
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
                                                  State::get().ports.directory));
    _kept->path()->onClick("Show in file explorer",
                           [] { Desktop::open(State::get().ports.directory); });
}

void EnginesPage::sync() {
    const bool here = installed();

    _which->setCurrent(static_cast<int>(State::get().nav.engines));

    _recheck->setVisible(!here);
    _recheck->busy(State::get().ports.checking);

    _note->setText(here ? components::say(State::get().cfg.ports.size(), "source port")
                              + " · what the profiles are run with"
                   : !State::get().ports.trouble.empty()
                       ? State::get().ports.trouble
                       : "Fetched, unpacked and set up here · what each project has released is "
                         "looked up on GitHub");

    _note->tone(!here && !State::get().ports.trouble.empty() ? Theme::of().danger
                                                             : Theme::of().faint);

    if (!here && !_asked) {
        _asked = true;

        _reach->engines.refresh(false);
    }

    // Rebuilt only when what the cards are made of has moved: the rows, not the
    // config revision, which every keystroke on another page bumps.
    std::string mark = std::to_string(static_cast<int>(State::get().nav.engines));

    if (here) {
        for (const State::NameRow &port : State::get().cfg.ports) {
            mark += '\n' + port.name + '\t' + port.file + (port.missing ? "\tgone" : "")
                + (port.dosbox ? "\tdos" : "") + (port.fetched ? "\tfetched" : "")
                + (port.detected ? "\tfound" : "");
        }
    } else {
        for (const State::EngineRow &row : State::get().ports.rows) {
            mark += '\n' + row.name + '\t' + std::to_string(static_cast<int>(row.status))
              + '\t' + row.version + '\t' + row.have
                + '\t' + row.error + (row.asking ? "\task" : "");
        }
    }

    if (mark == _mark) {
        // A moving bar is the one thing that changes without rebuilding.
        for (size_t index = 0; index < _cards.size() && !here; ++index) {
            const State::EngineRow &row = State::get().ports.rows[index];

            if (_cards[index]->working) {
                _cards[index]->progress = row.status == State::EngineState::Unpacking ? 1.0 : row.progress;

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
    settle(now);

    if (root() != nullptr) {
        _grid->arrange(root()->type());
    }

    if (_reorder.dragging()) {
        return true;
    }

    // A card still walking to its gap needs the grid laid out under it.
    const bool walking = std::ranges::any_of(
        _cards, [](const components::EngineCard *card) { return card->sliding(); });

    if (!walking) {
        _settling = -1;
    }

    return walking;
}

}
