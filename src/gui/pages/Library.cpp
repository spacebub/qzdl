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
#include <cstdint>
#include <cmath>
#include <utility>

#include "gui/app/Filters.h"
#include "gui/draw/Glyphs.h"
#include "gui/draw/Typeface.h"
#include "gui/pages/Library.h"
#include "gui/state/State.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/Field.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/controls/Segmented.h"
#include "gui/toolkit/controls/Select.h"
#include "gui/toolkit/layout/Box.h"
#include "gui/toolkit/layout/Scroll.h"
#include "gui/util/Desktop.h"
#include "gui/util/Format.h"

namespace {

enum class ProfileCardAction : std::uint8_t {
    Open,
    Launch,
    Duplicate,
    Rename,
    Delete,
};

enum class GameCardAction : std::uint8_t {
    Play,
    Use,
    Edit,
    Reveal,
    Remove,
};

// Segmented is keyed by text, so the shelf meets it here and nowhere else.
constexpr const char *shelfKey(const State::Shelf shelf) {
    return shelf == State::Shelf::Games ? "games" : "profiles";
}

State::Shelf shelfFrom(const std::string &key) {
    return key == "games" ? State::Shelf::Games : State::Shelf::Profiles;
}

constexpr double SETTLING = 0.19;

std::string say(const size_t number, const std::string &thing) {
    return std::to_string(number) + " " + thing + (number == 1 ? "" : "s");
}

}

namespace pages {

using namespace toolkit;

// The cards, the tile that adds one, and the word when a filter matches nothing.
class LibraryPage::Shelf : public Widget {
public:
    explicit Shelf(LibraryPage *view) : _view(view) {
        _takesPointer = true;
    }

    // The shelf owns the grid: the scroller asks it how tall the cards come to and
    // then places it, which is what makes a wheel or a dragged bar move them.
    double naturalHeight(Typeface & /*type*/, const double width) override {
        _view->measure(width);

        const int count = static_cast<int>(_view->_cards.size());
        const int rows = (count + _view->_columns) / _view->_columns;

        return (rows * (Theme::rowHeight + Theme::gutter)) + 34.0;
    }

    void arrange(Typeface &type) override {
        _view->measure(_box.w);

        // A card carried along by a scroll has not moved on the shelf, and the
        // scroller moves those pixels itself.
        const double alongX = _box.x - _atX;
        const double alongY = _box.y - _atY;

        _atX = _box.x;
        _atY = _box.y;

        const int count = static_cast<int>(_view->_cards.size());

        for (int index = 0; index < count; ++index) {
            components::LibraryCard *card = _view->_cards[static_cast<size_t>(index)];
            const int at = _view->slot(index);

            // Where it was drawn, carry and all: a card being carried keeps its
            // place in the grid and moves only by that, so the box alone would
            // say nothing had changed.
            const BLRect held = card->box();
            const BLRect was{held.x + card->carryX + card->slideX(),
                             held.y + card->carryY + card->slideY(), held.w, held.h};

            card->carryX = index == _view->_origin ? _view->_carryX.value() : 0.0;
            card->carryY = index == _view->_origin ? _view->_carryY.value() : 0.0;

            const BLRect cell{_view->cellX(at), _view->cellY(at), _view->_cell,
                              Theme::rowHeight};

            // A neighbour the carried one has passed walks to its new gap rather
            // than jumping into it. Both places are read off the grid as it stands
            // now, so a scroll -- which moves every cell -- is not a reorder.
            if (const int wasAt = card->slot();
                index != _view->_origin && wasAt >= 0 && wasAt != at) {
                card->slideFrom(_view->cellX(wasAt) - cell.x, _view->cellY(wasAt) - cell.y,
                                card->now());
            }

            card->setSlot(at);
            card->place(cell, type);

            const BLRect now{cell.x + card->carryX + card->slideX(),
                             cell.y + card->carryY + card->slideY(), cell.w, cell.h};

            // Only what moved is repainted; a drag redraws two cards, not a page.
            if (was.x + alongX != now.x || was.y + alongY != now.y || was.w != now.w
                || was.h != now.h) {
                card->invalidate(components::LibraryCard::spread(was));
                card->invalidate(components::LibraryCard::spread(now));
            }
        }
    }

    void paint(const Painter &painter) override {
        _view->paintAdder(painter, _lit);

        const std::vector<components::LibraryCard *> &cards = _view->_cards;
        const int carried = _view->_origin;

        // The carried card is drawn last, over its neighbours.
        for (size_t index = 0; index < cards.size(); ++index) {
            if (std::cmp_not_equal(index, carried) && painter.needed(cards[index]->drawn())) {
                cards[index]->paint(painter);
            }
        }

        if (carried >= 0 && std::cmp_less(carried, cards.size())) {
            cards[static_cast<size_t>(carried)]->paint(painter);
        }

        _view->paintNothing(painter);
    }

    void hover(const Pointer &at) override {
        const BLRect adder = _view->adderBox();
        const bool over = at.x >= adder.x && at.x < adder.x + adder.w && at.y >= adder.y
            && at.y < adder.y + adder.h;

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

        return at.x >= adder.x && at.x < adder.x + adder.w && at.y >= adder.y
            && at.y < adder.y + adder.h;
    }

    void release(const Pointer &at) override {
        const BLRect adder = _view->adderBox();

        if (at.x >= adder.x && at.x < adder.x + adder.w && at.y >= adder.y
            && at.y < adder.y + adder.h) {
            _view->addPressed();
        }
    }

private:
    LibraryPage *_view;

    double _atX = 0.0;
    double _atY = 0.0;

    bool _lit = false;
};

LibraryPage::LibraryPage(Reach *reach) : _reach(reach) {
    Box *column = append(Box::column());

    column->spacing(18.0);

    Box *header = column->append(Box::row());

    header->fixedHeight = 52.0;
    header->cross(Box::Place::Centre);
    header->pad(Theme::bleed, 0.0, Theme::bleed, 0.0);

    _head = header->append(Box::column());
    _head->spacing(3.0);
    _head->stretch = 1.0;
    _head->align(Box::Place::Centre);

    _title = _head->append(std::make_unique<Label>("Library"));
    _title->font(Theme::of().headingWeight, Theme::fontDisplay)->tone(Theme::of().text);

    _note = _head->append(std::make_unique<Label>());
    _note->font(400, Theme::fontSmall)->tone(Theme::of().faint);

    _tools = header->append(Box::row());
    _tools->spacing(14.0);
    _tools->cross(Box::Place::Centre);

    _addPort = _tools->append(std::make_unique<Button>("Add a port…", [this] {
        _reach->go(State::Page::Engines);
    }));
    _addPort->glyph(Glyphs::Glyph::Plus)->compact()
        ->tooltip("Nothing here can run until a source port is set up");

    _port = _tools->append(std::make_unique<Select>("", [this](const int index) {
        const std::vector<std::string> &names = State::get().cfg.portNames;

        _reach->config.settings().setGamePort(
            index < 0 || std::cmp_greater_equal(index, names.size())
                ? std::string()
                : names[static_cast<size_t>(index)]);
    }));
    _port->clearable()->placeholder("(Profile's port)")
        ->tooltip("What a game on this shelf launches with");
    _port->fixedWidth = 180.0;

    _shelf = _tools->append(std::make_unique<Segmented>([this](const std::string &key) {
        State::get().nav.shelf = shelfFrom(key);

        _reach->touch();
    }));
    _shelf->setOptions({{.key = "profiles", .label = "Profiles"},
                        {.key = "games", .label = "Games"}});

    _filter = _tools->append(std::make_unique<Field>("", [this](const std::string &value) {
        _reach->config.library().setFilter(value);
    }));
    _filter->leadingGlyph(Glyphs::Glyph::Search)->placeholder("Filter");
    _filter->fixedWidth = 190.0;

    _scroll = column->append(std::make_unique<Scroll>());
    _scroll->stretch = 1.0;

    _grid = static_cast<Shelf *>(_scroll->hold(std::make_unique<Shelf>(this)));

    // One card and its margins; under that the shelf is cut rather than squeezed.
    _grid->minWidth = Theme::cardWidth + (Theme::bleed * 2.0);
}

bool LibraryPage::profiles() {
    return State::get().nav.shelf == State::Shelf::Profiles;
}

void LibraryPage::measure(const double width) {
    // Stretched to fill the row, so the last card ends where the header does.
    const double room = std::max(Theme::cardWidth, width - (Theme::bleed * 2.0));

    _columns = std::max(1, static_cast<int>(std::floor((room + Theme::gutter)
                                                       / (Theme::cardWidth + Theme::gutter))));
    _cell = (room - ((_columns - 1) * Theme::gutter)) / _columns;
}

double LibraryPage::cellX(const int index) const {
    return _grid->box().x + Theme::bleed
        + ((index % _columns) * (_cell + Theme::gutter));
}

double LibraryPage::cellY(const int index) const {
    const int row = index / _columns;

    return _grid->box().y + Theme::shelfTop + (row * (Theme::rowHeight + Theme::gutter));
}

int LibraryPage::placeAt(const double x, const double y) const {
    const int count = static_cast<int>(_cards.size());

    if (count == 0) {
        return 0;
    }

    const int row = static_cast<int>(
        std::floor((y - _grid->box().y - Theme::shelfTop) / (Theme::rowHeight + Theme::gutter)));
    const int column = std::clamp(
        static_cast<int>(std::floor((x - _grid->box().x - Theme::bleed)
                                    / (_cell + Theme::gutter))),
        0, _columns - 1);

    return std::clamp((row * _columns) + column, 0, count - 1);
}

int LibraryPage::slot(const int index) const {
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

void LibraryPage::grabbed(const int index, const double x, const double y) {
    _dragging = true;
    _origin = index;
    _target = index;
    _grabX = x - cellX(index);
    _grabY = y - cellY(index);

    _carryX.set(0.0F);
    _carryY.set(0.0F);
}

void LibraryPage::carried(const double x, const double y) {
    const int wanted = placeAt(x, y);

    _target = wanted;

    _carryX.set(static_cast<float>(x - _grabX - cellX(_origin)));
    _carryY.set(static_cast<float>(y - _grabY - cellY(_origin)));

    animate();
}

void LibraryPage::dropped() {
    _dragging = false;

    if (_origin < 0) {
        return;
    }

    // The card walks to its gap; only then does the list change.
    _carryX.run(static_cast<float>(cellX(_target) - cellX(_origin)), now(), SETTLING,
                Anim::Curve::CubicOut);
    _carryY.run(static_cast<float>(cellY(_target) - cellY(_origin)), now(), SETTLING,
                Anim::Curve::CubicOut);

    _landing = now() + SETTLING + 0.02;

    animate();
}

void LibraryPage::land() {
    _landing = 0.0;

    if (_origin < 0) {
        return;
    }

    if (_target != _origin) {
        if (profiles()) {
            _reach->config.profile().moveProfile(_origin, _target);
        } else {
            _reach->config.lists().moveIwad(_origin, _target);
        }
    }

    _origin = -1;
    _target = -1;
    _carrying.clear();

    _carryX.set(0.0F);
    _carryY.set(0.0F);

    invalidate();
}

void LibraryPage::buildProfile(components::LibraryCard *card, const State::ProfileCard &profile,
                               const int index) {
    const std::string id = profile.id;
    const int at = profile.index;
    const std::string name = profile.name;

    card->title = profile.name;
    card->artKey = profile.artKey;
    card->caption = profile.iwad.empty() ? "NO GAME" : Format::upper(profile.iwad);
    card->subtitle = profile.port.empty() ? "No source port" : profile.port;
    card->playable = profile.ready;
    card->primary = "open";
    card->status = _reach->runs.stateOf(profile.key);
    card->statusReason = _reach->runs.reasonOf(profile.key);
    card->badges = ProfileBridge::badgesOf(profile.index);

    card->playHint = profile.ready ? "Launch " + profile.name
                                   : "This profile has no source port to run";

    card->actions = {
        Menu::item(ProfileCardAction::Open, "Set this one up", Glyphs::Glyph::Edit),
        Menu::item(ProfileCardAction::Launch, "Launch it", Glyphs::Glyph::Play, false,
                   !profile.ready),
        Menu::rule(),
        Menu::item(ProfileCardAction::Duplicate, "Duplicate", Glyphs::Glyph::Extract),
        Menu::item(ProfileCardAction::Rename, "Rename…", Glyphs::Glyph::Edit),
        Menu::rule(),
        Menu::item(ProfileCardAction::Delete, "Delete", Glyphs::Glyph::Trash, true),
    };

    card->played = [this, at] { _reach->config.profile().launchAt(at); };

    card->opened = [this, at] {
        _reach->config.profile().setProfileIndex(at);
        _reach->go(State::Page::Profile);
    };

    const std::string key = profile.key;

    card->logRequested = [this, key] { _reach->runs.show(key); };

    card->triggered = [this, at](const int action) {
        _reach->config.profile().setProfileIndex(at);

        switch (static_cast<ProfileCardAction>(action)) {
            case ProfileCardAction::Open:
                _reach->go(State::Page::Profile);
                break;
            case ProfileCardAction::Launch:
                _reach->config.profile().launchAt(at);
                break;
            case ProfileCardAction::Duplicate:
                _reach->config.profile().duplicateProfile();
                break;
            case ProfileCardAction::Rename:
                _reach->prompt("Rename profile", "Name", State::get().cfg.profileName,
                               "Rename", [this](const std::string &named) {
                                   _reach->config.profile().renameProfile(named);
                               });
                break;
            case ProfileCardAction::Delete:
                _reach->ask("Delete \"" + State::get().cfg.profileName + "\"?",
                            "The profile and everything in it goes. The files it loaded "
                            "are left alone.",
                            "Delete", true,
                            [this] { _reach->config.profile().removeProfile(); });
                break;
        }
    };

    card->pressedDown = [this] { land(); };

    card->dragStarted = [this, index, id](const double x, const double y) {
        _carrying = id;

        grabbed(index, x, y);
    };

    card->dragMoved = [this](const double x, const double y) { carried(x, y); };
    card->dragEnded = [this] { dropped(); };
}

void LibraryPage::buildGame(components::LibraryCard *card, const State::NameRow &game, const int index) {
    const std::string name = game.name;
    const std::string file = game.file;
    const int at = game.index;
    const bool missing = game.missing;
    const std::string key = ConfigBridge::gameKey(game.name);

    card->title = game.name;
    card->caption = game.kind.empty() ? "FILE" : Format::upper(game.kind);
    card->artKey = game.missing ? std::string() : game.file;
    card->subtitle = State::get().cfg.showPaths ? Format::prettyPath(game.directory)
                                                : std::string();
    card->playable = !game.missing;
    card->primary = "play";
    card->status = _reach->runs.stateOf(key);
    card->statusReason = _reach->runs.reasonOf(key);

    card->badges.clear();

    if (game.missing) {
        card->badges.push_back(State::BadgeSpec{.text = "Missing", .kind = State::BadgeKind::Danger, .dot = true});
    }

    card->playHint = game.missing ? "This file is not where the library says it is"
                                  : LibraryBridge::gameCommandLine(game.name);

    card->actions = {
        Menu::item(GameCardAction::Play, "Play it", Glyphs::Glyph::Play, false, missing),
        Menu::item(GameCardAction::Use, "Use it in this profile", Glyphs::Glyph::Check),
        Menu::rule(),
        Menu::item(GameCardAction::Edit, "Rename…", Glyphs::Glyph::Edit),
        Menu::item(GameCardAction::Reveal, "Show the folder it is in",
                   Glyphs::Glyph::Folder),
        Menu::rule(),
        Menu::item(GameCardAction::Remove, "Remove from the library",
                   Glyphs::Glyph::Trash, true),
    };

    card->played = [this, name] { _reach->config.library().launchGame(name); };
    card->opened = card->played;
    card->logRequested = [this, key] { _reach->runs.show(key); };

    card->triggered = [this, name, file, at](const int action) {
        switch (static_cast<GameCardAction>(action)) {
            case GameCardAction::Play:
                _reach->config.library().launchGame(name);
                break;
            case GameCardAction::Use:
                _reach->config.profile().setIwad(name);
                _reach->notify.success("\"" + State::get().cfg.profileName + "\" now plays "
                                       + name + ".");
                break;
            case GameCardAction::Edit:
                _reach->edit("Edit " + name, "iwad", Filters::wad(), FilePicker::Slot::Wad, name, file,
                             false, false,
                             [this, at](const std::string &named, const std::string &path,
                                        bool) {
                                 _reach->config.lists().updateIwad(at, named, path);
                             });
                break;
            case GameCardAction::Reveal:
                Desktop::open(Format::directoryOf(file));
                break;
            case GameCardAction::Remove:
                _reach->ask("Remove \"" + name + "\"?",
                            "It goes out of the library and out of every profile that named "
                            "it. The file itself is left where it is.",
                            "Remove", true,
                            [this, at] { _reach->config.lists().removeIwad(at); });
                break;
        }
    };

    card->pressedDown = [this] { land(); };

    card->dragStarted = [this, index, name](const double x, const double y) {
        _carrying = name;

        grabbed(index, x, y);
    };

    card->dragMoved = [this](const double x, const double y) { carried(x, y); };
    card->dragEnded = [this] { dropped(); };
}

void LibraryPage::sync() {
    const State::Cfg &cfg = State::get().cfg;
    const bool onProfiles = profiles();

    _shelf->setCurrent(shelfKey(State::get().nav.shelf));

    _title->setText("Library");

    _note->setText(onProfiles
        ? say(cfg.profileCards.size(), "profile")
          + " · press a card to set one up, or the play button to run it"
        : say(cfg.iwads.size(), "game") + " · everything the profiles are built on");

    _addPort->setVisible(!onProfiles && cfg.ports.empty());
    _port->setVisible(!onProfiles && !cfg.ports.empty());

    if (_port->visible()) {
        _port->setOptions(cfg.portNames);
        _port->setBadges(cfg.portBadges);

        const auto found = std::ranges::find(cfg.portNames, cfg.gamePort);

        _port->setCurrent(found == cfg.portNames.end()
                              ? -1
                              : static_cast<int>(found - cfg.portNames.begin()));
    }

    if (_filter->text() != cfg.filter) {
        _filter->setText(cfg.filter);
    }

    // A title screen arriving only repaints: rebuilding would drop the card the
    // pointer is on, and with it the hover, the sprites and the tween.
    if (State::get().sys.artRev != _artRev) {
        _artRev = State::get().sys.artRev;

        for (components::LibraryCard  const*card : _cards) {
            card->invalidate();
        }
    }

    // Rebuilt only when what the cards are made of has moved.
    std::string mark = std::string(shelfKey(State::get().nav.shelf)) + '\n'
        + std::to_string(cfg.rev) + '\n'
        + std::to_string(State::get().runs.rev) + '\n' + std::to_string(cfg.gameRev)
        + (cfg.showPaths ? "\np" : "");

    if (onProfiles) {
        for (const State::ProfileCard &card : cfg.shelfProfiles) {
            mark += '\n' + card.id;
        }
    } else {
        for (const State::NameRow &row : cfg.shelfGames) {
            mark += '\n' + row.name;
        }
    }

    if (mark == _mark) {
        return;
    }

    _mark = std::move(mark);

    _grid->clear();
    _cards.clear();

    const size_t count = onProfiles ? cfg.shelfProfiles.size() : cfg.shelfGames.size();

    // A filtered shelf's neighbours are not the list's.
    const bool reorderable = cfg.filter.empty();

    for (size_t index = 0; index < count; ++index) {
        auto made = std::make_unique<components::LibraryCard>();
        components::LibraryCard *card = made.get();

        card->draggable = reorderable;
        card->artwork = [this](const std::string &key) { return _reach->art.of(key); };

        if (onProfiles) {
            buildProfile(card, cfg.shelfProfiles[index], static_cast<int>(index));
        } else {
            buildGame(card, cfg.shelfGames[index], static_cast<int>(index));
        }

        _cards.push_back(card);
        _grid->add(std::move(made));
    }

    if (root() != nullptr) {
        root()->relayout();
    }
}

BLRect LibraryPage::adderBox() const {
    const int count = static_cast<int>(_cards.size());

    return BLRect{cellX(count), cellY(count), _cell, Theme::rowHeight};
}

void LibraryPage::paintAdder(const Painter &painter, const bool lit) const {
    const Theme::Palette &palette = Theme::of();
    const BLRect box = adderBox();

    if (!painter.needed(box)) {
        return;
    }

    painter.round(box, Theme::radius,
                  lit ? Theme::mix(palette.sunken, palette.hover, 0.85) : palette.sunken);
    painter.outline(box, Theme::radius, 1.0, lit ? palette.accent : palette.borderStrong);

    const BLPoint middle{box.x + (box.w / 2.0), box.y + (box.h / 2.0) - 14.0};

    painter.circle(middle, 23.0, lit ? palette.accentSoft : palette.surface);
    painter.context().set_stroke_width(1.0);
    painter.context().stroke_circle(middle.x, middle.y, 22.5,
                                    lit ? palette.accent : palette.borderStrong);

    const double side = Glyphs::span(1.5F);

    Glyphs::draw(painter.context(), Glyphs::Glyph::Plus, BLPoint{middle.x - (side / 2.0), middle.y - (side / 2.0)},
                 1.5F, lit ? palette.accent : palette.muted);

    painter.label(painter.font(600, Theme::fontSmall),
                  BLRect{box.x, middle.y + 33.0, box.w, 20.0}, Align::Centre,
                  profiles() ? "New profile" : "Add a game",
                  lit ? palette.accent : palette.muted);
}

void LibraryPage::paintNothing(const Painter &painter) const {
    const State::Cfg &cfg = State::get().cfg;

    if (!cfg.filter.empty() && _cards.empty()) {
        const Theme::Palette &palette = Theme::of();
        const BLRect box = adderBox();
        const double wide = std::min(420.0, _scroll->box().w - 60.0);
        const double x = _scroll->box().x + ((_scroll->box().w - wide) / 2.0);
        double y = box.y + box.h + 28.0;

        const BLFont &heading = painter.font(600, Theme::fontMedium);

        painter.label(heading, BLRect{x, y, wide, painter.lineHeight(heading)}, Align::Start,
                      "Nothing called that", palette.muted);

        y += painter.lineHeight(heading) + 6.0;

        painter.paragraph(painter.font(400, Theme::fontSmall), BLRect{x, y, wide, 0.0},
                          "No " + std::string(profiles() ? "profile" : "game") + " here has \""
                              + cfg.filter + "\" in its name.",
                          palette.faint);
    }
}

void LibraryPage::addPressed() {
    if (profiles()) {
        _reach->prompt("New profile", "Name", "New profile", "Create",
                     [this](const std::string &named) {
                         _reach->config.profile().addProfile(named);
                         _reach->go(State::Page::Profile);
                     });
    } else {
        _reach->picker.open(FilePicker::Action::AddIwads, "Add games", Filters::wad(), false, false, true,
                            FilePicker::Slot::Wad);
    }
}

bool LibraryPage::advance(const double now) {
    _carryX.advance(now);
    _carryY.advance(now);

    // The shelf repaints what actually moved.
    if (root() != nullptr) {
        _grid->arrange(root()->type());
    }

    if (_landing > 0.0 && now >= _landing) {
        land();

        return false;
    }

    return _carryX.live() || _carryY.live() || _landing > 0.0 || _dragging;
}

}
