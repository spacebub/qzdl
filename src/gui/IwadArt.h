/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <QQuickImageProvider>

/**
 * Serves a game's own title screen, read out of the IWAD the library points
 * at. A card asks for one by url and gets nothing back when the file has no
 * such picture in it, which is what leaves the placeholder showing.
 */
class IwadArt : public QQuickImageProvider {
public:
    static constexpr auto NAME = "iwad";

    IwadArt();

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    /** What to point an Image at, or nothing at all when there is no file. */
    [[nodiscard]] static QString urlFor(const QString &file);
};
