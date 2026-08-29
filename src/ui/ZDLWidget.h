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
#pragma once

#include <QWidget>

class ZDLWidget : public QWidget {
Q_OBJECT

public:
    explicit ZDLWidget(ZDLWidget *parent);

    explicit ZDLWidget(QWidget *parent);

    ZDLWidget();

    void setZParent(ZDLWidget *parent);

    virtual void rebuild();

    virtual void newConfig();

signals:

    // moc emits the definitions for these with its own parameter names.
    // NOLINTBEGIN(readability-inconsistent-declaration-parameter-name)
    void buildChildren(ZDLWidget *origin);

    void buildParent(ZDLWidget *origin);

    void readChildren(ZDLWidget *origin);

    void readParent(ZDLWidget *origin);
    // NOLINTEND(readability-inconsistent-declaration-parameter-name)

public slots:

    virtual void notifyFromChild(ZDLWidget * /*origin*/);

    virtual void notifyFromParent(ZDLWidget * /*origin*/);

    virtual void readFromChild(ZDLWidget * /*origin*/);

    virtual void readFromParent(ZDLWidget * /*origin*/);
//protected:
//	virtual void fromUpstream(ZDLWidget *origin);
//	virtual void fromDownstream(ZDLWidget *origin);

private:
    ZDLWidget *zparent{};
};
