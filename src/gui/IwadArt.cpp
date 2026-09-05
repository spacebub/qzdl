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

#include "core/Artwork.h"
#include "gui/IwadArt.h"

namespace {

/*
The file name rides in the url, and a path is full of the characters a url
argues about -- slashes, a drive colon, spaces, anything the user's own
language spells a folder with. Base64 has none of them, so what goes in is
exactly what comes back out.
*/
constexpr auto ENCODING = QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals;

constexpr int SQUASHED = 200;

}

IwadArt::IwadArt() : QQuickImageProvider(Image, ForceAsynchronousImageLoading) {
}

QString IwadArt::urlFor(const QString &file) {
    if (file.isEmpty()) {
        return {};
    }

    return QLatin1String("image://") + QLatin1String(NAME) + QLatin1Char('/')
           + QString::fromLatin1(file.toUtf8().toBase64(ENCODING));
}

QImage IwadArt::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    const QByteArray file = QByteArray::fromBase64(id.toLatin1(), ENCODING);
    const Artwork::Title title = Artwork::titleOf(QString::fromUtf8(file).toStdString());

    if (title.empty()) {
        return {};
    }

    QImage image;

    if (title.image) {
        // A picture that names its own colours needs no palette and no help.
        image.loadFromData(reinterpret_cast<const uchar *>(title.lump.data()),
                           static_cast<int>(title.lump.size()));
    } else if (const Artwork::Picture picture = Artwork::decode(title); !picture.empty()) {
        image = QImage(picture.pixels.data(), picture.width, picture.height,
                       static_cast<qsizetype>(picture.width) * 3,
                       QImage::Format_RGB888).copy();
    }

    if (image.isNull()) {
        return {};
    }

    /*
    A screen of this height was drawn for a 4:3 display out of pixels that
    were not square, so it is stored a fifth shorter than it was meant to be
    seen. True of a PK3 that keeps the same screen as a PNG.
    */
    if (image.height() == SQUASHED) {
        image = image.scaled(image.width(), image.height() * 6 / 5,
                             Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    if (requestedSize.isValid()) {
        image = image.scaled(requestedSize, Qt::KeepAspectRatioByExpanding,
                             Qt::SmoothTransformation);
    }

    if (size != nullptr) {
        *size = image.size();
    }

    return image;
}
