import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 流水表列头 33px：分类 · 时间 · 摘要 · 账户 · 金额
Rectangle {
    id: header
    height: Theme.colHeaderH
    color: Theme.bgSubtle

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spaceLg + 2
        anchors.rightMargin: Theme.spaceLg
        spacing: 0

        Text {
            Layout.preferredWidth: Theme.colCategoryW
            text: qsTr("分类")
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMicro3
            color: Theme.textTertiary
        }
        Text {
            Layout.preferredWidth: Theme.colTimeW
            text: qsTr("时间")
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMicro3
            color: Theme.textTertiary
        }
        Text {
            Layout.fillWidth: true
            text: qsTr("摘要")
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMicro3
            color: Theme.textTertiary
        }
        Text {
            Layout.preferredWidth: Theme.colAccountW
            text: qsTr("账户")
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMicro3
            color: Theme.textTertiary
        }
        Text {
            Layout.preferredWidth: Theme.colAmountW
            horizontalAlignment: Text.AlignRight
            text: qsTr("金额")
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMicro3
            color: Theme.textTertiary
        }
    }

    Rectangle {
        height: 1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: Theme.borderBase
    }
}