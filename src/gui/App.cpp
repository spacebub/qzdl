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

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QRect>
#include <QUrl>

#include "core/Paths.h"
#include "core/Session.h"
#include "gui/App.h"
#include "gui/IwadArt.h"
#include "gui/PathText.h"

namespace {

/** The remembered directory a kind of file dialog starts in. */
std::string &directoryFor(const QString &kind) {
    LastDirs &dirs = Session::get().config().general.lastDirs;

    if (kind == QLatin1String("wad")) {
        return dirs.wad;
    }

    if (kind == QLatin1String("src")) {
        return dirs.src;
    }

    if (kind == QLatin1String("save")) {
        return dirs.save;
    }

    if (kind == QLatin1String("zdl")) {
        return dirs.zdl;
    }

    if (kind == QLatin1String("config")) {
        return dirs.config;
    }

    return dirs.general;
}

}

App::App(QObject *parent) : QObject(parent) {
    _notifier = new Notifier(this);
    _runs = new Runs(this);
    _config = new ConfigBridge(_notifier, _runs, this);
}

QString App::version() {
    return QStringLiteral(QZDL_VERSION);
}

QString App::qtVersion() {
    return QStringLiteral(QT_VERSION_STR);
}

Notifier *App::notify() const { return _notifier; }
Runs *App::runs() const { return _runs; }
ConfigBridge *App::config() const { return _config; }

/*
The file pickers match on the extension alone, so each of these is the list of
suffixes rather than a description of them. Doom data comes in more shapes
than anyone remembers, which is why the list is this long.
*/
QStringList App::wadFilters() {
    return {"*.wad", "*.pwad", "*.iwad", "*.pk3", "*.pk7", "*.pkz", "*.pke", "*.ipk3", "*.ipk7",
            "*.zip", "*.7z", "*.deh", "*.bex", "*.lmp", "*.cfg"};
}

QStringList App::portFilters() {
#ifdef _WIN32
    return {"*.exe"};
#else
    return {"*"};
#endif
}

QStringList App::zdlFilters() { return {"*.zdl"}; }

// Only what Session::load can actually read. A source port's own .cfg is a
// config too, and offering it here only ever ended in a parse error.
QStringList App::configFilters() { return {"*.json", "*.ini"}; }

QStringList App::saveFilters() { return {"*.zds", "*.dsg", "*.esg", "*.sav", "*.save"}; }

void App::copyToClipboard(const QString &text) {
    QGuiApplication::clipboard()->setText(text);
}

bool App::isWindows() {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

QStringList App::drives() {
    QStringList result;

#ifdef _WIN32
    const QFileInfoList infos = QDir::drives();
    result.reserve(infos.size());

    for (const QFileInfo &info : infos) {
        result.push_back(QDir::fromNativeSeparators(info.absoluteFilePath()));
    }
#endif

    return result;
}

bool App::isDirectory(const QString &path) {
    std::error_code code;

    return std::filesystem::is_directory(PathText::toPath(path), code);
}

bool App::isFile(const QString &path) {
    std::error_code code;

    return std::filesystem::is_regular_file(PathText::toPath(path), code);
}

QString App::artFor(const QString &file) {
    return IwadArt::urlFor(file);
}

bool App::reveal(const QString &path) {
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QString App::prettyPath(const QString &path) {
    const QString home = PathText::fromPath(Paths::homeDirectory());

    return !home.isEmpty() && path.startsWith(home + "/") ? "~" + path.mid(home.length()) : path;
}

QString App::directoryOf(const QString &path) {
    return PathText::fromPath(PathText::toPath(path).parent_path());
}

QString App::startDirectory(const QString &kind) {
    const std::string &remembered = directoryFor(kind);
    std::error_code code;

    if (!remembered.empty() && std::filesystem::is_directory(remembered, code)) {
        return PathText::fromPath(remembered);
    }

    // Nowhere remembered yet, so wherever the user's own files are.
    const std::filesystem::path home = Paths::homeDirectory();
    const std::filesystem::path start = home.empty() ? std::filesystem::current_path(code) : home;

    return PathText::fromPath(start);
}

void App::rememberDirectory(const QString &kind, const QString &path) {
    if (path.isEmpty()) {
        return;
    }

    directoryFor(kind) = path.toStdString();

    // Where a dialog last landed is part of the config, and nothing else is
    // going to write it: picking a file need not change anything else.
    _config->scheduleSave();
}

QRect App::rememberedGeometry() {
    const WindowGeometry &window = Session::get().config().general.window;

    return {
        window.hasPosition ? window.x : -1,
        window.hasPosition ? window.y : -1,
        window.hasSize ? window.width : -1,
        window.hasSize ? window.height : -1,
    };
}

void App::rememberGeometry(const int x, const int y, const int width, const int height) {
    WindowGeometry &window = Session::get().config().general.window;

    window.hasPosition = true;
    window.x = x;
    window.y = y;
    window.hasSize = true;
    window.width = width;
    window.height = height;
}

void App::shutdown() {
    std::string error;

    if (!Session::get().save(&error)) {
        qWarning("Could not save the config: %s", error.c_str());
    }
}
