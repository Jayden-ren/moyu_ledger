import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 账户页：净资产汇总 + 账户卡片（编辑/归档）+ 新增账户
Item {
    id: page
    signal addRequested()
    signal editRequested(var detail)
    signal archiveRequested(int id, bool archived)
    signal dailyRecordRequested(int accountId)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.cardGap

        // 头部
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceMd

            Text {
                text: qsTr("账户")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontH2
                font.weight: Theme.weightSemibold
                color: Theme.textPrimary
            }

            Rectangle { width: 1; height: 18; color: Theme.borderBase }

            Text {
                text: qsTr("净资产 ") + Ledger.netWorthText
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody2
                font.weight: Theme.weightMedium
                font.features: ({ "tnum": 1 })
                color: Theme.textSecondary
            }

            Item { Layout.fillWidth: true }

            FlatButton {
                text: qsTr("＋ 新增账户")
                kind: "primary"
                onClicked: page.addRequested()
            }
        }

        // 列头（与行内固定列宽一一对应，保证不同账户数值纵向对齐）
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.colHeaderH
            spacing: Theme.spaceMd
            anchors.leftMargin: Theme.cardPadding
            anchors.rightMargin: Theme.cardPadding

            Item {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.cardPadding
                Text {
                    text: qsTr("账户")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }
            }

            Text {
                Layout.preferredWidth: 120
                text: qsTr("类型")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMicro3
                color: Theme.textTertiary
            }

            Text {
                Layout.preferredWidth: 160
                horizontalAlignment: Text.AlignRight
                text: qsTr("余额")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMicro3
                color: Theme.textTertiary
            }

            Text {
                Layout.preferredWidth: 160
                horizontalAlignment: Text.AlignRight
                text: qsTr("操作")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMicro3
                color: Theme.textTertiary
            }
        }

        // 账户卡片
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.spaceSm
            model: Ledger.accountModel
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                required property string name
                required property string typeText
                required property string balanceText
                required property bool isNegative
                required property string badge
                required property string accentColor
                required property int accountId

                width: list.width
                height: 64
                radius: Theme.radiusCard
                color: Theme.bgSurface
                border.width: 1
                border.color: Theme.borderBase

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.cardPadding
                    anchors.rightMargin: Theme.cardPadding
                    spacing: Theme.spaceMd

                    // 账户：徽标 + 名称（占满剩余宽度）
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceSm

                        Rectangle {
                            width: 36
                            height: 36
                            radius: 10
                            color: Theme.bgTint
                            Text {
                                anchors.centerIn: parent
                                text: badge
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontBody
                                font.weight: Theme.weightMedium
                                color: accentColor.length > 0 ? accentColor : Theme.textSecondary
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: name
                            elide: Text.ElideRight
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontBody2
                            font.weight: Theme.weightMedium
                            color: Theme.textPrimary
                        }
                    }

                    // 类型：固定列宽
                    Text {
                        Layout.preferredWidth: 120
                        text: typeText
                        elide: Text.ElideRight
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }

                    // 余额：固定列宽右对齐
                    Text {
                        Layout.preferredWidth: 160
                        horizontalAlignment: Text.AlignRight
                        text: balanceText
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontTitle
                        font.weight: Theme.weightSemibold
                        font.features: ({ "tnum": 1 })
                        color: isNegative ? Theme.negative : Theme.textPrimary
                    }

                    // 操作：固定列宽右对齐
                    Row {
                        Layout.preferredWidth: 220
                        Layout.alignment: Qt.AlignRight
                        spacing: Theme.spaceS

                        FlatButton {
                            text: qsTr("收益")
                            kind: "ghost"
                            visible: Ledger.isInvestmentAccount(accountId)
                            onClicked: page.dailyRecordRequested(accountId)
                        }
                        FlatButton {
                            text: qsTr("编辑")
                            kind: "ghost"
                            onClicked: page.editRequested(Ledger.accountDetail(accountId))
                        }
                        FlatButton {
                            text: qsTr("归档")
                            kind: "ghost"
                            onClicked: page.archiveRequested(accountId, true)
                        }
                    }
                }
            }

            // 空状态
            Column {
                anchors.centerIn: parent
                spacing: Theme.spaceSm
                visible: Ledger.accountModel.count === 0
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("还没有账户")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontTitle
                    font.weight: Theme.weightSemibold
                    color: Theme.textPrimary
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("先建一个储蓄卡或支付宝账户，再开始记账")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    color: Theme.textSecondary
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("提示：归档的账户不会出现在列表与净资产的统计口径中，历史流水保持可查。")
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMicro3
            color: Theme.textTertiary
        }
    }
}