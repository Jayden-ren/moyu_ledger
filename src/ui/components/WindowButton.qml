import QtQuick
import MoyuLedger

// 自绘标题栏的三个窗口按钮（46×40 热区，关闭 hover 为红底白字）
Rectangle {
    id: btn
    property string kind: "min"  // min | max | close
    signal clicked()

    width: Theme.windowBtnW
    height: Theme.windowBtnH
    color: btnArea.containsMouse
           ? (btn.kind === "close" ? Theme.closeHover : Theme.bgHover)
           : "transparent"

    readonly property color iconColor:
        (btnArea.containsMouse && btn.kind === "close") ? Theme.textOnAccent : Theme.textSecondary

    Rectangle {
        visible: btn.kind === "min"
        anchors.centerIn: parent
        width: 10
        height: 1
        color: btn.iconColor
    }

    Rectangle {
        visible: btn.kind === "max"
        anchors.centerIn: parent
        width: 10
        height: 10
        color: "transparent"
        border.width: 1
        border.color: btn.iconColor
    }

    Item {
        visible: btn.kind === "close"
        anchors.centerIn: parent
        width: 10
        height: 10
        Rectangle {
            width: 11
            height: 1
            color: btn.iconColor
            anchors.centerIn: parent
            rotation: 45
        }
        Rectangle {
            width: 11
            height: 1
            color: btn.iconColor
            anchors.centerIn: parent
            rotation: -45
        }
    }

    MouseArea {
        id: btnArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: btn.clicked()
    }
}