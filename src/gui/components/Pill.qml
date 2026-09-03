import QtQuick
import Zdl

// A small status badge: a dot and a word.
Rectangle {
    id: pill

    property string text: ""
    property color tone: Theme.accent
    property color wash: Theme.accentSoft
    property bool dot: true

    /*
    What the word is actually set in. A hue that carries a dark shade is
    light by definition, and laid on its own near-white tint in the light
    shade there is too little between the two to read, so there it is taken
    down until there is.
    */
    readonly property color ink: Theme.dark ? pill.tone : Qt.darker(pill.tone, 1.35)

    implicitWidth: row.implicitWidth + 22
    implicitHeight: 26
    radius: height / 2
    color: wash

    /*
    The tint alone does not always separate the pill from what it sits on:
    the neutral one is the same wash an inset card is painted in, and every
    one of them is faint over white. A hairline of its own ink gives it an
    edge wherever it lands.
    */
    border.width: 1
    border.color: Qt.alpha(pill.ink, 0.3)

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6

        Rectangle {
            visible: pill.dot
            width: 8
            height: 8
            radius: 4
            color: pill.ink
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: pill.text
            color: pill.ink
            font.pixelSize: Theme.fontSmall
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
