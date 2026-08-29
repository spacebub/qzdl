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

#include "ui/dialogs/ZDLAboutDialog.h"
#include "config/ZDLConfigurationManager.h"
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QLabel>
#include "bmp_logo.xpm"

ZDLAboutDialog::ZDLAboutDialog(ZDLWidget *parent) : QDialog(parent) {
    setWindowTitle("About ZDL");
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    auto *box = new QVBoxLayout(this);
    auto *hbox = new QHBoxLayout();
    box->addLayout(hbox);
    auto *vbox = new QVBoxLayout();
    auto *title = new QLabel(QString("ZDL") + " " + ZDL_VERSION_STRING + " " + PROCESS, this);

    QFont font;
    font.setPointSize(24);
    title->setFont(font);
    title->setAlignment(Qt::AlignHCenter);

    vbox->addWidget(title);

    auto *pic = new QLabel(this);
    pic->setPixmap(QPixmap(aboutImg));

    hbox->addWidget(pic);
    hbox->addLayout(vbox);
    auto *hrTop = new QFrame(this);
    hrTop->setFrameStyle(QFrame::HLine);
    box->addWidget(hrTop);
    box->addWidget(new QLabel("Copyright (c) 2023 spacebub", this));
    box->addWidget(new QLabel("Copyright (c) 2018-2019 Lcferrum", this));
    box->addWidget(new QLabel("Copyright (c) 2004-2012 ZDL Software Foundation", this));
    auto *url = new QLabel("<a href=https://github.com/spacebub/qzdl>GitHub/qzdl</a>", this);
    url->setOpenExternalLinks(true);
    box->addWidget(url);
    box->addWidget(new QLabel(QString("Qt Version: ") + QString(QT_VERSION_STR), this));
    box->addWidget(new QLabel(QString("Version: ") + QString(ZDL_PRIVATE_VERSION_STRING), this));
    box->addWidget(new QLabel(QString("Built on ") + QString(__DATE__) + QString(" at ") + QString(__TIME__), this));
#ifndef NDEBUG
    box->addWidget(new QLabel(QString("This is a development build"), this));
#endif
    auto *hrMid = new QFrame(this);
    hrMid->setFrameStyle(QFrame::HLine);
    box->addWidget(hrMid);

    box->addWidget(new QLabel("Special thanks to BioHazard for the original version.", this));
    box->addWidget(new QLabel("Huge thanks to NeuralStunner. Without his help, none of this would be possible.", this));
    box->addWidget(new QLabel("Special thanks to Blzut3, Risen, Enjay, DRDTeam.org, ZDoom.org.", this));

    const ZDLConfiguration *conf = ZDLConfigurationManager::getConfiguration();
    if (conf != nullptr) {
        QString const userConfPath = conf->getPath(ZDLConfiguration::CONF_USER);

        auto *hrBot = new QFrame(this);
        hrBot->setFrameStyle(QFrame::HLine);
        box->addWidget(hrBot);

        auto *userConf = new QLabel("User configuration file: " + userConfPath, this);
        userConf->setCursor(Qt::IBeamCursor);
        userConf->setTextInteractionFlags(Qt::TextSelectableByMouse);

        box->addWidget(userConf);
    }

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok, Qt::Horizontal, this);
    box->addWidget(btnBox);
    connect(btnBox, SIGNAL(accepted()), this, SLOT(closeDialog()));
}

void ZDLAboutDialog::closeDialog() {
    done(0);
}
