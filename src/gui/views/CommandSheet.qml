import QtQuick
import QtQuick.Controls.Basic
import Zdl
import Zdl.Components

/*
Exactly what the source port would be handed. Everything the launch page does
comes down to this one line, and being able to read it is how a profile that
launches the wrong thing gets diagnosed.
*/
Item {
    id: sheet

    visible: false

    function show() {
        sheet.visible = true
    }

    function dismiss() {
        sheet.visible = false
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.scrim

        /*
        A sheet covers the window, so nothing underneath it should react to
        anything. Without hoverEnabled the hover still goes through and the
        page below lights up under a pointer that is over a sheet, and a
        button this does not claim is a click the page below still gets.
        */
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
            onClicked: sheet.dismiss()
        }
    }

    Surface {
        anchors.centerIn: parent
        width: Math.min(760, parent.width - 48)
        height: Math.min(420, parent.height - 48)

        Column {
            id: content
            anchors.fill: parent
            anchors.margins: 22
            spacing: 14

            Row {
                width: parent.width
                spacing: 10

                Text {
                    width: parent.width - close.width - 10
                    text: "Command line"
                    color: Theme.text
                    font.pixelSize: Theme.fontLarge
                    font.weight: Theme.headingWeight
                    anchors.verticalCenter: parent.verticalCenter
                    textFormat: Text.PlainText
                }

                GlyphButton {
                    id: close
                    glyph: "cross"
                    size: 26
                    anchors.verticalCenter: parent.verticalCenter
                    onClicked: sheet.dismiss()
                }
            }

            Surface {
                width: parent.width
                height: parent.height - content.spacing * 2 - 34 - Theme.control
                inset: true

                Flickable {
                    anchors.fill: parent
                    anchors.margins: 14
                    contentHeight: line.implicitHeight
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    ScrollBar.vertical: ScrollBar {
                        id: bar
                        visible: bar.size < 1
                        width: 8

                        contentItem: Rectangle {
                            radius: 4
                            color: Theme.borderStrong
                            opacity: bar.pressed ? 0.9 : 0.45
                        }
                    }

                    TextEdit {
                        id: line

                        width: parent.width - (bar.visible ? bar.width + 6 : 0)
                        text: App.config.commandLine !== "" ? App.config.commandLine
                            : App.config.dosPort && App.config.dosbox === ""
                              && App.config.systemDosbox === ""
                            ? "Nothing to launch yet: this source port is a DOS one, and "
                            + "there is no DOSBox on this machine to run it in."
                            : "Nothing to launch yet: no source port is selected."
                        color: App.config.commandLine === "" ? Theme.faint : Theme.text
                        font.family: Theme.mono
                        font.pixelSize: Theme.fontSmall
                        selectionColor: Theme.accent
                        selectedTextColor: Theme.accentText
                        wrapMode: TextEdit.WrapAnywhere
                        readOnly: true
                        selectByMouse: true
                        textFormat: TextEdit.PlainText
                    }
                }
            }

            Row {
                anchors.right: parent.right
                spacing: 8

                AppButton {
                    text: "Copy"
                    glyph: "edit"
                    enabled: App.config.commandLine !== ""

                    onClicked: {
                        App.copyToClipboard(App.config.commandLine)
                        App.notify.success("The command line is on the clipboard.")
                    }
                }

                AppButton {
                    text: "Close"
                    variant: "primary"
                    onClicked: sheet.dismiss()
                }
            }
        }
    }

    Keys.onEscapePressed: sheet.dismiss()
}
