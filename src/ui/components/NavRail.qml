import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 左导航 248px：一级导航 + 账户分组（Ledger.accounts）+ 本月分类 Top4 + 预算卡
Rectangle {
    id: rail
    width: Theme.navW
    color: Theme.bgNav

    property int currentIndex: 0
    readonly property var items: ["工作台", "概览", "流水", "报表", "预算", "账户", "设置"]
    readonly property var itemIcons: ["workbench", "overview", "ledger", "report", "budget", "accounts", "settings"]

    Rectangle {
        width: 1
        height: parent.height
        anchors.right: parent.right
        color: Theme.borderBase
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spaceSm
        spacing: 2

        Repeater {
            model: rail.items

            delegate: Rectangle {
                id: navItem
                required property int index
                required property string modelData

                Layout.fillWidth: true
                Layout.preferredHeight: Theme.navItemH
                radius: Theme.radiusBtn
                color: rail.currentIndex === index
                       ? Theme.accentSoft
                       : (itemArea.containsMouse ? Theme.bgHover : "transparent")

                NavIcon {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.spaceMd
                    anchors.verticalCenter: parent.verticalCenter
                    name: rail.itemIcons[navItem.index]
                    color: rail.currentIndex === navItem.index ? Theme.accent : Theme.textSecondary
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.spaceMd + 24
                    anchors.verticalCenter: parent.verticalCenter
                    text: navItem.modelData
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    font.weight: rail.currentIndex === index ? Theme.weightSemibold : Theme.weightRegular
                    color: rail.currentIndex === index ? Theme.accent : Theme.textPrimary
                }

                MouseArea {
                    id: itemArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: rail.currentIndex = navItem.index
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spaceXs
            Layout.bottomMargin: Theme.spaceXs
            Layout.leftMargin: Theme.spaceSm
            Layout.rightMargin: Theme.spaceSm
            height: 1
            color: Theme.borderDivider
        }

        // ---- 账户分组 ----
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spaceMd
            Layout.rightMargin: Theme.spaceMd
            Layout.preferredHeight: 22
            Text {
                text: qsTr("账户")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMicro3
                color: Theme.textTertiary
            }
            Item { Layout.fillWidth: true }
            Text {
                text: Ledger.accountModel.count
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMicro3
                color: Theme.textTertiary
            }
        }

        Repeater {
            model: Ledger.accountModel

            delegate: Rectangle {
                id: accountRow
                required property string name
                required property string balanceText
                required property bool isNegative
                required property string badge
                required property string accentColor

                Layout.fillWidth: true
                Layout.preferredHeight: Theme.accountRowH
                color: accountArea.containsMouse ? Theme.bgHover : "transparent"
                radius: Theme.radiusBtn

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spaceS
                    anchors.rightMargin: Theme.spaceMd
                    spacing: Theme.spaceS

                    Rectangle {
                        width: 22
                        height: 22
                        radius: 5
                        color: Theme.bgTint
                        Text {
                            anchors.centerIn: parent
                            text: accountRow.badge
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: accountRow.accentColor.length > 0 ? accountRow.accentColor : Theme.textSecondary
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: accountRow.name
                        elide: Text.ElideRight
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall2
                        color: Theme.textPrimary
                    }
                    Text {
                        text: accountRow.balanceText
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                        font.features: ({ "tnum": 1 })
                        color: accountRow.isNegative ? Theme.negative : Theme.textSecondary
                    }
                }

                MouseArea {
                    id: accountArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                }
            }
        }

        Text {
            visible: Ledger.accountModel.count === 0
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spaceMd
            Layout.preferredHeight: 28
            verticalAlignment: Text.AlignVCenter
            text: qsTr("还没有账户")
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            color: Theme.textTertiary
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spaceXs
            Layout.bottomMargin: Theme.spaceXs
            Layout.leftMargin: Theme.spaceSm
            Layout.rightMargin: Theme.spaceSm
            height: 1
            color: Theme.borderDivider
        }

        // ---- 本月支出分类 Top4 ----
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spaceMd
            Layout.rightMargin: Theme.spaceMd
            Layout.preferredHeight: 22
            Text {
                text: qsTr("本月支出分类")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMicro3
                color: Theme.textTertiary
            }
            Item { Layout.fillWidth: true }
            Text {
                text: Ledger.categoryShares.length + qsTr(" 类")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMicro3
                color: Theme.textTertiary
            }
        }

        Repeater {
            model: Ledger.categoryShares.length > 4 ? 4 : Ledger.categoryShares.length

            delegate: RowLayout {
                id: categoryRow
                required property int index

                Layout.fillWidth: true
                Layout.leftMargin: Theme.spaceMd
                Layout.rightMargin: Theme.spaceMd
                Layout.preferredHeight: Theme.categoryRowH
                spacing: Theme.spaceS

                Rectangle {
                    width: 6
                    height: 6
                    radius: 3
                    color: Ledger.categoryShares[categoryRow.index].color
                }
                Text {
                    Layout.fillWidth: true
                    text: Ledger.categoryShares[categoryRow.index].name
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                    color: Theme.textSecondary
                }
                Text {
                    text: Ledger.categoryShares[categoryRow.index].totalText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                    font.features: ({ "tnum": 1 })
                    color: Theme.textPrimary
                }
            }
        }

        Item { Layout.fillHeight: true }

        // ---- 预算卡（未设置态）----
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 74
            radius: Theme.radiusCard
            color: Theme.warnBg
            border.width: 1
            border.color: Theme.warnBorder

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spaceMd
                spacing: 3

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("本月预算")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.warnFg
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: Ledger.hasBudget ? (qsTr("已用 ") + Ledger.budgetUsedText) : qsTr("未设置")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.warnFg
                    }
                }

                Text {
                    visible: Ledger.hasBudget
                    text: Ledger.expenseText + " / " + Ledger.budgetLimitText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    font.weight: Theme.weightMedium
                    font.features: ({ "tnum": 1 })
                    color: Theme.warnFg
                }

                Rectangle {
                    visible: Ledger.hasBudget
                    Layout.fillWidth: true
                    Layout.preferredHeight: 5
                    radius: 2.5
                    color: Theme.warnBorder

                    Rectangle {
                        width: parent.width * Math.max(0, Math.min(1, Ledger.budgetRatio))
                        height: parent.height
                        radius: 2.5
                        color: Theme.warnSolid
                    }
                }

                Text {
                    visible: Ledger.hasBudget
                    text: qsTr("剩余 ") + Ledger.budgetRemainText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.warnFg
                    opacity: 0.85
                }

                Text {
                    visible: !Ledger.hasBudget
                    text: qsTr("设置月度预算后可查看执行进度")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.warnFg
                    opacity: 0.75
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
        }
    }
}