import QtQuick
import Zdl

// Messages pile up from the bottom corner, newest closest to the edge.
Item {
    id: stack

    property int limit: 4

    function post(severity, title, text, duration) {
        if (messages.count >= limit) {
            messages.remove(0)
        }

        messages.append({ severity: severity, title: title, body: text, duration: duration })
    }

    ListModel {
        id: messages
    }

    Column {
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 10

        Repeater {
            model: messages

            delegate: Buzz {
                required property var model
                required property int index

                severity: model.severity
                title: model.title
                body: model.body
                duration: model.duration

                onDismissed: messages.remove(index)
            }
        }
    }
}
