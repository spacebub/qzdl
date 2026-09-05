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
import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Zdl
import Zdl.Components
import Zdl.Views

/*
One window, and anything that would have been a second window happens over
this one instead. ZDL is a thing you pass through on the way to a game, so it
is one tile under a window manager that tiles and one window everywhere else.
*/
ApplicationWindow {
    id: window

    width: 1180
    height: 760
    minimumWidth: 720
    minimumHeight: 520
    visible: true
    title: "ZDL"
    color: Theme.background
    flags: Qt.Window | Qt.FramelessWindowHint

    property string page: "library"

    readonly property var pages: [
        { key: "library",  label: "Library",  badge: false },
        { key: "profile",  label: "Profile",  badge: false },
        { key: "engines",  label: "Engines",  badge: App.config.ports.count === 0 },
        { key: "settings", label: "Settings", badge: false }
    ]

    // Where one has been and, once stepped back, where one was. The buttons on
    // the side of a mouse walk these two.
    property var history: []
    property var ahead: []

    readonly property int pageIndex: {
        for (let each = 0; each < window.pages.length; ++each) {
            if (window.pages[each].key === window.page) {
                return each
            }
        }

        return 0
    }

    // A frameless window has to offer its own edges. A tiling manager ignores them.
    Item {
        anchors.fill: parent
        z: 100

        Repeater {
            model: [
                { edges: Qt.LeftEdge,                  cursor: Qt.SizeHorCursor, side: "left" },
                { edges: Qt.RightEdge,                 cursor: Qt.SizeHorCursor, side: "right" },
                { edges: Qt.TopEdge,                   cursor: Qt.SizeVerCursor, side: "top" },
                { edges: Qt.BottomEdge,                cursor: Qt.SizeVerCursor, side: "bottom" },
                { edges: Qt.LeftEdge | Qt.TopEdge,     cursor: Qt.SizeFDiagCursor, side: "topleft" },
                { edges: Qt.RightEdge | Qt.TopEdge,    cursor: Qt.SizeBDiagCursor, side: "topright" },
                { edges: Qt.LeftEdge | Qt.BottomEdge,  cursor: Qt.SizeBDiagCursor, side: "bottomleft" },
                { edges: Qt.RightEdge | Qt.BottomEdge, cursor: Qt.SizeFDiagCursor, side: "bottomright" }
            ]

            delegate: MouseArea {
                required property var modelData

                readonly property bool corner: modelData.side.length > 6
                readonly property bool horizontal: modelData.side === "left" || modelData.side === "right"

                width: corner ? 12 : horizontal ? 5 : parent.width
                height: corner ? 12 : horizontal ? parent.height : 5

                anchors.left: modelData.side.indexOf("left") >= 0 ? parent.left : undefined
                anchors.right: modelData.side.indexOf("right") >= 0 ? parent.right : undefined
                anchors.top: modelData.side.indexOf("top") >= 0 ? parent.top : undefined
                anchors.bottom: modelData.side.indexOf("bottom") >= 0 ? parent.bottom : undefined

                cursorShape: modelData.cursor
                onPressed: window.startSystemResize(modelData.edges)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TitleBar {
            id: titleBar

            Layout.fillWidth: true
            target: window
            pages: window.pages
            current: window.page
            compact: window.width < 860
            onSelected: function (key) { window.go(key) }
        }

        // A page is only so wide and sits in the middle of what is left over.
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            StackLayout {
                anchors.top: parent.top
                anchors.bottom: logs.visible ? logs.top : parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: 22
                anchors.bottomMargin: logs.visible ? 12 : 22
                width: Math.min(parent.width - 44, Theme.pageWidth)
                currentIndex: window.pageIndex

                LibraryView {
                    id: shelf

                    pick: pickSheet
                    confirm: confirmSheet
                    prompt: promptSheet
                    entry: entrySheet
                    onLaunched: window.afterLaunch()
                    onOpened: window.go("profile")
                    onEnginesRequested: window.go("engines")
                }

                ProfileView {
                    pick: pickSheet
                    confirm: confirmSheet
                    prompt: promptSheet
                    command: commandSheet
                    onLaunched: window.afterLaunch()
                    onEnginesRequested: window.go("engines")

                    // Landing on the shelf showing profiles would be landing
                    // one step short of what was asked for.
                    onGamesRequested: {
                        shelf.mode = "games"
                        window.go("library")
                    }

                    // The profile it was showing is gone, so this is not a step back.
                    onClosed: window.go("library")
                }

                EnginesView {
                    pick: pickSheet
                    confirm: confirmSheet
                    entry: entrySheet
                }

                SettingsView {
                    pick: pickSheet
                    confirm: confirmSheet
                    about: aboutSheet
                }
            }

            LogDock {
                id: logs

                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottomMargin: 12
                width: Math.min(parent.width - 44, Theme.pageWidth)
                confirm: confirmSheet
            }
        }
    }

    // Everything that covers the window, stacked in the order one can open
    // another: a confirmation asked from a sheet has to land on top of it.

    CommandSheet {
        id: commandSheet
        anchors.fill: parent
        anchors.topMargin: titleBar.height
        z: 200
    }

    AboutSheet {
        id: aboutSheet
        anchors.fill: parent
        anchors.topMargin: titleBar.height
        z: 210
    }

    EntrySheet {
        id: entrySheet
        anchors.fill: parent
        anchors.topMargin: titleBar.height
        pick: pickSheet
        z: 220
    }

    PromptSheet {
        id: promptSheet
        anchors.fill: parent
        anchors.topMargin: titleBar.height
        z: 230
    }

    PickSheet {
        id: pickSheet
        anchors.fill: parent
        anchors.topMargin: titleBar.height
        z: 250
    }

    ConfirmSheet {
        id: confirmSheet
        anchors.fill: parent
        anchors.topMargin: titleBar.height
        z: 300
    }

    BuzzStack {
        id: buzzes
        anchors.fill: parent
        anchors.margins: 18
        anchors.topMargin: titleBar.height + 18
        z: 400
    }

    Connections {
        target: App.notify

        function onPosted(severity, title, text, duration) {
            buzzes.post(severity, title, text, duration)
        }
    }

    // Return launches, which is what the window is for.
    Shortcut {
        sequences: [ "Return", "Enter" ]
        enabled: window.page !== "settings" && window.page !== "engines"
                 && !pickSheet.visible && !confirmSheet.visible
                 && !promptSheet.visible && !entrySheet.visible
        onActivated: App.config.launch()
    }

    // Whatever is open over the window, topmost first. Escape closes that and
    // nothing else; a page is not a thing one escapes from.
    readonly property var sheets: [ confirmSheet, pickSheet, promptSheet, entrySheet,
                                    aboutSheet, commandSheet ]

    readonly property var covered: {
        for (let each = 0; each < window.sheets.length; ++each) {
            if (window.sheets[each].visible) {
                return window.sheets[each]
            }
        }

        return null
    }

    Shortcut {
        sequence: "Escape"
        enabled: window.covered !== null
        onActivated: window.covered.dismiss()
    }

    Shortcut {
        sequences: [ StandardKey.Back ]
        onActivated: window.back()
    }

    Shortcut {
        sequences: [ StandardKey.Forward ]
        onActivated: window.forward()
    }

    // The buttons on the side of a mouse. They are taken here rather than on
    // any one page, so they mean the same thing wherever one is.
    TapHandler {
        acceptedButtons: Qt.BackButton | Qt.ForwardButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onSingleTapped: function (point, button) {
            if (button === Qt.BackButton) {
                window.back()
            } else {
                window.forward()
            }
        }
    }

    Shortcut {
        sequences: [ StandardKey.HelpContents ]
        onActivated: aboutSheet.show()
    }

    function go(key) {
        if (key === window.page) {
            return
        }

        window.history.push(window.page)

        // Long enough to walk back through a session, short enough not to grow
        // for as long as the window is open.
        if (window.history.length > 24) {
            window.history.shift()
        }

        // Going somewhere new is the end of whatever was ahead.
        window.ahead = []
        window.page = key
    }

    function back() {
        if (window.history.length === 0) {
            return
        }

        window.ahead.push(window.page)
        window.page = window.history.pop()
    }

    function forward() {
        if (window.ahead.length === 0) {
            return
        }

        window.history.push(window.page)
        window.page = window.ahead.pop()
    }

    function afterLaunch() {
        if (App.config.autoClose) {
            window.close()
        }
    }

    /*
    Where the window was left. A position that is off every screen is one saved
    against a monitor that is no longer plugged in, so it is dropped and the
    window comes up wherever the desktop would have put it.
    */
    Component.onCompleted: {
        const saved = App.rememberedGeometry()

        if (saved.width > 0 && saved.height > 0) {
            window.width = Math.max(window.minimumWidth, saved.width)
            window.height = Math.max(window.minimumHeight, saved.height)
        }

        if (saved.x >= 0 && saved.y >= 0
            && saved.x < Screen.desktopAvailableWidth && saved.y < Screen.desktopAvailableHeight) {
            window.x = saved.x
            window.y = saved.y
        }
    }

    onClosing: {
        App.rememberGeometry(window.x, window.y, window.width, window.height)
        App.shutdown()
    }
}
