import QtQuick
import Zdl
import Zdl.Components

// Anything that needs one line typed before it can happen: naming a profile,
// renaming one. In the window, like every other question.
Item {
    id: sheet

    property string title: ""
    property string label: ""
    property string acceptText: "Save"
    property var accepted: null

    visible: false

    function ask(title, label, value, acceptText, onAccepted) {
        sheet.title = title
        sheet.label = label
        sheet.acceptText = acceptText
        sheet.accepted = onAccepted
        sheet.visible = true

        input.text = value
        input.input.forceActiveFocus()
        input.input.selectAll()
    }

    function dismiss() {
        sheet.visible = false
        sheet.accepted = null
    }

    function commit() {
        const callback = sheet.accepted
        const value = input.text.trim()

        if (value === "") {
            return
        }

        sheet.dismiss()

        if (callback) {
            callback(value)
        }
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
        width: Math.min(460, parent.width - 48)
        height: content.implicitHeight + 44

        scale: sheet.visible ? 1 : 0.95
        Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

        Column {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 22
            spacing: 16

            Text {
                width: parent.width
                text: sheet.title
                color: Theme.text
                font.pixelSize: Theme.fontLarge
                font.weight: Theme.headingWeight
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
            }

            Field {
                id: input
                width: parent.width
                label: sheet.label
                onAccepted: sheet.commit()
            }

            Row {
                anchors.right: parent.right
                spacing: 8

                AppButton {
                    text: "Cancel"
                    onClicked: sheet.dismiss()
                }

                AppButton {
                    text: sheet.acceptText
                    variant: "primary"
                    enabled: input.text.trim() !== ""
                    onClicked: sheet.commit()
                }
            }
        }
    }

    Keys.onEscapePressed: sheet.dismiss()
}
