import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 报表页：分类排行 + 近 6 月收支对比 + 账户维度 + 导出 CSV
Item {
    id: page
    signal notifyRequested(string message)

    readonly property var kpiItems: [
        { label: qsTr("收入"), value: Ledger.reportIncomeText, sub: qsTr("环比 ") + Ledger.reportIncomeMoM },
        { label: qsTr("支出"), value: Ledger.reportExpenseText,
          sub: qsTr("环比 ") + Ledger.reportExpenseMoM + qsTr(" · 同比 ") + Ledger.reportExpenseYoY },
        { label: qsTr("结余"), value: Ledger.reportBalanceText, sub: Ledger.reportRangeLabel },
        { label: qsTr("记账笔数"), value: Ledger.reportCountText + qsTr(" 笔"), sub: qsTr("范围内合计") }
    ]

    // 把报表页抓成 PNG 存到「文档」目录
    function exportPng() {
        const path = Ledger.documentsDirectory() + "/摸鱼记账-报表-" + AppContext.monthKey + "-"
                     + Ledger.reportRange + ".png"
        page.grabToImage(function(result) {
            const ok = result.saveToFile("file:///" + path)
            page.notifyRequested(ok ? qsTr("报表已导出到 ") + path : qsTr("导出 PNG 失败"))
        })
    }

    readonly property var rangeOptions: [
        { key: "month", label: qsTr("本月") },
        { key: "3m", label: qsTr("近 3 月") },
        { key: "6m", label: qsTr("近 6 月") },
        { key: "year", label: qsTr("本年") }
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.cardGap

        // 头部
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceMd

            Text {
                text: qsTr("报表")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontH2
                font.weight: Theme.weightSemibold
                color: Theme.textPrimary
            }

            Rectangle { width: 1; height: 18; color: Theme.borderBase }

            Text {
                text: Ledger.reportRangeLabel
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody2
                color: Theme.textSecondary
            }

            Item { Layout.fillWidth: true }

            Row {
                spacing: Theme.spaceXs

                Repeater {
                    model: page.rangeOptions

                    delegate: Rectangle {
                        required property var modelData
                        width: rangeLabel.implicitWidth + Theme.spaceXl
                        height: 28
                        radius: Theme.radiusBtn
                        color: Ledger.reportRange === modelData.key ? Theme.accentSoft : "transparent"
                        border.width: 1
                        border.color: Ledger.reportRange === modelData.key ? Theme.accent : Theme.borderBase

                        Text {
                            id: rangeLabel
                            anchors.centerIn: parent
                            text: modelData.label
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            color: Ledger.reportRange === modelData.key ? Theme.accent : Theme.textSecondary
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Ledger.setReportRange(modelData.key)
                        }
                    }
                }
            }

            FlatButton {
                text: qsTr("导出 PDF")
                kind: "ghost"
                onClicked: page.notifyRequested(Ledger.exportReportPdf("").message)
            }

            FlatButton {
                text: qsTr("导出 PNG")
                kind: "ghost"
                onClicked: page.exportPng()
            }

            FlatButton {
                text: qsTr("导出 CSV")
                kind: "ghost"
                onClicked: {
                    const result = Ledger.exportMonthCsv("")
                    page.notifyRequested(result.message)
                }
            }
        }

        // KPI（用固定高度的 Item 约束，避免被 ColumnLayout 拉伸）
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 76
            Layout.minimumHeight: 76
            Layout.maximumHeight: 76

            RowLayout {
                anchors.fill: parent
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
                            text: modelData.sub || ""
                            elide: Text.ElideRight
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }
                    }
                }
            }
            }
        }

        // 分类排行 + 收支对比
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
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
                    spacing: Theme.spaceS

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("支出分类排行")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontBody
                            font.weight: Theme.weightSemibold
                            color: Theme.textPrimary
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: qsTr("合计 ") + Ledger.reportExpenseText
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }
                    }

                    ListView {
                        id: rankList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: Ledger.reportShares
                        spacing: 2

                        delegate: Item {
                            required property int index
                            required property var modelData

                            width: rankList.width
                            height: 30

                            RowLayout {
                                anchors.fill: parent
                                spacing: Theme.spaceSm

                                Text {
                                    Layout.preferredWidth: 18
                                    text: (index + 1)
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro3
                                    color: Theme.textTertiary
                                }

                                Rectangle {
                                    width: 8
                                    height: 8
                                    radius: 4
                                    color: modelData.color
                                }

                                Text {
                                    Layout.preferredWidth: 64
                                    text: modelData.name
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSmall2
                                    color: Theme.textPrimary
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 6
                                    radius: 3
                                    color: Theme.chartTrack

                                    Rectangle {
                                        width: parent.width * (modelData.share || 0)
                                        height: parent.height
                                        radius: 3
                                        color: modelData.color
                                    }
                                }

                                Text {
                                    Layout.preferredWidth: 42
                                    horizontalAlignment: Text.AlignRight
                                    text: modelData.shareText
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro3
                                    color: Theme.textTertiary
                                }

                                Text {
                                    Layout.preferredWidth: 96
                                    horizontalAlignment: Text.AlignRight
                                    text: modelData.totalText
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSmall2
                                    font.features: ({ "tnum": 1 })
                                    color: Theme.textPrimary
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 360
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
                            text: qsTr("近 6 个月收支对比")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontBody
                            font.weight: Theme.weightSemibold
                            color: Theme.textPrimary
                        }
                        Item { Layout.fillWidth: true }
                        Row {
                            spacing: Theme.spaceSm
                            Rectangle { width: 8; height: 8; radius: 2; color: Theme.accent; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: qsTr("支出"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary; anchors.verticalCenter: parent.verticalCenter }
                            Rectangle { width: 8; height: 8; radius: 2; color: Theme.income; opacity: 0.75; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: qsTr("收入"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary; anchors.verticalCenter: parent.verticalCenter }
                        }
                    }

                    GroupedBars {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        points: Ledger.reportTrend
                    }
                }
            }
        }

        // 账户维度 + 月度明细
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 196
            Layout.minimumHeight: 196
            Layout.maximumHeight: 196
            spacing: Theme.cardGap

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
            Layout.minimumHeight: 196
            Layout.maximumHeight: 196
            radius: Theme.radiusCard
            color: Theme.bgSurface
            border.width: 1
            border.color: Theme.borderBase

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.cardPadding
                spacing: Theme.spaceXs

                Text {
                    text: qsTr("账户维度")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    font.weight: Theme.weightSemibold
                    color: Theme.textPrimary
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Text { Layout.preferredWidth: 112; text: qsTr("账户"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                    Text { Layout.preferredWidth: 88; horizontalAlignment: Text.AlignRight; text: qsTr("收入"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                    Text { Layout.preferredWidth: 88; horizontalAlignment: Text.AlignRight; text: qsTr("支出"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                    Text { Layout.preferredWidth: 52; horizontalAlignment: Text.AlignRight; text: qsTr("笔数"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                    Item { Layout.fillWidth: true }
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: Ledger.reportAccountTotals
                    spacing: 1

                    delegate: RowLayout {
                        required property var modelData
                        width: ListView.view.width
                        spacing: 0

                        Text { Layout.preferredWidth: 112; text: modelData.name; elide: Text.ElideRight; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall2; color: Theme.textPrimary }
                        Text { Layout.preferredWidth: 88; horizontalAlignment: Text.AlignRight; text: modelData.incomeText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall2; font.features: ({ "tnum": 1 }); color: Theme.income }
                        Text { Layout.preferredWidth: 88; horizontalAlignment: Text.AlignRight; text: modelData.expenseText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall2; font.features: ({ "tnum": 1 }); color: Theme.textPrimary }
                        Text { Layout.preferredWidth: 52; horizontalAlignment: Text.AlignRight; text: modelData.count; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall2; color: Theme.textSecondary }
                        Item { Layout.fillWidth: true }
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
                    spacing: Theme.spaceXs

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("标签维度")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontBody
                            font.weight: Theme.weightSemibold
                            color: Theme.textPrimary
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: Ledger.reportTagShares.length + qsTr(" 个标签")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceSm
                        Text { Layout.fillWidth: true; text: qsTr("标签"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                        Text { Layout.preferredWidth: 80; horizontalAlignment: Text.AlignRight; text: qsTr("支出"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                        Text { Layout.preferredWidth: 50; horizontalAlignment: Text.AlignRight; text: qsTr("占比"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                    }

                    ListView {
                        id: tagList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: Ledger.reportTagShares
                        spacing: 1

                        delegate: RowLayout {
                            required property var modelData
                            width: tagList.width
                            spacing: Theme.spaceSm

                            Rectangle {
                                width: 18
                                height: 18
                                radius: 9
                                color: Theme.accentSoft

                                Text {
                                    anchors.centerIn: parent
                                    text: "#"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro3
                                    color: Theme.accent
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: modelData.name
                                elide: Text.ElideRight
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSmall2
                                color: Theme.textPrimary
                            }

                            Text {
                                Layout.preferredWidth: 80
                                horizontalAlignment: Text.AlignRight
                                text: modelData.totalText
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSmall2
                                font.features: ({ "tnum": 1 })
                                color: Theme.textPrimary
                            }

                            Text {
                                Layout.preferredWidth: 50
                                horizontalAlignment: Text.AlignRight
                                text: modelData.shareText
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontMicro3
                                color: Theme.textTertiary
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: tagList.count === 0
                            text: qsTr("还没有带标签的流水")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            color: Theme.textTertiary
                        }
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 320
                Layout.fillHeight: true
                radius: Theme.radiusCard
                color: Theme.bgSurface
                border.width: 1
                border.color: Theme.borderBase

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.cardPadding
                    spacing: Theme.spaceXs

                    Text {
                        text: qsTr("月度明细")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        font.weight: Theme.weightSemibold
                        color: Theme.textPrimary
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceSm
                        Text { Layout.preferredWidth: 70; text: qsTr("月份"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                        Text { Layout.preferredWidth: 80; horizontalAlignment: Text.AlignRight; text: qsTr("收入"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                        Text { Layout.preferredWidth: 80; horizontalAlignment: Text.AlignRight; text: qsTr("支出"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                        Text { Layout.preferredWidth: 40; horizontalAlignment: Text.AlignRight; text: qsTr("笔数"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                    }

                    ListView {
                        id: monthList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: Ledger.reportMonthlyRows
                        spacing: 1

                        delegate: RowLayout {
                            required property var modelData
                            width: monthList.width
                            spacing: Theme.spaceSm

                            Text { Layout.preferredWidth: 70; text: modelData.label; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall2; color: Theme.textPrimary }
                            Text { Layout.preferredWidth: 80; horizontalAlignment: Text.AlignRight; text: modelData.incomeText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall2; font.features: ({ "tnum": 1 }); color: Theme.income }
                            Text { Layout.preferredWidth: 80; horizontalAlignment: Text.AlignRight; text: modelData.expenseText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall2; font.features: ({ "tnum": 1 }); color: Theme.textPrimary }
                            Text { Layout.preferredWidth: 40; horizontalAlignment: Text.AlignRight; text: modelData.count; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall2; color: Theme.textSecondary }
                        }
                    }
                }
            }
        }
    }
}
