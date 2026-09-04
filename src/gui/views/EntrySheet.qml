import QtQuick
import Zdl
import Zdl.Components

/*
Editing one IWAD or one source port: what it is called, and the file it stands
for. Adding does not come through here -- picking the file is the whole of it,
and the name is read off the file.
*/
Item {
    id: sheet

    property var pick: null

    property string title: ""
    property var filters: [ "*" ]
    property string remember: "general"

    // Where the name comes from when the field is left empty: the list the
    // entry is going into knows how to read a file's proper name off it.
    property var list: null

    property var accepted: null

    visible: false

    function ask(title, list, filters, remember, name, file, onAccepted) {
        sheet.title = title
        sheet.list = list
        sheet.filters = filters
        sheet.remember = remember
        sheet.accepted = onAccepted
        sheet.visible = true

        nameField.text = name
        pathField.text = file

        nameField.input.forceActiveFocus()
        nameField.input.selectAll()
    }

    function dismiss() {
        sheet.visible = false
        sheet.accepted = null
        sheet.list = null
    }

    function commit() {
        const callback = sheet.accepted
        const name = nameField.text.trim()
        const file = pathField.text.trim()

        if (file === "") {
            return
        }

        sheet.dismiss()

        if (callback) {
            callback(name, file)
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
        width: Math.min(520, parent.width - 48)
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

            PathField {
                id: pathField
                width: parent.width
                label: "File"
                pick: sheet.pick
                filters: sheet.filters
                remember: sheet.remember
                browseTitle: sheet.title

                /*
                Picking a file is nearly always the whole answer: the name it
                should carry is one this already knows how to work out. It is
                only filled in when nothing has been typed, so a name that was
                chosen on purpose survives changing the file under it.
                */
                onTextChanged: {
                    if (nameField.text.trim() === "" && pathField.text !== "" && sheet.list) {
                        nameField.text = sheet.list.describe(pathField.text)
                    }
                }
            }

            Field {
                id: nameField
                width: parent.width
                label: "Name"
                placeholder: "Taken from the file"
                hint: "What profiles and .zdl files call this one."
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
                    text: "Save"
                    variant: "primary"
                    enabled: pathField.text.trim() !== ""
                    onClicked: sheet.commit()
                }
            }
        }
    }

    Keys.onEscapePressed: sheet.dismiss()
}
