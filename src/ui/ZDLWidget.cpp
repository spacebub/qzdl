/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2019  Lcferrum
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
#include "ui/ZDLWidget.h"

ZDLWidget::ZDLWidget(ZDLWidget *parent) : QWidget(parent) {
    setZParent(parent);
    setContentsMargins(0, 0, 0, 0);
}

void ZDLWidget::setZParent(ZDLWidget *parent) {
    zparent = parent;
    connect(parent, SIGNAL(buildChildren(ZDLWidget * )), this, SLOT(notifyFromParent(ZDLWidget * )));
    connect(this, SIGNAL(buildParent(ZDLWidget * )), parent, SLOT(notifyFromChild(ZDLWidget * )));
    connect(parent, SIGNAL(readChildren(ZDLWidget * )), this, SLOT(readFromParent(ZDLWidget * )));
    connect(this, SIGNAL(readParent(ZDLWidget * )), parent, SLOT(readFromChild(ZDLWidget * )));
}

ZDLWidget::ZDLWidget() {
    setContentsMargins(0, 0, 0, 0);
    zparent = nullptr;
}

ZDLWidget::ZDLWidget(QWidget *parent) : QWidget(parent) {
    setContentsMargins(0, 0, 0, 0);
    zparent = nullptr;
}

void ZDLWidget::notifyFromChild(ZDLWidget *origin) {
    if (origin != this) {
        emit buildChildren(origin);
        emit buildParent(origin);
        rebuild();
    }
}

void ZDLWidget::notifyFromParent(ZDLWidget *origin) {
    if (origin != this) {
        emit buildChildren(origin);
        rebuild();
    }
}

void ZDLWidget::readFromChild(ZDLWidget *origin) {
    if (origin != this) {
        emit readChildren(origin);
        emit readParent(origin);
        newConfig();
    }
}

void ZDLWidget::readFromParent(ZDLWidget *origin) {
    if (origin != this) {
        emit readChildren(origin);
        newConfig();
    }
}

void ZDLWidget::rebuild() {
}

void ZDLWidget::newConfig() {
}






