import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Zdl
import Zdl.Components

/*
The two collections every profile picks from, and the handful of settings that
are the application's rather than any one profile's.
*/
Item {
    id: page

    property var pick: null
    property var confirm: null
    property var entry: null

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        // The IWADs on this machine, and the ports that can run them.
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14

            Collection {
                Layout.fillWidth: true
                Layout.fillHeight: true

                title: "IWADs"
                blurb: "The games themselves: doom2.wad, tnt.wad, freedoom2.wad and the rest."
                list: App.config.iwads
                filters: App.wadFilters
                remember: "wad"
                pick: page.pick
                confirm: page.confirm
                entry: page.entry
            }

            Collection {
                Layout.fillWidth: true
                Layout.fillHeight: true

                title: "Source ports"
                blurb: "The engines: gzdoom, zandronum, prboom-plus, whatever is installed."
                list: App.config.ports
                filters: App.portFilters
                remember: "src"
                pick: page.pick
                confirm: page.confirm
                entry: page.entry
            }
        }

        Surface {
            Layout.fillWidth: true
            Layout.preferredHeight: options.implicitHeight + 32

            Column {
                id: options

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 16

                Text {
                    text: "Behaviour"
                    color: Theme.text
                    font.pixelSize: Theme.fontMedium
                    font.weight: Theme.headingWeight
                    textFormat: Text.PlainText
                }

                Field {
                    width: parent.width
                    label: "Always add these arguments"
                    placeholder: "Added to every launch, whatever the profile"
                    text: App.config.alwaysAdd
                    mono: true
                    onTextChanged: App.config.alwaysAdd = text
                }

                Grid {
                    width: parent.width
                    columns: Math.max(1, Math.floor(width / 300))
                    columnSpacing: 16
                    rowSpacing: 14

                    readonly property real cell: (width - (columns - 1) * 16) / columns

                    Toggle {
                        width: parent.cell
                        text: "Close on launch"
                        checked: App.config.autoClose
                        hint: "Quit ZDL as soon as the source port has started"
                        onToggled: function (value) { App.config.autoClose = value }
                    }

                    Toggle {
                        width: parent.cell
                        text: "Show file paths"
                        checked: App.config.showPaths
                        hint: "Show the directory a file came from underneath its name"
                        onToggled: function (value) { App.config.showPaths = value }
                    }

                    Toggle {
                        width: parent.cell
                        text: "Remember the file list"
                        checked: App.config.rememberFileList
                        hint: "Keep each profile's external files between runs"
                        onToggled: function (value) { App.config.rememberFileList = value }
                    }

                    Toggle {
                        width: parent.cell
                        text: "Launch .zdl files at once"
                        checked: App.config.launchZdlImmediately
                        hint: "A .zdl given on the command line launches without showing this window"
                        onToggled: function (value) { App.config.launchZdlImmediately = value }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Theme.border
                }

                Row {
                    width: parent.width
                    spacing: 10

                    Fact {
                        label: "Configuration file"
                        value: App.config.path
                        path: true
                        clickable: true
                        hint: "Open the directory it is in"
                        maximumWidth: Math.max(200, parent.width - 220)
                        onActivated: App.reveal(App.directoryOf(App.config.path))
                    }
                }
            }
        }
    }
}
