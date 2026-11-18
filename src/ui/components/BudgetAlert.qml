import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 预算提醒条：接近上限（琥珀）/ 已超支（红），点击跳到预算页
Rectangle {
    id: alert
    property int level: 0
    property string text: ""
    signal openRequested()

    visible: level > 0
    radius: 0
    color: level >= 2 ? Theme.dangerBg : Theme.warnBg
    border.width: 0

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.pagePadding
        anchors.rightMargin: Theme.pagePadding
        spacing: Theme.spaceSm

        Text {
            text: alert.level >= 2 ? "!" : "!"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall2
            font.weight: Theme.weightBold
            color: alert.level >= 2 ? Theme.dangerFg : Theme.warnFg
        }

        Text {
            Layout.fillWidth: true
            text: alert.text
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall2
            color: alert.level >= 2 ? Theme.dangerFg : Theme.warnFg
        }

        Text {
            text: qsTr("查看预算 ›")
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall2
            color: alert.level >= 2 ? Theme.dangerFg : Theme.warnFg

            MouseArea {
                anchors.fill: parent
                anchors.margins: -4
                cursorShape: Qt.PointingHandCursor
                onClicked: alert.openRequested()
            }
        }
    }
}