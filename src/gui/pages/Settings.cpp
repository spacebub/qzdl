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
#include "gui/app/App.h"
#include "gui/draw/Paint.h"
#include "gui/pages/Settings.h"
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

    return Format::sameFile(path, App::state().cfg.systemDosbox) ? "detected" : "custom";
}

SettingsPage::SettingsPage(App *app) : _app(app) {
    _mark = Paint::load(std::string(ZDL_ASSET_DIR) + "/qzdl-128.png");

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
        _app->config().settings().setAlwaysAdd(value);
    }));

    _always->placeholder("Added to every launch, whatever the profile")->mono();

    _dosbox = fields->append(std::make_unique<Field>("DOSBox", [this](const std::string &value) {
        _app->config().settings().setDosbox(value);
    }));

    _dosbox->placeholder("Only for source ports that are DOS programs")->mono();

    _dosbox->icon("folder", "Browse", [this] {
        _app->picker().open("dosbox", "Select DOSBox", App::portFilters(), false, false, false,
                            "src");
    });

    inside->append(std::make_unique<Rule>());

    Box *switches = inside->append(Box::column());

    switches->spacing(18.0);

    Box *first = switches->append(std::make_unique<Pair>(560.0));

    first->spacing(GUTTER);

    _closing = first->append(std::make_unique<Toggle>("Close on launch", [this](const bool on) {
        _app->config().settings().setAutoClose(on);
    }));

    _closing->hint = "Quit ZDL4 as soon as the source port has started";

    _paths = first->append(std::make_unique<Toggle>("Show file paths", [this](const bool on) {
        _app->config().settings().setShowPaths(on);
    }));

    _paths->hint = "Show the directory a file came from underneath its name in Games and Add-ons";

    Box *second = switches->append(std::make_unique<Pair>(560.0));

    second->spacing(GUTTER);

    _atOnce = second->append(std::make_unique<Toggle>("Launch .zdl files at once",
                                                      [this](const bool on) {
        _app->config().settings().setLaunchZdlImmediately(on);
    }));

    _atOnce->hint = "A .zdl given on the command line launches without showing this window";

    _perProfile = second->append(std::make_unique<Toggle>("Per profile port config",
                                                          [this](const bool on) {
        _app->config().settings().setProfileConfigs(on);
    }));

    _perProfile->hint = "Each profile keeps the source port's settings in a file of its own, "
                        "instead of every profile sharing one";

    Box *third = switches->append(std::make_unique<Pair>(560.0));

    third->spacing(GUTTER);

    _hardware = third->append(std::make_unique<Toggle>("Hardware acceleration",
                                                       [this](const bool on) {
        _app->config().settings().setHardwareRendering(on);
    }));

    _hardware->hint = "Requires restart. The window's pixels are uploaded through the video "
                      "driver rather than blitted from plain memory: more RAM for the graphics "
                      "stack, and the only path there is on Wayland and macOS.";

    // The empty half, so the switch lines up with the ones above it.
    third->append(std::make_unique<Spacer>(0.0));

    inside->append(std::make_unique<Rule>());

    Box *opens = inside->append(Box::row());

    opens->spacing(12.0)->cross(Box::Place::Centre);
    opens->fixedHeight = Theme::control;

    opens->append(std::make_unique<Label>("Open the library on"))
        ->font(400, Theme::fontBody)
        ->tone(Theme::of().text);

    opens->append(std::make_unique<Spacer>());

    _startView = opens->append(std::make_unique<Segmented>([this](const std::string &key) {
        _app->config().settings().setStartView(key);
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
        Desktop::open(Format::directoryOf(App::state().cfg.path));
    });

    Box *buttons = where->append(Box::row());

    buttons->spacing(8.0);
    buttons->fixedHeight = Theme::controlSmall;

    buttons->append(std::make_unique<Button>("Open a config", [this] {
        _app->picker().open("load-config", "Open a config file", App::configFilters(), false,
                            false, false, "config");
    }))->glyph("folder")->compact()->tip("Work on a different config file from here on");

    buttons->append(std::make_unique<Button>("Save as", [this] {
        _app->picker().openSave("save-config", "Save the config as", App::configFilters(),
                                "config", Format::fileName(App::state().cfg.path));
    }))->glyph("save")->compact()
        ->tip("Write this config somewhere else and work on it there from now on");

    _adopt = buttons->append(std::make_unique<Button>("Use as the user config", [this] {
        _app->ask("Use this as the user config?",
                  "This config replaces the one ZDL4 opens by default, at "
                      + Format::prettyPath(App::state().cfg.path) + ".",
                  "Replace it", false,
                  [this] { _app->config().settings().adoptAsUserConfig(); });
    }));

    _adopt->glyph("check")->compact();

    buttons->append(std::make_unique<Button>("Clear everything", [this] {
        _app->ask("Clear everything?",
                  "Every profile, every game and every source port is removed. Nothing on disk "
                  "is touched, but this config is emptied and cannot be got back.",
                  "Clear everything", true,
                  [this] { _app->config().settings().clearEverything(); });
    }))->kind(Button::Kind::Danger)->glyph("trash")->compact()
        ->tip("Empties this config: every profile with the files and settings in it, every "
              "game, and every source port. The wads and the ports themselves are left where "
              "they are");

    buttons->append(std::make_unique<Spacer>());

    where->append(std::make_unique<Rule>());

    _ignoreUser = where->append(std::make_unique<Toggle>("Skip the user config at startup",
                                                         [this](const bool on) {
        _app->config().settings().setIgnoreUserConfig(on);
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
        Desktop::open(App::state().ports.downloads);
    });

    _kept = told->append(std::make_unique<Label>());
    _kept->font(400, Theme::fontSmall)->tone(Theme::of().faint);

    _empty = row->append(std::make_unique<Button>("Empty it", [this] {
        _app->ask("Empty the downloads?",
                  "The archives ZDL4 fetched are deleted. Every port already unpacked stays "
                  "where it is, and anything fetched after this comes down the wire afresh.",
                  "Empty it", true, [this] { _app->engines().clearDownloads(); });
    }));

    _empty->kind(Button::Kind::Danger)->glyph("trash")->compact();

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
        _app->showAbout();

        _app->touch();
    }))->compact();
}

void SettingsPage::sync() {
    const State::Cfg &cfg = App::state().cfg;

    if (!_measured) {
        _measured = true;

        _app->engines().measure();
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
                   kind == "none"       ? "warning"
                   : kind == "missing"  ? "danger"
                   : kind == "detected" ? "success"
                                        : "");

    _closing->setChecked(cfg.autoClose);
    _paths->setChecked(cfg.showPaths);
    _atOnce->setChecked(cfg.launchZdlImmediately);
    _perProfile->setChecked(cfg.profileConfigs);
    _hardware->setChecked(cfg.hardwareRendering);
    _ignoreUser->setChecked(cfg.ignoreUserConfig);

    _startView->setCurrent(cfg.startView == StartView::GAMES ? "games" : "profiles");

    _configFile->setValue(cfg.path);

    _adopt->setEnabled(!cfg.userConfig);
    _adopt->tip(cfg.userConfig ? "This is already the one ZDL4 opens by default"
                               : "Make this the one ZDL4 opens by default");

    _downloads->setValue(App::state().ports.downloads);
    _kept->setText(App::state().ports.cachedText
                   + " kept · what a port was fetched from stays here, so fetching it again "
                     "does not bring it down twice");

    _empty->setEnabled(App::state().ports.cached);
    _empty->tip(App::state().ports.cached
                    ? "Delete what was downloaded. The ports already unpacked are left alone"
                    : "There is nothing being kept");

    _version->setText("ZDL4 " + App::state().sys.version);
    _blurb->setText("A launcher for Doom engine source ports · " + App::state().sys.runtime);
}

}
