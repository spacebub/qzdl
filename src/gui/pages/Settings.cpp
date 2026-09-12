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

#include "core/config/Schema.h"
#include "gui/app/Filters.h"
#include "gui/draw/Mark.h"
#include "gui/pages/Settings.h"
#include "gui/state/State.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/Fact.h"
#include "gui/toolkit/controls/Field.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/controls/Segmented.h"
#include "gui/toolkit/controls/Toggle.h"
#include "gui/toolkit/layout/Box.h"
#include "gui/toolkit/layout/Pair.h"
#include "gui/toolkit/layout/Panel.h"
#include "gui/toolkit/layout/Picture.h"
#include "gui/toolkit/layout/Scroll.h"
#include "gui/toolkit/layout/Spacer.h"
#include "gui/toolkit/layout/Wrap.h"
#include "gui/util/Desktop.h"
#include "gui/util/Format.h"

namespace {

constexpr double BLEED = 16.0;
constexpr double GUTTER = 20.0;

// A heading inside a panel.
toolkit::Label *panelTitle(toolkit::Box *into, const std::string &text) {
    toolkit::Label *made = into->append(std::make_unique<toolkit::Label>(text));

    made->font(Theme::of().headingWeight, Theme::fontMedium)->tone(Theme::of().text);

    return made;
}

// The hairline every panel divides itself with.
class Rule : public toolkit::Widget {
public:
    Rule() { fixedHeight = 1.0; }

    void paint(const toolkit::Painter &painter) override {
        painter.fill(BLRect{_box.x, _box.y, _box.w, 1.0}, Theme::of().border);
    }
};

}

namespace pages {

using namespace toolkit;

std::string SettingsPage::dosboxKind(const std::string &path) {
    if (path.empty()) {
        return "none";
    }

    if (!Format::isFile(path)) {
        return "missing";
    }

    return Format::sameFile(path, State::get().cfg.systemDosbox) ? "detected" : "custom";
}

SettingsPage::SettingsPage(Reach *reach) : _reach(reach), _mark(Mark::of(128)) {
    Box *column = append(Box::column());

    column->spacing(16.0);

    Box *head = column->append(Box::column());

    head->spacing(3.0)->pad(BLEED, 0.0);

    head->append(std::make_unique<Label>("Settings"))
        ->font(Theme::of().headingWeight, Theme::fontDisplay)
        ->tone(Theme::of().text);

    head->append(std::make_unique<Label>("How launching behaves, and where all of it is kept"))
        ->font(400, Theme::fontSmall)
        ->tone(Theme::of().faint);

    _scroll = column->append(std::make_unique<Scroll>());
    _scroll->stretch = 1.0;

    _body = static_cast<Box *>(_scroll->hold(Box::column()));
    _body->spacing(16.0)->pad(BLEED, 0.0, BLEED, 8.0);
    _body->minWidth = 360.0 + (BLEED * 2.0);

    // --- Behaviour ---

    Panel *behaviour = _body->append(std::make_unique<Panel>());
    Box *inside = behaviour->append(Box::column());

    inside->pad(16.0)->spacing(16.0);

    panelTitle(inside, "Behaviour");

    Box *fields = inside->append(std::make_unique<Pair>(760.0));

    fields->spacing(GUTTER)->cross(Box::Place::End);

    _always = fields->append(std::make_unique<Field>("Always add these arguments",
                                                     [this](const std::string &value) {
        _reach->config.settings().setAlwaysAdd(value);
    }));

    _always->placeholder("Added to every launch, whatever the profile")->mono();

    _dosbox = fields->append(std::make_unique<Field>("DOSBox", [this](const std::string &value) {
        _reach->config.settings().setDosbox(value);
    }));

    _dosbox->placeholder("Only for source ports that are DOS programs")->mono();

    _dosbox->icon(Glyphs::Glyph::Folder, "Browse", [this] {
        _reach->picker.open(FilePicker::Action::Dosbox, "Select DOSBox", Filters::port(), false, false, false,
                            FilePicker::Slot::Src);
    });

    inside->append(std::make_unique<Rule>());

    Box *switches = inside->append(Box::column());

    switches->spacing(18.0);

    Box *first = switches->append(std::make_unique<Pair>(560.0));

    first->spacing(GUTTER);

    _closing = first->append(std::make_unique<Toggle>("Close on launch", [this](const bool on) {
        _reach->config.settings().setAutoClose(on);
    }));

    _closing->hint = "Quit ZDL4 as soon as the source port has started";

    _paths = first->append(std::make_unique<Toggle>("Show file paths", [this](const bool on) {
        _reach->config.settings().setShowPaths(on);
    }));

    _paths->hint = "Show the directory a file came from underneath its name in Games and Add-ons";

    Box *second = switches->append(std::make_unique<Pair>(560.0));

    second->spacing(GUTTER);

    _atOnce = second->append(std::make_unique<Toggle>("Launch .zdl files at once",
                                                      [this](const bool on) {
        _reach->config.settings().setLaunchZdlImmediately(on);
    }));

    _atOnce->hint = "A .zdl given on the command line launches without showing this window";

    _perProfile = second->append(std::make_unique<Toggle>("Per profile port config",
                                                          [this](const bool on) {
        _reach->config.settings().setProfileConfigs(on);
    }));

    _perProfile->hint = "Each profile keeps the source port's settings in a file of its own, "
                        "instead of every profile sharing one";

    inside->append(std::make_unique<Rule>());

    Box *opens = inside->append(Box::row());

    opens->spacing(12.0)->cross(Box::Place::Centre);
    opens->fixedHeight = Theme::control;

    opens->append(std::make_unique<Label>("Open the library on"))
        ->font(400, Theme::fontBody)
        ->tone(Theme::of().text);

    opens->append(std::make_unique<Spacer>());

    _startView = opens->append(std::make_unique<Segmented>([this](const std::string &key) {
        _reach->config.settings().setStartView(key);
    }));

    _startView->setOptions({{.key = "profiles", .label = "Profiles"},
                            {.key = "games", .label = "Games"}});

    // --- This config ---

    Panel *kept = _body->append(std::make_unique<Panel>());
    Box *where = kept->append(Box::column());

    where->pad(16.0)->spacing(16.0);

    panelTitle(where, "This config");

    _configFile = where->append(std::make_unique<Fact>("Configuration file", ""));
    _configFile->path()->onClick("Open the directory it is in", [] {
        Desktop::open(Format::directoryOf(State::get().cfg.path));
    });

    Wrap *buttons = where->append(std::make_unique<Wrap>());

    buttons->spacing(8.0, 8.0);

    buttons->append(std::make_unique<Button>("Open a config", [this] {
        _reach->picker.open(FilePicker::Action::LoadConfig, "Open a config file", Filters::config(), false,
                            false, false, FilePicker::Slot::Config);
    }))->glyph(Glyphs::Glyph::Folder)->compact()->tooltip("Work on a different config file from here on");

    buttons->append(std::make_unique<Button>("Save as", [this] {
        _reach->picker.openSave(FilePicker::Action::SaveConfig, "Save the config as", Filters::config(),
                                FilePicker::Slot::Config, Format::fileName(State::get().cfg.path));
    }))->glyph(Glyphs::Glyph::Save)->compact()
        ->tooltip("Write this config somewhere else and work on it there from now on");

    _adopt = buttons->append(std::make_unique<Button>("Use as the user config", [this] {
        _reach->ask("Use this as the user config?",
                  "This config replaces the one ZDL4 opens by default, at "
                      + Format::prettyPath(State::get().cfg.path) + ".",
                  "Replace it", false,
                  [this] { _reach->config.settings().adoptAsUserConfig(); });
    }));

    _adopt->glyph(Glyphs::Glyph::Check)->compact();

    buttons->append(std::make_unique<Button>("Clear everything", [this] {
        _reach->ask("Clear everything?",
                  "Every profile, every game and every source port is removed. Nothing on disk "
                  "is touched, but this config is emptied and cannot be got back.",
                  "Clear everything", true,
                  [this] { _reach->config.settings().clearEverything(); });
    }))->kind(Button::Kind::Danger)->glyph(Glyphs::Glyph::Trash)->compact()
        ->tooltip("Empties this config: every profile with the files and settings in it, every "
              "game, and every source port. The wads and the ports themselves are left where "
              "they are");

    where->append(std::make_unique<Rule>());

    _ignoreUser = where->append(std::make_unique<Toggle>("Skip the user config at startup",
                                                         [this](const bool on) {
        _reach->config.settings().setIgnoreUserConfig(on);
    }));

    _ignoreUser->hint = "ZDL4 loads a portable config kept next to its program instead. If "
                        "there is none there, it falls back to the user config anyway";

    // --- Downloads ---

    Panel *shelf = _body->append(std::make_unique<Panel>());
    Box *cache = shelf->append(Box::column());

    cache->pad(16.0)->spacing(14.0);

    panelTitle(cache, "Downloads");

    Box *row = cache->append(Box::row());

    row->spacing(14.0)->cross(Box::Place::Centre);

    Box *told = row->append(Box::column());

    told->spacing(6.0);
    told->stretch = 1.0;

    _downloads = told->append(std::make_unique<Fact>("Where they are kept", ""));
    _downloads->path()->onClick("Open the directory", [] {
        Desktop::open(State::get().ports.downloads);
    });

    _kept = told->append(std::make_unique<Label>());
    _kept->font(400, Theme::fontSmall)->tone(Theme::of().faint);

    _empty = row->append(std::make_unique<Button>("Empty it", [this] {
        _reach->ask("Empty the downloads?",
                  "The archives ZDL4 fetched are deleted. Every port already unpacked stays "
                  "where it is, and anything fetched after this comes down the wire afresh.",
                  "Empty it", true, [this] { _reach->engines.clearDownloads(); });
    }));

    _empty->kind(Button::Kind::Danger)->glyph(Glyphs::Glyph::Trash)->compact();

    // --- The footer ---

    Panel *about = _body->append(std::make_unique<Panel>());

    about->fixedHeight = 76.0;

    Box *footer = about->append(Box::row());

    footer->pad(16.0, 0.0)->spacing(14.0)->cross(Box::Place::Centre);

    Picture *mark = footer->append(std::make_unique<Picture>(_mark));

    mark->fixedWidth = 40.0;
    mark->fixedHeight = 40.0;

    Box *said = footer->append(Box::column());

    said->spacing(2.0)->align(Box::Place::Centre);
    said->stretch = 1.0;

    _version = said->append(std::make_unique<Label>());
    _version->font(600, Theme::fontBody)->tone(Theme::of().text);

    _blurb = said->append(std::make_unique<Label>());
    _blurb->font(400, Theme::fontSmall)->tone(Theme::of().faint);

    footer->append(std::make_unique<Button>("Project page", [] {
        Desktop::open("https://github.com/spacebub/qzdl");
    }))->kind(Button::Kind::Ghost)->compact();

    footer->append(std::make_unique<Button>("About", [this] {
        _reach->showAbout();

        _reach->touch();
    }))->compact();
}

void SettingsPage::sync() {
    const State::Cfg &cfg = State::get().cfg;

    if (!_measured) {
        _measured = true;

        _reach->engines.measure();
    }

    if (_always->text() != cfg.alwaysAdd) {
        _always->setText(cfg.alwaysAdd);
    }

    const std::string dosbox = cfg.dosbox.empty() ? cfg.systemDosbox : cfg.dosbox;

    if (_dosbox->text() != dosbox) {
        _dosbox->setText(dosbox);
    }

    const std::string kind = dosboxKind(dosbox);

    _dosbox->badge(kind == "none"       ? "Not found"
                   : kind == "missing"  ? "Missing"
                   : kind == "detected" ? "Detected"
                                        : "Custom",
                   kind == "none"       ? Pill::Kind::Warning
                   : kind == "missing"  ? Pill::Kind::Danger
                   : kind == "detected" ? Pill::Kind::Success
                                        : Pill::Kind::None);

    _closing->setChecked(cfg.autoClose);
    _paths->setChecked(cfg.showPaths);
    _atOnce->setChecked(cfg.launchZdlImmediately);
    _perProfile->setChecked(cfg.profileConfigs);
    _ignoreUser->setChecked(cfg.ignoreUserConfig);

    _startView->setCurrent(cfg.startView == StartView::GAMES ? "games" : "profiles");

    _configFile->setValue(cfg.path);

    _adopt->setEnabled(!cfg.userConfig);
    _adopt->tooltip(cfg.userConfig ? "This is already the one ZDL4 opens by default"
                               : "Make this the one ZDL4 opens by default");

    _downloads->setValue(State::get().ports.downloads);
    _kept->setText(State::get().ports.cachedText
                   + " kept · what a port was fetched from stays here, so fetching it again "
                     "does not bring it down twice");

    _empty->setEnabled(State::get().ports.cached);
    _empty->tooltip(State::get().ports.cached
                    ? "Delete what was downloaded. The ports already unpacked are left alone"
                    : "There is nothing being kept");

    _version->setText("ZDL4 " + State::get().sys.version);
    _blurb->setText("A launcher for Doom engine source ports · " + State::get().sys.runtime);
}

}
