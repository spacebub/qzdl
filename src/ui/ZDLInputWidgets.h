/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023  spacebub
 * Copyright (C) 2026  spacebub
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

#include <QComboBox>
#include <QValidator>

/** A combo box that says when its popup opens and closes. */
class VerboseComboBox : public QComboBox {
Q_OBJECT

public:
    explicit VerboseComboBox(QWidget *parent = nullptr) : QComboBox(parent) {}

    void showPopup() override;

    void hidePopup() override;

signals:

    void onPopup();

    void onHidePopup();
};

/** Rejects everything, used to make an editable combo box read only. */
class EvilValidator : public QValidator {
Q_OBJECT

public:
    explicit EvilValidator(QObject *parent) : QValidator(parent) {}

    QValidator::State validate(QString &input, int &pos) const override;
};
