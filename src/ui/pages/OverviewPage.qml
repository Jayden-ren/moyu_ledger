import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 概览页：KPI 四卡 + 预算环 + 6 月趋势 + 分类结构 + 洞察提醒（全部 QML 自绘）
Item {
    id: page
    property string expenseChartType: "stack"
    signal openBudgetRequested()

    readonly property var kpiItems: [
        { label: qsTr("本月支出"), value: Ledger.expenseText, sub: Ledger.expenseDeltaText },
        { label: qsTr("本月收入"), value: Ledger.incomeText, sub: Ledger.incomeDeltaText },
        { label: qsTr("本月结余"), value: Ledger.balanceText, sub: Ledger.balanceRatioText },
        { label: qsTr("净资产"), value: Ledger.netWorthText, sub: qsTr("全部账户余额之和") }
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.cardGap

        BudgetAlert {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 34 : 0
            level: Ledger.budgetAlertLevel
            text: Ledger.budgetAlertText
            onOpenRequested: page.openBudgetRequested()
        }

        // ---- KPI 四卡 ----
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 92
            spacing: Theme.cardGap

            Repeater {
                model: page.kpiItems

                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radiusCard
                    color: Theme.bgSurface
                    border.width: 1
                    border.color: Theme.borderBase

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.cardPadding
                        spacing: 2

                        Text {
                            text: modelData.label
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }
                        Text {
                            text: modelData.value
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontH2
                            font.weight: Theme.weightSemibold
                            font.features: ({ "tnum": 1 })
                            color: Theme.textPrimary
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.sub
                            elide: Text.ElideRight
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }
                    }
                }
            }
        }

        // ---- 预算执行 + 6 月趋势 ----
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 268
            spacing: Theme.cardGap

            Rectangle {
                Layout.preferredWidth: 300
                Layout.fillHeight: true
                radius: Theme.radiusCard
                color: Theme.bgSurface
                border.width: 1
                border.color: Theme.borderBase

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.cardPadding
                    spacing: Theme.spaceS

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("本月预算执行")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontBody
                            font.weight: Theme.weightSemibold
                            color: Theme.textPrimary
                        }
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            visible: Ledger.budgetNearLimit
                            width: warnLabel.implicitWidth + Theme.spaceLg
                            height: 20
                            radius: Theme.radiusChip
                            color: Theme.warnBg
                            border.width: 1
                            border.color: Theme.warnBorder
                            Text {
                                id: warnLabel
                                anchors.centerIn: parent
                                text: qsTr("接近超支")
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontMicro3
                                color: Theme.warnFg
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    BudgetRing {
                        Layout.alignment: Qt.AlignHCenter
                        ratio: Ledger.budgetRatio
                        near: Ledger.budgetNearLimit
                        valueText: Ledger.hasBudget ? Ledger.budgetUsedText : qsTr("未设置")
                        caption: Ledger.hasBudget ? qsTr("已用") : qsTr("点此设置")
                    }

                    Item { Layout.fillHeight: true }

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("预算 ")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: Ledger.budgetLimitText
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            font.features: ({ "tnum": 1 })
                            color: Theme.textPrimary
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("已支出 ")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: Ledger.expenseText
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            font.features: ({ "tnum": 1 })
                            color: Theme.textPrimary
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("剩余 ")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: Ledger.budgetRemainText
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            font.features: ({ "tnum": 1 })
                            color: Ledger.budgetNearLimit ? Theme.warnFg : Theme.textPrimary
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusCard
                color: Theme.bgSurface
                border.width: 1
                border.color: Theme.borderBase

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.cardPadding
                    spacing: Theme.spaceSm

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("近 6 个月支出趋势")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontBody
                            font.weight: Theme.weightSemibold
                            color: Theme.textPrimary
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: qsTr("单位：元")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }
                    }

                    AreaTrendChart {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        points: Ledger.trend
                    }
                }
            }
        }

        // ---- 分类结构 + 洞察 ----
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 168
            spacing: Theme.cardGap

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusCard
                color: Theme.bgSurface
                border.width: 1
                border.color: Theme.borderBase

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.cardPadding
                    spacing: Theme.spaceSm

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceXs

                        Text {
                            text: qsTr("本月支出结构")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontBody
                            font.weight: Theme.weightSemibold
                            color: Theme.textPrimary
                        }

                        Item { Layout.fillWidth: true }

                        // 图表切换
                        Repeater {
                            model: [
                                { key: "stack", label: qsTr("堆叠") },
                                { key: "bar", label: qsTr("条形") },
                                { key: "donut", label: qsTr("环形") }
                            ]

                            delegate: Rectangle {
                                required property var modelData
                                width: chartTypeLabel.implicitWidth + Theme.spaceMd * 2
                                height: 22
                                radius: 11
                                color: page.expenseChartType === modelData.key ? Theme.accentSoft : "transparent"
                                border.width: 1
                                border.color: page.expenseChartType === modelData.key ? Theme.accent : Theme.borderBase

                                Text {
                                    id: chartTypeLabel
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro3
                                    color: page.expenseChartType === modelData.key ? Theme.accent : Theme.textSecondary
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: page.expenseChartType = modelData.key
                                }
                            }
                        }

                        Text {
                            text: qsTr("合计 ") + Ledger.expenseText
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }
                    }

                    // 形态一：堆叠条 + 图例
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: Theme.spaceSm
                        visible: page.expenseChartType === "stack"

                        StackedBar {
                            Layout.fillWidth: true
                            items: Ledger.categoryShares
                        }

                        Flow {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: Theme.spaceMd

                            Repeater {
                                model: Ledger.categoryShares.length > 6 ? 6 : Ledger.categoryShares.length

                                delegate: Row {
                                    required property int index
                                    spacing: Theme.spaceS

                                    Rectangle {
                                        width: 8
                                        height: 8
                                        radius: 4
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: Ledger.categoryShares[index].color
                                    }
                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: Ledger.categoryShares[index].name + " "
                                              + Ledger.categoryShares[index].shareText + "  "
                                              + Ledger.categoryShares[index].totalText
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontMicro3
                                        color: Theme.textSecondary
                                    }
                                }
                            }
                        }
                    }

                    // 形态二：横向条形排行
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: page.expenseChartType === "bar"
                        clip: true
                        model: Ledger.categoryShares
                        spacing: 2

                        delegate: Item {
                            required property int index
                            required property var modelData
                            width: ListView.view.width
                            height: 24

                            RowLayout {
                                anchors.fill: parent
                                spacing: Theme.spaceSm

                                Text { Layout.preferredWidth: 18; text: index + 1; font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                                Rectangle { width: 8; height: 8; radius: 4; color: modelData.color }
                                Text { Layout.preferredWidth: 64; text: modelData.name; font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textPrimary }
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 8
                                    radius: 4
                                    color: Theme.chartTrack
                                    Rectangle {
                                        width: parent.width * (modelData.share || 0)
                                        height: parent.height
                                        radius: 4
                                        color: modelData.color
                                    }
                                }
                                Text { Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight; text: modelData.shareText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                                Text { Layout.preferredWidth: 96; horizontalAlignment: Text.AlignRight; text: modelData.totalText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; font.features: ({ "tnum": 1 }); color: Theme.textPrimary }
                            }
                        }
                    }

                    // 形态三：环形图 + 图例
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: page.expenseChartType === "donut"
                        spacing: Theme.spaceLg

                        DonutChart {
                            Layout.preferredWidth: 132
                            Layout.preferredHeight: 132
                            Layout.alignment: Qt.AlignVCenter
                            items: Ledger.categoryShares
                            centerTitle: Ledger.expenseText
                            centerCaption: qsTr("本月支出")
                            ringWidth: 16
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: Ledger.categoryShares
                            spacing: 1

                            delegate: RowLayout {
                                required property var modelData
                                width: ListView.view.width
                                spacing: Theme.spaceS

                                Rectangle { width: 8; height: 8; radius: 4; color: modelData.color }
                                Text { Layout.fillWidth: true; text: modelData.name; elide: Text.ElideRight; font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textSecondary }
                                Text { Layout.preferredWidth: 40; horizontalAlignment: Text.AlignRight; text: modelData.shareText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                                Text { Layout.preferredWidth: 88; horizontalAlignment: Text.AlignRight; text: modelData.totalText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; font.features: ({ "tnum": 1 }); color: Theme.textPrimary }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 300
                Layout.fillHeight: true
                radius: Theme.radiusCard
                color: Theme.bgSurface
                border.width: 1
                border.color: Theme.borderBase

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.cardPadding
                    spacing: Theme.spaceS

                    Text {
                        text: qsTr("洞察提醒")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        font.weight: Theme.weightSemibold
                        color: Theme.textPrimary
                    }

                    Repeater {
                        model: [
                            {
                                title: Ledger.categoryShares.length > 0
                                       ? qsTr("最大支出：") + Ledger.categoryShares[0].name : qsTr("暂无支出"),
                                detail: Ledger.categoryShares.length > 0
                                        ? Ledger.categoryShares[0].totalText + qsTr("，占比 ")
                                          + Ledger.categoryShares[0].shareText : ""
                            },
                            {
                                title: Ledger.hasBudget
                                       ? qsTr("预算剩余 ") + Ledger.budgetRemainText : qsTr("尚未设置预算"),
                                detail: Ledger.hasBudget
                                        ? qsTr("已用 ") + Ledger.budgetUsedText + qsTr("，注意控制节奏")
                                        : qsTr("设置月度预算后可查看执行进度")
                            },
                            {
                                title: qsTr("本月 ") + Ledger.countText + qsTr(" 笔记账"),
                                detail: qsTr("日均支出 ") + Ledger.dayAverageText
                            }
                        ]

                        delegate: ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 1

                            Text {
                                Layout.fillWidth: true
                                text: modelData.title
                                elide: Text.ElideRight
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSmall2
                                font.weight: Theme.weightMedium
                                color: Theme.textPrimary
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.detail
                                elide: Text.ElideRight
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontMicro3
                                color: Theme.textTertiary
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}