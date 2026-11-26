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
#include <iterator>
#include <string>
#include <utility>

#include "ttk/draw/Glyphs.h"
#include "ttk/draw/Typeface.h"
#include "ttk/system/Text.h"
#include "ttk/toolkit/Root.h"
#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/MultistateSwitch.h"
#include "ttk/toolkit/controls/Select.h"
#include "ttk/toolkit/layout/Box.h"
#include "ttk/toolkit/layout/Scroll.h"
#include "ttk/util/Desktop.h"
#include "ttk/util/Format.h"

#include "core/launch/Shortcut.h"
#include "gui/components/GroupMenu.h"
#include "gui/components/Parts.h"
#include "gui/draw/Band.h"
#include "gui/draw/Cards.h"
#include "gui/draw/DropMark.h"
#include "gui/pages/LibraryPage.h"
#include "gui/services/Filters.h"
#include "gui/state/State.h"

using namespace ttk;

namespace {

enum class ProfileCardAction : std::uint8_t {
    Open,
    Launch,
    Duplicate,
    Rename,
    Shortcut,
    Delete,
};

enum class GameCardAction : std::uint8_t {
    Play,
    Use,
    Edit,
    Reveal,
    Remove,
};

// The shelf is not rebuilt while a run comes and goes, so its rows are re-armed in place.
void armProfileActions(components::LibraryCard *card, const bool busy) {
    for (ttk::Menu::Row &row : card->actions) {
        if (row.separator) {
            continue;
        }

        if (const auto action = static_cast<ProfileCardAction>(row.action);
            action == ProfileCardAction::Rename || action == ProfileCardAction::Delete) {
            row.disabled = busy;
        }
    }
}

}

namespace pages {

// The cards, the tile that adds one, and the word when a filter matches nothing.
class LibraryPage::Shelf : public Widget {
public:
    explicit Shelf(LibraryPage *view) : _view(view) { _takesPointer = true; }

    // The shelf owns the grid: the scroller asks it how tall the cards come to and
    // then places it, which is what makes a wheel or a dragged bar move them.
    double natural_height(Typeface & /*type*/, const double width) override {
        _view->_reorder.measure(width);

        if (!_view->_sections.empty()) {
            return _view->_reorder.height(true) + 24.0 - _view->lift(_view->_sections.size());
        }

        const int count = static_cast<int>(_view->_cards.size());
        const int rows = _view->_reorder.rows_with_adder(count);

        return (rows * (Cards::rowHeight + Theme::gutter)) + 34.0;
    }

    void arrange(Typeface &type) override {
        _view->_reorder.place(_box);

        // A card carried along by a scroll has not moved on the shelf, and the
        // scroller moves those pixels itself.
        const double alongX = _box.x - _atX;
        const double alongY = _box.y - _atY;

        _atX = _box.x;
        _atY = _box.y;

        const int count = static_cast<int>(_view->_cards.size());
        const bool sectioned = !_view->_sections.empty();

        markBands(alongX, alongY);

        for (int index = 0; index < count; ++index) {
            components::LibraryCard *card = _view->_cards[static_cast<size_t>(index)];
            const int at = _view->_reorder.slot(index);

            // Where it was drawn, carry and all: a card being carried keeps its
            // place in the grid and moves only by that, so the box alone would
            // say nothing had changed.
            const BLRect held = card->box();
            const BLRect was{
                    held.x + card->carryX + card->slide_x(), held.y + card->carryY + card->slide_y(), held.w, held.h};

            card->carryX = index == _view->_reorder.origin() ? _view->_reorder.carry_x() : 0.0;
            card->carryY = index == _view->_reorder.origin() ? _view->_reorder.carry_y() : 0.0;

            const int section = _view->sectionOf(index);
            const BLRect cell{_view->_reorder.cell_x(at),
                              _view->_reorder.cell_y(at)
                                      - (section < 0 ? 0.0 : _view->lift(static_cast<size_t>(section))),
                              _view->_reorder.cell(),
                              Cards::rowHeight};

            // A neighbour the carried one has passed walks to its new gap rather
            // than jumping into it. Both places are read off the grid as it stands
            // now, so a scroll, which moves every cell, is not a reorder.
            const int wasAt = card->slot();
            const bool walks = index != _view->_reorder.origin() && wasAt >= 0;

            if (walks && !sectioned && wasAt != at) {
                card->slideFrom(
                        _view->_reorder.cell_x(wasAt) - cell.x, _view->_reorder.cell_y(wasAt) - cell.y, card->now());
            } else if (walks && sectioned && _view->_reorder.dragging()
                       && std::abs(held.x + alongX - cell.x) + std::abs(held.y + alongY - cell.y) > 0.5) {
                // A band that moves carries the cells below it along, so the walk starts where the card was.
                card->slideFrom(held.x + alongX - cell.x, held.y + alongY - cell.y, card->now());
            }

            card->setSlot(at);
            card->place(cell, type);

            const BLRect now{
                    cell.x + card->carryX + card->slide_x(), cell.y + card->carryY + card->slide_y(), cell.w, cell.h};

            // Only what moved is repainted. A drag redraws two cards, not a page.
            if (was.x + alongX != now.x || was.y + alongY != now.y || was.w != now.w || was.h != now.h) {
                card->invalidate(components::LibraryCard::spread(was));
                card->invalidate(components::LibraryCard::spread(now));
            }
        }

        // A card holds a sheet and art sprites of about a third of a megabyte each, so a library
        // scrolled end to end would keep them per profile. They are drawn again as the card comes back.
        // The carried card is drawn under the pointer, wherever its slot is.
        if (BLRect band{}; parent() != nullptr && parent()->clips(band)) {
            const double reach = band.h;
            const int carried = _view->_reorder.origin() >= 0 ? _view->_reorder.origin() : _view->_settling;

            for (size_t index = 0; index < _view->_cards.size(); ++index) {
                components::LibraryCard *card = _view->_cards[index];
                const BLRect where = card->box();

                if (std::cmp_not_equal(index, carried)
                    && (where.y + where.h < band.y - reach || where.y > band.y + band.h + reach)) {
                    card->dropArt();
                }
            }
        }
    }

    void paint(const Painter &painter) override {
        _view->paintAdder(painter, _lit);
        paintBands(painter);

        const std::vector<components::LibraryCard *> &cards = _view->_cards;
        const int carried = _view->_reorder.origin() >= 0 ? _view->_reorder.origin() : _view->_settling;

        // The carried card is drawn last, over its neighbours.
        for (size_t index = 0; index < cards.size(); ++index) {
            if (std::cmp_not_equal(index, carried) && painter.needed(cards[index]->drawn())) {
                paintCard(painter, static_cast<int>(index));
            }
        }

        if (carried >= 0 && std::cmp_less(carried, cards.size())) {
            cards[static_cast<size_t>(carried)]->paint(painter);
        }

        _view->paintNothing(painter);
    }

    void hover(const Pointer &at) override {
        const BLRect adder = _view->adderBox();
        const bool over = within(adder, at);

        if (over != _lit) {
            _lit = over;

            invalidate(adder);
        }

        const int band = bandAt(at);
        const bool menu = band >= 0 && Band::onMenu(_view->bandBox(static_cast<size_t>(band)), at.x, at.y);

        if (band != _over || menu != _overMenu) {
            invalidateBand(std::exchange(_over, band));
            invalidateBand(band);

            _overMenu = menu;
        }
    }

    // Only the adder and the bands answer the pointer. The bands repaint on their own, and the rest of the box is
    // every row of the grid.
    [[nodiscard]] BLRect lit_box() const override { return _view->adderBox(); }

    [[nodiscard]] Cursor cursor_at(const double x, const double y) const override {
        return bandAt(Pointer{.x = x, .y = y}) >= 0 ? Cursor::Pointer : Cursor::Default;
    }

    void leave() override {
        Widget::leave();

        _lit = false;
        _overMenu = false;

        invalidateBand(std::exchange(_over, -1));
    }

    // Only the adder and the bands answer a press, and a keyboard click lands elsewhere.
    [[nodiscard]] bool takes_focus() const override { return false; }

    bool press(const Pointer &at) override { return within(_view->adderBox(), at) || bandAt(at) >= 0; }

    void release(const Pointer &at) override {
        if (within(_view->adderBox(), at)) {
            _view->addPressed();

            return;
        }

        const int band = bandAt(at);

        if (band < 0) {
            return;
        }

        const State::Section &section = _view->_sections[static_cast<size_t>(band)];
        const GroupsBridge::List list =
                LibraryPage::profiles() ? GroupsBridge::List::Profiles : GroupsBridge::List::Games;
        const BLRect box = _view->bandBox(static_cast<size_t>(band));
        const std::uint16_t group = section.group;

        if (!Band::onMenu(box, at.x, at.y)) {
            // A filter shows every card, so a group has nothing to fold away.
            if (State::get().cfg.filter.empty()) {
                _view->_folds.turn(
                        group, section.collapsed, now(), [reach = _view->_reach, list, group](const bool folded) {
                            reach->config.groups().fold(list, group, folded);
                        });

                _view->wake();
            }

            return;
        }

        _menuFor = band;

        invalidateBand(band);

        components::showGroupMenu(*_view->_reach, *this, list, group, section.name, Band::menuBox(box), [this] {
            invalidateBand(std::exchange(_menuFor, -1));
        });
    }

private:
    static bool within(const BLRect &box, const Pointer &at) {
        return at.x >= box.x && at.x < box.x + box.w && at.y >= box.y && at.y < box.y + box.h;
    }

    // The named band under the pointer. The ungrouped one answers nothing.
    [[nodiscard]] int bandAt(const Pointer &at) const {
        const std::vector<State::Section> &sections = _view->_sections;

        for (size_t band = 0; band < sections.size(); ++band) {
            if (sections[band].group != 0 && within(_view->bandBox(band), at)) {
                return static_cast<int>(band);
            }
        }

        return -1;
    }

    void invalidateBand(const int band) const {
        if (band >= 0 && std::cmp_less(band, _bands.size())) {
            invalidate(_bands[static_cast<size_t>(band)]);
        }
    }

    // Repaints the bands a carry moved or lit, and keeps where each was drawn.
    void markBands(const double alongX, const double alongY) {
        const int lit = _view->litBand();

        _bands.resize(_view->_sections.size());

        for (size_t band = 0; band < _bands.size(); ++band) {
            const BLRect was{_bands[band].x + alongX, _bands[band].y + alongY, _bands[band].w, _bands[band].h};
            const BLRect now = _view->bandBox(band);

            if (was != now || std::cmp_equal(band, lit) != std::cmp_equal(band, _litDrawn)) {
                invalidate(was);
                invalidate(now);
            }

            _bands[band] = now;
        }

        _litDrawn = lit;
    }

    // A card of a group sliding open or shut shows only as far as the group has slid.
    void paintCard(const Painter &painter, const int index) const {
        components::LibraryCard *card = _view->_cards[static_cast<size_t>(index)];
        const int section = _view->sectionOf(index);

        if (section < 0 || _view->shown(static_cast<size_t>(section)) >= 1.0F) {
            card->paint(painter);

            return;
        }

        painter.push(_view->bodyBox(static_cast<size_t>(section)));
        card->paint(painter);
        painter.pop();
    }

    void paintBands(const Painter &painter) const {
        const std::vector<State::Section> &sections = _view->_sections;
        const int lit = _view->litBand();

        for (size_t band = 0; band < sections.size(); ++band) {
            const BLRect box = _view->bandBox(band);
            const State::Section &section = sections[band];
            const bool over = std::cmp_equal(band, _over);

            if (!painter.needed(box)) {
                continue;
            }

            Band::paint(painter,
                        box,
                        {
                                .name = section.name,
                                .count = std::to_string(section.count),
                                .indent = Band::LANE + 6.0,
                                .open = _view->shown(band),
                                .named = section.group != 0,
                                .over = over,
                                .menu = over || std::cmp_equal(band, _menuFor),
                                .overMenu = over && _overMenu,
                                .lit = std::cmp_equal(band, lit),
                        });
        }
    }

    LibraryPage *_view;

    double _atX = 0.0;
    double _atY = 0.0;

    bool _lit = false;

    std::vector<BLRect> _bands;
    int _litDrawn = -1;

    int _over = -1;
    bool _overMenu = false;
    int _menuFor = -1;
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
    _title->font(Theme::palette().headingWeight, Theme::fontDisplay)->tone(&Theme::Palette::text);

    _note = _head->append(std::make_unique<Label>());
    _note->font(400, Theme::fontSmall)->tone(&Theme::Palette::faint);

    _tools = header->append(Box::row());
    _tools->spacing(14.0);
    _tools->cross(Box::Place::Centre);

    _newGroup = _tools->append(std::make_unique<Label>("New group"));
    _newGroup->tone(&Theme::Palette::muted);
    _newGroup->on_click([this] {
        _reach->prompt("New group", "Name", "New group", "Create", [this](const std::string &named) {
            _reach->config.groups().add(profiles() ? GroupsBridge::List::Profiles : GroupsBridge::List::Games, named);
        });
    });

    _addEngine = _tools->append(std::make_unique<Button>("Add a port", [this] { _reach->go(State::Page::Engines); }));
    _addEngine->glyph(Glyphs::Glyph::Plus)->compact()->tooltip("Nothing here can run until a source port is set up");

    _engine = _tools->append(std::make_unique<Select>("", [this](const int index) {
        const std::vector<std::string> &names = State::get().cfg.engineNames;

        _reach->config.settings().setGameEngine(index < 0 || std::cmp_greater_equal(index, names.size())
                                                        ? std::string()
                                                        : names[static_cast<size_t>(index)]);
    }));
    _engine->clearable()->placeholder("(Profile's port)")->tooltip("What a game on this shelf launches with");
    _engine->fixedWidth = 180.0;

    _shelfSwitch = _tools->append(std::make_unique<MultistateSwitch>([this](const int value) {
        State::get().nav.shelf = static_cast<State::Shelf>(value);

        _reach->touch();
    }));
    _shelfSwitch->set_options({
            {.value = static_cast<int>(State::Shelf::Profiles), .label = "Profiles"},
            {.value = static_cast<int>(State::Shelf::Games), .label = "Games"},
    });

    _filter = _tools->append(std::make_unique<Field>(
            "", [this](const std::string &value) { _reach->config.library().setFilter(value); }));
    _filter->leading_glyph(Glyphs::Glyph::Search)->placeholder("Filter");
    _filter->fixedWidth = 190.0;

    _scroll = column->append(std::make_unique<Scroll>());
    _scroll->stretch = 1.0;

    _grid = static_cast<Shelf *>(_scroll->hold(std::make_unique<Shelf>(this)));

    // One card and its margins. Under that the shelf is cut rather than squeezed.
    _grid->minWidth = Cards::width + (Theme::bleed * 2.0);
}

bool LibraryPage::profiles() {
    return State::get().nav.shelf == State::Shelf::Profiles;
}

void LibraryPage::land() {
    const int from = _reorder.origin();
    const int to = _reorder.target();
    const int section = _reorder.target_section();
    const int place = _reorder.target_place();

    if (from < 0) {
        _reorder.landed();

        return;
    }

    // Before the grid forgets the drag, and for the cards the rebuild puts in
    // place of these.
    _settle = _reorder.offsets(_cards);
    _settling = to;

    // A folded section hides the card, so the entry at the target is its own.
    if (section >= 0 && _sections[static_cast<size_t>(section)].collapsed) {
        _settle.erase(_settle.begin() + to);
        _settling = -1;
    }

    _reorder.landed();

    if (to != from || section != _fromSection) {
        if (section >= 0 && profiles()) {
            _reach->config.library().dropProfile(from, section, place);
        } else if (section >= 0) {
            _reach->config.library().dropIwad(from, section, place);
        } else if (profiles()) {
            _reach->config.profile().moveProfile(from, to);
        } else {
            _reach->config.lists().moveIwad(from, to);
        }
    }

    wake();
    invalidate();
}

void LibraryPage::settle(const double now) {
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

void LibraryPage::armCarry(components::LibraryCard *card, const int index) {
    card->dragStarted = [this, index](const double x, const double y) {
        _reorder.grabbed(index, x, y);
        _fromSection = _reorder.target_section();
    };

    card->dragMoved = [this](const double x, const double y) {
        _reorder.carried(static_cast<int>(_cards.size()), x, y);
    };

    card->dragEnded = [this] { land(); };
}

void LibraryPage::buildProfile(components::LibraryCard *card, const State::ProfileCard &profile, const int index) {
    const int at = profile.index;

    card->title = profile.name;
    card->artKey = profile.artKey;
    card->caption = profile.iwad.empty() ? "NO GAME" : Text::upper(profile.iwad);
    card->subtitle = profile.engine.empty() ? "No source port" : profile.engine;
    card->playable = profile.ready;
    card->primary = components::LibraryCard::Primary::Open;
    card->setStatus(_reach->runs.stateOf(profile.key), _reach->runs.reasonOf(profile.key));
    card->badges = ProfileBridge::badgesOf(profile);

    card->playHint = profile.ready ? "Launch " + profile.name : "This profile has no source port to run";

    card->actions = {
            Menu::item(ProfileCardAction::Open, "Open", Glyphs::Glyph::Cog),
            Menu::item(ProfileCardAction::Launch, "Launch", Glyphs::Glyph::Play, false, !profile.ready),
            Menu::rule(),
            Menu::item(ProfileCardAction::Duplicate, "Duplicate", Glyphs::Glyph::Extract),
            Menu::item(ProfileCardAction::Rename, "Rename", Glyphs::Glyph::Edit),
            Menu::rule(),
            Menu::item(ProfileCardAction::Shortcut,
                       profile.shortcut ? Shortcut::REMOVE : Shortcut::CREATE,
                       components::shortcutGlyph()),
            Menu::rule(),
            Menu::item(ProfileCardAction::Delete, "Delete", Glyphs::Glyph::Trash, true),
    };

    armProfileActions(card, _reach->runs.alive(profile.key));

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
                _reach->prompt("Rename profile",
                               "Name",
                               State::get().cfg.profileName,
                               "Rename",
                               [this](const std::string &named) { _reach->config.profile().renameProfile(named); });
                break;
            case ProfileCardAction::Shortcut:
                _reach->config.profile().toggleShortcut();

                _reach->touch();
                break;
            case ProfileCardAction::Delete:
                _reach->ask("Delete \"" + State::get().cfg.profileName + "\"?",
                            ProfileBridge::removalNote(),
                            "Delete",
                            true,
                            [this] { _reach->config.profile().removeProfile(); });
                break;
        }
    };

    armCarry(card, index);
}

void LibraryPage::buildGame(components::LibraryCard *card, const State::NameRow &game, const int index) {
    const std::string name = game.name;
    const std::string file = game.file;
    const int at = game.index;
    const bool missing = game.missing;
    const std::string key = ConfigBridge::gameKey(game.name);

    card->title = game.name;
    card->caption = game.kind.empty() ? "FILE" : Text::upper(game.kind);
    card->artKey = game.missing ? std::string() : game.file;
    card->subtitle = State::get().cfg.showPaths ? Format::pretty_path(game.directory) : std::string();
    card->playable = !game.missing;
    card->primary = components::LibraryCard::Primary::Play;
    card->setStatus(_reach->runs.stateOf(key), _reach->runs.reasonOf(key));

    card->badges.clear();

    if (game.missing) {
        card->badges.push_back(State::BadgeSpec{.text = "Missing", .kind = State::BadgeKind::Danger, .dot = true});
    }

    card->playHint =
            game.missing ? "This file is not where the library says it is" : LibraryBridge::gameCommandLine(game.name);

    card->actions = {
            Menu::item(GameCardAction::Play, "Play", Glyphs::Glyph::Play, false, missing),
            Menu::item(GameCardAction::Use, "Use in active profile", Glyphs::Glyph::Check),
            Menu::rule(),
            Menu::item(GameCardAction::Edit, "Edit", Glyphs::Glyph::Edit),
            Menu::item(GameCardAction::Reveal, "Show in file explorer", Glyphs::Glyph::Folder),
            Menu::rule(),
            Menu::item(GameCardAction::Remove, "Remove", Glyphs::Glyph::Trash, true),
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
                _reach->notify.success("\"" + State::get().cfg.profileName + "\" now plays " + name + ".");
                break;
            case GameCardAction::Edit:
                _reach->editGame(
                        "Edit " + name, GameEntry{.name = name, .file = file}, [this, at](const GameEntry &edited) {
                            _reach->config.lists().updateIwad(at, edited.name, edited.file);
                        });
                break;
            case GameCardAction::Reveal:
                Desktop::open(Format::directory_of(file));
                break;
            case GameCardAction::Remove:
                _reach->ask("Remove \"" + name + "\"?",
                            "It goes out of the library and out of every profile that named "
                            "it. The file itself is left where it is.",
                            "Remove",
                            true,
                            [this, at] { _reach->config.lists().removeIwad(at); });
                break;
        }
    };

    armCarry(card, index);
}

void LibraryPage::sync() {
    const State::Cfg &cfg = State::get().cfg;
    const bool onProfiles = profiles();

    _folds.settled();

    _shelfSwitch->set_current(static_cast<int>(State::get().nav.shelf));

    _title->set_text("Library");

    _note->set_text(onProfiles ? components::say(cfg.profileCards.size(), "profile")
                                         + " · press a card to set one up, or the play button to run it"
                               : components::say(cfg.iwads.size(), "game") + " · everything the profiles are built on");

    _newGroup->set_visible(cfg.groupLibrary);
    _newGroup->hint = onProfiles ? "Add a group to sort the profiles into" : "Add a group to sort the games into";

    _addEngine->set_visible(!onProfiles && cfg.engines.empty());
    _engine->set_visible(!onProfiles && !cfg.engines.empty());

    if (_engine->visible()) {
        _engine->set_options(cfg.engineNames);
        _engine->set_badges(cfg.engineBadges);

        const auto found = std::ranges::find(cfg.engineNames, cfg.gameEngine);

        _engine->set_current(found == cfg.engineNames.end() ? -1 : static_cast<int>(found - cfg.engineNames.begin()));
    }

    if (_filter->text() != cfg.filter) {
        _filter->set_text(cfg.filter);
    }

    // A title screen arriving only repaints: rebuilding would drop the card the
    // pointer is on, and with it the hover, the sprites and the tween.
    if (State::get().sys.artRev != _artRev) {
        _artRev = State::get().sys.artRev;

        for (const components::LibraryCard *card : _cards) {
            card->invalidate();
        }
    }

    // Rebuilt only when what the cards are made of has moved.
    const Mark mark{
            .shelf = State::get().nav.shelf,
            .shelfRev = cfg.shelfRev,
            .gameRev = cfg.gameRev,
            .paths = cfg.showPaths,
    };

    if (mark == _mark) {
        // A run starting or ending only repaints the pill on the card it belongs
        // to. Rebuilding the shelf for it would drop the card the pointer is on, and
        // relayout the window behind it. Only on a kept shelf: the cards and the
        // rows are the same length then.
        if (State::get().runs.rev != _runRev) {
            _runRev = State::get().runs.rev;

            for (size_t index = 0; index < _cards.size(); ++index) {
                const std::string key =
                        onProfiles ? cfg.shelfProfiles[index].key : ConfigBridge::gameKey(cfg.shelfGames[index].name);

                _cards[index]->setStatus(_reach->runs.stateOf(key), _reach->runs.reasonOf(key));

                if (onProfiles) {
                    armProfileActions(_cards[index], _reach->runs.alive(key));
                }
            }
        }

        return;
    }

    _mark = mark;
    _runRev = State::get().runs.rev;

    // A rebuild takes a carried card out of the pointer's hand, and no release
    // follows it.
    _reorder.landed();

    _sections = onProfiles ? cfg.shelfProfileSections : cfg.shelfGameSections;

    std::vector<ReorderGrid::Section> runs;

    runs.reserve(_sections.size());

    for (const State::Section &section : _sections) {
        runs.push_back({.cells = section.count, .closed = section.collapsed});
    }

    _reorder.set_sections(std::move(runs));

    // A card is found again by what it stands for, so a filter, an edit or a drop
    // builds the art of only the cards it brings. Only a card at rest is kept: one
    // under the pointer, in hand or on its way is made anew, so it lands at rest.
    std::vector<components::LibraryCard *> had = std::move(_cards);
    std::vector<std::string> hadKeys = std::move(_keys);

    had.insert(had.end(), _spares.begin(), _spares.end());
    hadKeys.insert(
            hadKeys.end(), std::make_move_iterator(_spareKeys.begin()), std::make_move_iterator(_spareKeys.end()));

    std::vector<bool> kept(had.size(), false);
    size_t next = 0;

    _cards.clear();
    _keys.clear();
    _spares.clear();
    _spareKeys.clear();

    const size_t count = onProfiles ? cfg.shelfProfiles.size() : cfg.shelfGames.size();

    // A filtered shelf's neighbours are not the list's.
    const bool reorderable = cfg.filter.empty();

    for (size_t index = 0; index < count; ++index) {
        std::string key = onProfiles ? cfg.shelfProfiles[index].key : ConfigBridge::gameKey(cfg.shelfGames[index].name);
        components::LibraryCard *card = nullptr;

        // From where the last match was, since a shelf mostly keeps its order.
        for (size_t step = 0; step < had.size() && card == nullptr; ++step) {
            const size_t at = (next + step) % had.size();

            if (!kept[at] && hadKeys[at] == key && had[at]->resting()) {
                kept[at] = true;
                card = had[at];
                next = at + 1;
            }
        }

        if (card == nullptr) {
            card = _grid->append(std::make_unique<components::LibraryCard>());
            card->artwork = [this](const std::string &art) { return _reach->art.of(art); };
        } else {
            // Placed afresh rather than walked from its old place.
            card->setSlot(-1);
            card->set_visible(true);
        }

        card->draggable = reorderable;

        if (onProfiles) {
            buildProfile(card, cfg.shelfProfiles[index], static_cast<int>(index));
        } else {
            buildGame(card, cfg.shelfGames[index], static_cast<int>(index));
        }

        _cards.push_back(card);
        _keys.push_back(std::move(key));
    }

    // A card the filter hides keeps its art for when the filter goes, which is no more
    // than the whole shelf holds. Not one the keys are on: hidden, it would keep them.
    const auto listed = [&cfg, onProfiles](const std::string &key) {
        return onProfiles ? std::ranges::any_of(cfg.profileCards,
                                                [&key](const State::ProfileCard &row) { return row.key == key; })
                          : std::ranges::any_of(cfg.iwads, [&key](const State::NameRow &row) {
                                return ConfigBridge::gameKey(row.name) == key;
                            });
    };

    for (size_t at = 0; at < had.size(); ++at) {
        if (kept[at]) {
            continue;
        }

        components::LibraryCard *card = had[at];

        if (!cfg.filter.empty() && card->resting() && (root() == nullptr || root()->focused() != card)
            && listed(hadKeys[at])) {
            card->set_visible(false);
            card->dropSheet();

            _spares.push_back(card);
            _spareKeys.push_back(std::move(hadKeys[at]));
        } else {
            _grid->erase(card);
        }
    }

    if (root() != nullptr) {
        root()->relayout();
    }
}

BLRect LibraryPage::bandBox(const size_t section) const {
    const BLRect strip = _reorder.header(static_cast<int>(section));

    return BLRect{strip.x, strip.y - lift(section), strip.w, Band::LINE};
}

float LibraryPage::shown(const size_t section) const {
    return _folds.open(_sections[section].group, _sections[section].collapsed);
}

double LibraryPage::body(const size_t section) const {
    const State::Section &at = _sections[section];

    return _reorder.rows_for(at.collapsed ? 0 : at.count) * (Cards::rowHeight + Theme::gutter);
}

double LibraryPage::lift(const size_t section) const {
    double up = 0.0;

    for (size_t above = 0; above < section && !_folds.empty(); ++above) {
        up += body(above) * (1.0 - shown(above));
    }

    return up;
}

int LibraryPage::sectionOf(const int index) const {
    if (_folds.empty() || _sections.empty()) {
        return -1;
    }

    return static_cast<int>(std::ranges::upper_bound(_sections, index, {}, &State::Section::first) - _sections.begin())
           - 1;
}

BLRect LibraryPage::bodyBox(const size_t section) const {
    const double top = bandBox(section).y + Band::LINE;

    return BLRect{_grid->box().x, top, _grid->box().w, Band::STRIP - Band::LINE + (body(section) * shown(section))};
}

int LibraryPage::litBand() const {
    const int into = _reorder.target_section();

    return into != _fromSection ? into : -1;
}

BLRect LibraryPage::adderBox() const {
    const int count = static_cast<int>(_cards.size());
    const int section = sectionOf(count);

    return BLRect{_reorder.cell_x(count),
                  _reorder.cell_y(count) - (section < 0 ? 0.0 : lift(static_cast<size_t>(section))),
                  _reorder.cell(),
                  Cards::rowHeight};
}

void LibraryPage::paintAdder(const Painter &painter, const bool lit) const {
    const Theme::Palette &palette = Theme::palette();
    const BLRect box = adderBox();

    if (!painter.needed(box)) {
        return;
    }

    painter.round(box, Theme::radius, lit ? Theme::mix(palette.sunken, palette.hover, 0.85) : palette.sunken);
    painter.outline(box, Theme::radius, 1.0, lit ? palette.accent : palette.borderStrong);

    const BLPoint middle{box.x + (box.w / 2.0), box.y + (box.h / 2.0) - 14.0};

    painter.circle(middle, 23.0, lit ? palette.accentSoft : palette.surface);
    painter.context().set_stroke_width(1.0);
    painter.context().stroke_circle(middle.x, middle.y, 22.5, lit ? palette.accent : palette.borderStrong);

    const double side = Glyphs::span(1.5F);

    Glyphs::draw(painter.context(),
                 Glyphs::Glyph::Plus,
                 BLPoint{middle.x - (side / 2.0), middle.y - (side / 2.0)},
                 1.5F,
                 lit ? palette.accent : palette.muted);

    painter.label(painter.font(600, Theme::fontSmall),
                  BLRect{box.x, middle.y + 33.0, box.w, 20.0},
                  Align::Centre,
                  profiles() ? "New profile" : "Add a game",
                  lit ? palette.accent : palette.muted);
}

void LibraryPage::paintNothing(const Painter &painter) const {
    const State::Cfg &cfg = State::get().cfg;

    if (!cfg.filter.empty() && _cards.empty()) {
        const Theme::Palette &palette = Theme::palette();
        const BLRect box = adderBox();
        const double wide = std::min(420.0, _scroll->box().w - 60.0);
        const double x = _scroll->box().x + ((_scroll->box().w - wide) / 2.0);
        double y = box.y + box.h + 28.0;

        const BLFont &heading = painter.font(600, Theme::fontMedium);

        painter.label(heading,
                      BLRect{x, y, wide, painter.line_height(heading)},
                      Align::Start,
                      "Nothing called that",
                      palette.muted);

        y += painter.line_height(heading) + 6.0;

        painter.paragraph(painter.font(400, Theme::fontSmall),
                          BLRect{x, y, wide, 0.0},
                          "No " + std::string(profiles() ? "profile" : "game") + " here has \"" + cfg.filter
                                  + "\" in its name.",
                          palette.faint);
    }
}

void LibraryPage::addPressed() const {
    if (profiles()) {
        _reach->prompt("New profile", "Name", "New profile", "Create", [this](const std::string &named) {
            _reach->config.profile().addProfile(named);
            _reach->go(State::Page::Profile);
        });
    } else {
        _reach->files.open("Add games",
                           Filters::game(),
                           false,
                           false,
                           true,
                           LastDir::WAD,
                           [this](const std::vector<std::string> &paths, bool) {
                               _reach->config.lists().addIwads(paths);
                               _reach->touch();
                           });
    }
}

void LibraryPage::paint(const Painter &painter) {
    Widget::paint(painter);

    if (_filesOver) {
        DropMark::paint(painter, _scroll->box(), Theme::radius);
    }
}

bool LibraryPage::drag_files(const Pointer &at) {
    if (!_scroll->holds(at.x, at.y)) {
        return false;
    }

    if (!_filesOver) {
        _filesOver = true;
        DropMark::invalidate(*this, _scroll->box(), Theme::radius);
    }

    return true;
}

void LibraryPage::drag_files_ended() {
    if (_filesOver) {
        _filesOver = false;
        DropMark::invalidate(*this, _scroll->box(), Theme::radius);
    }
}

// The Profiles shelf takes .zdl files and the Games shelf takes games.
bool LibraryPage::drop_files(const std::vector<std::string> &paths, const Pointer &at) {
    if (!_scroll->holds(at.x, at.y)) {
        return false;
    }

    const bool zdl = profiles();
    const Filters::Dropped dropped = Filters::split(paths, zdl ? Filters::zdl() : Filters::game(), false);

    if (!dropped.left.empty()) {
        _reach->notify.warning(zdl ? Filters::leftOut(dropped.left, "a .zdl file", ".zdl files")
                                   : Filters::leftOut(dropped.left, "a game", "games"));
    }

    if (zdl && !dropped.taken.empty()) {
        _reach->config.profile().loadZdls(dropped.taken);
    } else if (!dropped.taken.empty()) {
        _reach->config.lists().addIwads(dropped.taken);
    }

    _reach->touch();

    return true;
}

bool LibraryPage::advance(const double now) {
    settle(now);

    const bool sliding = !_folds.empty();
    const bool folding = _folds.advance(now);

    // A slide moves every section below it, and the shelf grows or shrinks with it.
    if (sliding && root() != nullptr) {
        root()->relayout();
        _grid->invalidate();
    }

    // The shelf repaints what actually moved.
    if (root() != nullptr) {
        _grid->arrange(root()->type());
    }

    if (_reorder.dragging()) {
        return true;
    }

    const bool walking =
            std::ranges::any_of(_cards, [](const components::LibraryCard *card) { return card->sliding(); });

    if (!walking) {
        _settling = -1;
    }

    return walking || folding;
}

}
