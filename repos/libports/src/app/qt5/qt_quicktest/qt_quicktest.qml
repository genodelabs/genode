/*
 * QML example from the Qt5 Quick Start Guide
 *
 * http://qt-project.org/doc/qt-5.0/qtquick/qtquick-quickstart-essentials.html
 */

import QtQuick 2.0

Rectangle {
    width: 200
    height: 100
    color: "red"

    Text {
        anchors.centerIn: parent
        text: "Hello, World!"
    }

    MouseArea {
        anchors.fill: parent
        onClicked: parent.color = "blue"
    }

    focus: true
    Keys.onPressed: {
        if (event.key == Qt.Key_Return) {
            color = "green";
            event.accepted = true;
        }
    }
}

