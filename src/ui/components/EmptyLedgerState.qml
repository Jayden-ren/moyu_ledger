import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 流水区空状态：新库的第一屏只给「记第一笔」与「导入账单」两个出口
Item {
    id: empty
    signal addRequested()
    signal importRequested()
    signal accountRequested()

    // 首次使用时（还没有任何账户）先引导建账户
    property bool needsAccount: false

    Column {
        anchors.centerIn: parent
        spacing: Theme.spaceLg

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 48
            height: 48
            radius: 24
            color: Theme.bgSelection

            Text {
                anchors.centerIn: parent
                text: "¥"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontH2
                font.weight: Theme.weightSemibold
                color: Theme.accent
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: empty.needsAccount ? qsTr("还没有账户") : qsTr("还没有账目")
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontTitle
            font.weight: Theme.weightSemibold
            color: Theme.textPrimary
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: empty.needsAccount
                      ? qsTr("先创建一个储蓄卡或支付宝账户，再开始记账")
                      : qsTr("记下第一笔，或导入支付宝 / 微信账单开始对账")
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall2
            color: Theme.textSecondary
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            topPadding: Theme.spaceS
            spacing: Theme.spaceMd

            FlatButton {
                text: empty.needsAccount ? qsTr("＋ 创建账户") : qsTr("记第一笔")
                kind: "primary"
                onClicked: {
                    if (empty.needsAccount)
                        empty.accountRequested()
                    else
                        empty.addRequested()
                }
            }
            FlatButton {
                text: qsTr("导入账单")
                kind: "ghost"
                onClicked: empty.importRequested()
            }
        }
    }
}