/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023  spacebub
 * 
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>
#include "wad/ZDLMapFile.h"
#include "config/ZDLConfigurationManager.h"
#include "ui/ZDLInputWidgets.h"
#include "ui/ZDLSettingsPane.h"

void
AlwaysFocusedDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QStyleOptionViewItem new_option(option);
    if (new_option.state.testFlag(QStyle::State_Selected)) {
        new_option.state = new_option.state | QStyle::State_Active;
    }
    QItemDelegate::paint(painter, new_option, index);
}

ZDLSettingsPane::ZDLSettingsPane(QWidget *parent) :
        ZDLWidget(parent),
        diffList(new QComboBox(this)),
        monstersList(new QComboBox(this)),
        sourceList(new QComboBox(this)),
        IWADList(new DeselectableListWidget(this)),
        warpCombo(new VerboseComboBox(this)) {
    LOGDATAO() << "New ZDLSettingsPane" << Qt::endl;
    auto *box = new QVBoxLayout(this);
    setContentsMargins(0, 0, 0, 0);
    layout()->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(2);

    box->addWidget(new QLabel("Source port", this));

    box->addWidget(sourceList);

    box->addWidget(new QLabel("IWAD", this));

    IWADList->setItemDelegate(new AlwaysFocusedDelegate());
    connect(IWADList, SIGNAL(currentRowChanged(int)), this, SLOT(iwadRowChanged(int)));
    box->addWidget(IWADList);

    auto *box2 = new QHBoxLayout();
    box->addLayout(box2);

    auto *warpBox = new QVBoxLayout();
    box2->addLayout(warpBox);

    auto *skillBox = new QVBoxLayout();
    box2->addLayout(skillBox);

    auto *monstersBox = new QVBoxLayout();
    box2->addLayout(monstersBox);

    connect(warpCombo, SIGNAL(activated(int)), this, SLOT(currentRowChanged(int)));
    connect(warpCombo, SIGNAL(onPopup()), this, SLOT(VerbosePopup()));
    connect(warpCombo, SIGNAL(onHidePopup()), this, SLOT(HidePopup()));
    warpCombo->setInsertPolicy(QComboBox::NoInsert);
    warpCombo->setEditable(true);
    warpCombo->setValidator(new EvilValidator(this));
    warpCombo->setCompleter(nullptr);
    warpCombo->lineEdit()->setPlaceholderText("(Default)");
    warpCombo->addItem("(Default)");
    warpBox->addWidget(new QLabel("Map", this));
    warpBox->addWidget(warpCombo);
    warpBox->setSpacing(2);

    diffList->addItem("(Default)");
    diffList->addItem("V. Easy");
    diffList->addItem("Easy");
    diffList->addItem("Medium");
    diffList->addItem("Hard");
    diffList->addItem("V. Hard");
    skillBox->addWidget(new QLabel("Skill", this));
    skillBox->addWidget(diffList);

    monstersList->addItem("(Default)");
    monstersList->addItem("No");
    monstersList->addItem("Fast");
    monstersList->addItem("Respawn");
    monstersList->addItem("Fast & Respawn");
    monstersBox->addWidget(new QLabel("Monsters", this));
    monstersBox->addWidget(monstersList);

    LOGDATAO() << "Done" << Qt::endl;
}

void DeselectableListWidget::mousePressEvent(QMouseEvent *event) {
    const QListWidgetItem *item = itemAt(event->pos());

    if ((item != nullptr) && item->isSelected()) {
        QListWidget::mousePressEvent(event);
        //clearSelection();
        setCurrentRow(-1);
    } else {
        QListWidget::mousePressEvent(event);
    }
}

void ZDLSettingsPane::VerbosePopup() {
    warpCombo->lineEdit()->setPlaceholderText("");
    QString const current = warpCombo->currentText();
    int idx = 0;
    emit buildParent(this);
    warpCombo->setUpdatesEnabled(false);
    reloadMapList();
    if (current.isEmpty()) {
        warpCombo->setCurrentIndex(0);
        warpCombo->clearEditText();
    } else if (idx = warpCombo->findText(current, Qt::MatchFixedString); idx > 0) {
        warpCombo->setCurrentIndex(idx);
    } else {
        warpCombo->setEditText(current);
    }
    warpCombo->setUpdatesEnabled(true);
}

void ZDLSettingsPane::HidePopup() {
    warpCombo->lineEdit()->setPlaceholderText("(Default)");
}

void ZDLSettingsPane::currentRowChanged(int idx) {
    if (idx == 0) {
        warpCombo->setCurrentIndex(-1);
    }
}

void ZDLSettingsPane::iwadRowChanged(int row) {
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }

    // An empty name means the selection was cleared, which unbinds the profile.
    QString name;
    if (row >= 0 && row < config->iwads.size()) {
        name = config->iwads[row].name;
    }

    // ZDLInterface decides what this means for the profile; it owns the profile
    // selector and the reload that a switch implies.
    emit iwadSelected(name);
}

QStringList ZDLSettingsPane::getFilesMaps() {
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return {};
    }

    QStringList maps;
    for (const ZDLFileEntry &entry: std::as_const(config->activeProfile().files)) {
        // Disabled files aren't loaded, so their maps aren't reachable either.
        if (!entry.enabled) {
            continue;
        }
        if (ZDLMapFile *mapfile = ZDLMapFile::getMapFile(entry.file)) {
            maps += mapfile->getMapNames();
            delete mapfile;
        }
    }
    return maps;
}

bool ZDLSettingsPane::naturalSortLess(const QString &left, const QString &right) {
    //Less comparison algorithm for natural sorting
    //Based on "The Alphanum Algorithm" by David Koelle
    //http://www.davekoelle.com/alphanum.html
    //Released under MIT License (https://opensource.org/licenses/MIT)

    bool mode_letter = true;    //If it's not letter mode, then it's digit mode
    QString::const_iterator li = left.begin();
    QString::const_iterator ri = right.begin();
    bool l_is_digit = false;
    bool r_is_digit = false;
    unsigned int l_as_uint = 0;
    unsigned int r_as_uint = 0;
    unsigned int l_digits = 0;
    unsigned int r_digits = 0;

    while (li != left.end() && ri != right.end()) {
        if (mode_letter) {
            while (li != left.end() && ri != right.end()) {
                //Check if these are digit characters
                l_is_digit = li->isDigit();
                r_is_digit = ri->isDigit();

                //If both characters are digits, we continue in digit mode
                if (l_is_digit && r_is_digit) {
                    mode_letter = false;
                    break;
                }

                //If one of the characters is a digit, we have a result
                if (l_is_digit) {
                    return true;
                }
                if (r_is_digit) {
                    return false;
                }

                //Else, compare both characters and if they differ we have a result
                if (*li < *ri) {
                    return true;
                }
                if (*li > *ri) {
                    return false;
                }

                //Otherwise, process next characters
                ++li;
                ++ri;
            }
        } else {
            //Get left and right numbers
            //To prevent overflow we process maximum of 9 digits (ignoring leading zeroes)
            //It's ok for WAD map names because they are limited to 8 characters anyway

            l_as_uint = 0;
            l_digits = 0;
            while (li != left.end() && li->isDigit() && l_digits < 9) {
                l_as_uint = (l_as_uint * 10) + li->digitValue();
                if (l_as_uint != 0U) {
                    l_digits++;
                }
                ++li;
            }

            r_as_uint = 0;
            r_digits = 0;
            while (ri != right.end() && ri->isDigit() && r_digits < 9) {
                r_as_uint = (r_as_uint * 10) + ri->digitValue();
                if (r_as_uint != 0U) {
                    r_digits++;
                }
                ++ri;
            }

            //If numbers differ, we have a comparison result
            if (l_as_uint < r_as_uint) {
                return true;
            }
            if (l_as_uint > r_as_uint) {
                return false;
            }

            //Otherwise we process the next substring in letter mode
            mode_letter = true;
        }
    }

    //We got here, so one of the strings (or both) is out of characters
    //If right string still has some characters left, then left string is out of characters, so it is "less" than right
    //Otherwise right is "less" then left or "equal" to it
    return ri != right.end();
}

void ZDLSettingsPane::reloadMapList() {
    LOGDATAO() << "reloadMapList START" << Qt::endl;

    warpCombo->clear();
    warpCombo->addItem("(Default)");

    QStringList wadMaps;

    if (const QListWidgetItem *item = IWADList->currentItem()) {
        if (ZDLMapFile *mapfile = ZDLMapFile::getMapFile(item->data(32).toString())) {
            wadMaps += mapfile->getMapNames();
            delete mapfile;
        }
    }

    wadMaps.append(getFilesMaps());

    if (!wadMaps.empty()) {
        std::ranges::sort(wadMaps, naturalSortLess);
        wadMaps.removeDuplicates();
        warpCombo->addItems(wadMaps);
    }

    warpCombo->setCurrentIndex(-1);
}

void ZDLSettingsPane::rebuild() {
    LOGDATAO() << "Saving config" << Qt::endl;
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }
    ZDLProfile &profile = config->activeProfile();

    // Index 0 of both combos is "(Default)", which is the same as unset.
    profile.monsters = monstersList->currentIndex() > 0 ? monstersList->currentIndex() : 0;
    profile.skill = diffList->currentIndex() > 0 ? diffList->currentIndex() : 0;
    profile.warp = warpCombo->currentText();

    // The combo and the list are populated in config order, so the widget index
    // is the index into the model.
    int const portRow = sourceList->currentIndex();
    profile.port = (portRow >= 0 && portRow < config->ports.size())
                   ? config->ports[portRow].name : QString();

    int const iwadRow = IWADList->currentRow();
    profile.iwad = (iwadRow >= 0 && iwadRow < config->iwads.size())
                   ? config->iwads[iwadRow].name : QString();

    config->rememberProfileForIwad();
}

void ZDLSettingsPane::newConfig() {
    LOGDATAO() << "Loading new config" << Qt::endl;
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }
    ZDLProfile &profile = config->activeProfile();

    // Repopulating the list must not look like the user picking a game.
    QSignalBlocker const iwadBlocker(IWADList);

    if (profile.monsters < 0 || profile.monsters > 4) {
        profile.monsters = 0;
    }
    monstersList->setCurrentIndex(profile.monsters);

    if (profile.skill < 0 || profile.skill > 5) {
        profile.skill = 0;
    }
    diffList->setCurrentIndex(profile.skill);

    if (profile.warp.isEmpty()) {
        warpCombo->clearEditText();
    } else {
        warpCombo->setEditText(profile.warp);
    }

    sourceList->clear();
    for (const ZDLNameEntry &entry: std::as_const(config->ports)) {
        sourceList->addItem(entry.name);
    }

    IWADList->clear();
    for (const ZDLNameEntry &entry: std::as_const(config->iwads)) {
        auto *item = new QListWidgetItem(entry.name, IWADList, 1001);
        item->setData(32, entry.file);
        IWADList->addItem(item);
    }

    // A profile can name a port or IWAD that has since been removed from the
    // lists; drop the reference rather than leaving a selection that isn't there.
    int portRow = -1;
    for (int i = 0; i < config->ports.size(); i++) {
        if (config->ports[i].name == profile.port) {
            portRow = i;
            break;
        }
    }
    if (portRow >= 0) {
        sourceList->setCurrentIndex(portRow);
    } else {
        profile.port.clear();
    }

    int iwadRow = -1;
    for (int i = 0; i < config->iwads.size(); i++) {
        if (config->iwads[i].name == profile.iwad) {
            iwadRow = i;
            break;
        }
    }
    if (iwadRow >= 0) {
        IWADList->setCurrentRow(iwadRow);
    } else {
        profile.iwad.clear();
        IWADList->setCurrentRow(-1);
    }
}
