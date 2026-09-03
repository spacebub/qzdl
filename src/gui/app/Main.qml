import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Zdl
import Zdl.Components
import Zdl.Views

/*
One window and two pages, and anything that would have been a second window
happens over this one instead. ZDL is a thing you pass through on the way to
a game, so it is one tile under a window manager that tiles and one window
everywhere else.
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

    property string page: "launch"

    readonly property var pages: [
        { key: "launch",   label: "Launch",   badge: false },
        { key: "settings", label: "Settings", badge: App.config.ports.count === 0 }
    ]

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
            trailing: App.prettyPath(App.config.path)
            compact: window.width < 800
            onSelected: function (key) { window.page = key }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 20
            currentIndex: window.page === "launch" ? 0 : 1

            LaunchView {
                pick: pickSheet
                confirm: confirmSheet
                prompt: promptSheet
                command: commandSheet
                about: aboutSheet
                onLaunched: window.afterLaunch()
            }

            SettingsView {
                pick: pickSheet
                confirm: confirmSheet
                entry: entrySheet
            }
        }
    }

    /*
    Everything that covers the window, stacked in the order one can open
    another: a confirmation asked from a sheet has to land on top of it.
    */

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

    // Return launches, which is what the window is for; Escape closes it.
    Shortcut {
        sequences: [ "Return", "Enter" ]
        enabled: window.page === "launch" && !pickSheet.visible && !confirmSheet.visible
                 && !promptSheet.visible && !entrySheet.visible
        onActivated: App.config.launch()
    }

    Shortcut {
        sequence: "Escape"
        enabled: !pickSheet.visible && !confirmSheet.visible && !promptSheet.visible
                 && !entrySheet.visible && !commandSheet.visible && !aboutSheet.visible
        onActivated: window.close()
    }

    Shortcut {
        sequences: [ StandardKey.HelpContents ]
        onActivated: aboutSheet.show()
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
