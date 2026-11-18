import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 投资类账户（理财 / 股票 / 投资）的每日收益录入与修复
Item {
    id: dialog
    anchors.fill: parent
    visible: false
    z: 900

    signal saved(string message)

    property int accountId: -1
    property int recordId: 0
    property string mode: "income"   // income = 填当日收益；value = 填当日市值
    property string dateText: ""
    property string errorText: ""

    readonly property var summary: dialog.accountId > 0 ? Ledger.investmentSummary(dialog.accountId) : ({})
    readonly property var records: dialog.accountId > 0 ? Ledger.dailyRecords(dialog.accountId) : []

    function today() { return Qt.formatDate(new Date(), "yyyy-MM-dd") }

    function reset() {
        recordId = 0
        mode = "income"
        dateText = today()
        incomeInput.text = ""
        valueInput.text = ""
        commissionInput.text = ""
        noteInput.text = ""
        errorText = ""
    }

    function open(account) {
        accountId = account
        reset()
        visible = true
        incomeInput.forceActiveFocus()
    }

    function close() { visible = false }

    function editRecord(record) {
        recordId = record.id
        dateText = record.dateText
        mode = "income"
        incomeInput.text = String(record.incomeCents / 100).replace(/\.?0+$/, "")
        commissionInput.text = String(record.commissionText).replace(/[^0-9.]/g, "")
        noteInput.text = record.note || ""
        errorText = ""
    }

    function deleteRecord(id) {
        const result = Ledger.removeDailyRecord(id)
        if (result.ok)
            dialog.saved(result.message)
        else
            errorText = result.message
    }

    function submit() {
        if (dialog.accountId <= 0) {
            errorText = qsTr("请选择账户")
            return
        }
        const entry = {
            "accountId": dialog.accountId,
            "recordId": dialog.recordId,
            "dateText": dialog.dateText,
            "mode": dialog.mode,
            "incomeText": incomeInput.text,
            "valueText": valueInput.text,
            "commissionText": commissionInput.text,
            "note": noteInput.text
        }
        const result = Ledger.saveDailyRecord(entry)
        if (result.ok) {
            dialog.saved(result.message)
            dialog.reset()
            dialog.close()
        } else {
            errorText = result.message
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.scrim
        MouseArea { anchors.fill: parent; onClicked: dialog.close() }
    }

    Rectangle {
        width: 900
        height: 540
        anchors.centerIn: parent
        radius: Theme.radiusPanel
        color: Theme.bgSurface
        border.width: 1
        border.color: Theme.borderBase

        MouseArea { anchors.fill: parent }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spaceXl
            spacing: Theme.spaceS

            // 头部
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spaceMd

                Text {
                    text: qsTr("每日收益")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontH1
                    font.weight: Theme.weightSemibold
                    color: Theme.textPrimary
                }

                Text {
                    text: dialog.summary.name || ""
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    color: Theme.textSecondary
                }

                Rectangle {
                    height: 20
                    width: typeLabel.implicitWidth + Theme.spaceMd
                    radius: 4
                    color: Theme.accentSoft
                    Text {
                        id: typeLabel
                        anchors.centerIn: parent
                        text: dialog.summary.typeText || ""
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.accent
                    }
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: qsTr("记录当日收益与佣金；历史记录可点「修复」修改或删除")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }

                Rectangle {
                    width: 28
                    height: 28
                    radius: 6
                    color: closeMouse.containsMouse ? Theme.bgHover : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        font.pixelSize: Theme.fontSmall2
                        color: Theme.textSecondary
                    }
                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: dialog.close()
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.borderHairline }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.spaceXl

                // 左：录入表单
                ColumnLayout {
                    Layout.preferredWidth: 330
                    Layout.alignment: Qt.AlignTop
                    spacing: Theme.spaceXs

                    Text {
                        text: qsTr("日期")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }

                    Rectangle {
                        Layout.preferredWidth: 220
                        Layout.preferredHeight: 32
                        radius: Theme.radiusBtn
                        color: dateFieldMouse.containsMouse ? Theme.bgHover : Theme.bgSurface
                        border.width: 1
                        border.color: datePicker.visible ? Theme.accent : Theme.borderStrong

                        Row {
                            anchors.centerIn: parent
                            spacing: 6
                            Text { text: "📅"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
                            Text {
                                text: dialog.dateText
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontMicro2
                                font.features: ({ "tnum": 1 })
                                color: Theme.textPrimary
                            }
                        }

                        MouseArea {
                            id: dateFieldMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: datePicker.show(dialog.dateText)
                        }
                    }

                    Text {
                        Layout.topMargin: Theme.spaceXs
                        text: qsTr("录入方式")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Repeater {
                            model: [{ key: "income", label: qsTr("填当日收益") },
                                    { key: "value", label: qsTr("填当日市值") }]

                            delegate: Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                height: 30
                                radius: 6
                                color: dialog.mode === modelData.key ? Theme.accentSoft : Theme.bgSurface
                                border.width: 1
                                border.color: dialog.mode === modelData.key ? Theme.accent : Theme.borderStrong

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro2
                                    color: dialog.mode === modelData.key ? Theme.accent : Theme.textSecondary
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        dialog.mode = modelData.key
                                        dialog.errorText = ""
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        Layout.topMargin: Theme.spaceXs
                        text: dialog.mode === "value" ? qsTr("当日市值") : qsTr("当日收益")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        radius: Theme.radiusBtn
                        color: Theme.bgSurface
                        border.width: 1
                        border.color: Theme.borderStrong
                        visible: dialog.mode === "income"

                        TextInput {
                            id: incomeInput
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spaceSm
                            verticalAlignment: TextInput.AlignVCenter
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontBody
                            font.features: ({ "tnum": 1 })
                            color: Theme.textPrimary
                            selectionColor: Theme.accentSoftStrong
                            validator: RegularExpressionValidator {
                                regularExpression: /-?[0-9]{0,9}(\.[0-9]{0,2})?/
                            }
                        }
                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spaceSm
                            verticalAlignment: Text.AlignVCenter
                            visible: incomeInput.text.length === 0
                            text: qsTr("如 25.50，亏损填 -15.00")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro2
                            color: Theme.textTertiary
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        radius: Theme.radiusBtn
                        color: Theme.bgSurface
                        border.width: 1
                        border.color: Theme.borderStrong
                        visible: dialog.mode === "value"

                        TextInput {
                            id: valueInput
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spaceSm
                            verticalAlignment: TextInput.AlignVCenter
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontBody
                            font.features: ({ "tnum": 1 })
                            color: Theme.textPrimary
                            selectionColor: Theme.accentSoftStrong
                            validator: RegularExpressionValidator {
                                regularExpression: /[0-9]{0,9}(\.[0-9]{0,2})?/
                            }
                        }
                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spaceSm
                            verticalAlignment: Text.AlignVCenter
                            visible: valueInput.text.length === 0
                            text: qsTr("收盘后的账户总市值")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro2
                            color: Theme.textTertiary
                        }
                    }

                    Text {
                        Layout.topMargin: Theme.spaceXs
                        text: qsTr("佣金 / 手续费")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        radius: Theme.radiusBtn
                        color: Theme.bgSurface
                        border.width: 1
                        border.color: Theme.borderStrong

                        TextInput {
                            id: commissionInput
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spaceSm
                            verticalAlignment: TextInput.AlignVCenter
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontBody
                            font.features: ({ "tnum": 1 })
                            color: Theme.textPrimary
                            selectionColor: Theme.accentSoftStrong
                            validator: RegularExpressionValidator {
                                regularExpression: /[0-9]{0,9}(\.[0-9]{0,2})?/
                            }
                        }
                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spaceSm
                            verticalAlignment: Text.AlignVCenter
                            visible: commissionInput.text.length === 0
                            text: qsTr("可留空；会作为支出记一笔")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro2
                            color: Theme.textTertiary
                        }
                    }

                    Text {
                        Layout.topMargin: Theme.spaceXs
                        text: qsTr("备注")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        radius: Theme.radiusBtn
                        color: Theme.bgSurface
                        border.width: 1
                        border.color: Theme.borderStrong

                        TextInput {
                            id: noteInput
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spaceSm
                            verticalAlignment: TextInput.AlignVCenter
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            color: Theme.textPrimary
                            selectionColor: Theme.accentSoftStrong
                            onAccepted: dialog.submit()
                        }
                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spaceSm
                            verticalAlignment: Text.AlignVCenter
                            visible: noteInput.text.length === 0
                            text: qsTr("可选")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            color: Theme.textTertiary
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.spaceXs
                        wrapMode: Text.WordWrap
                        text: dialog.mode === "value"
                              ? qsTr("收益 = 当日市值 − 上一日市值 + 佣金；市值会自动作为下一天的基准")
                              : qsTr("收益填正数=盈利、负数=亏损；佣金会单独记一笔支出")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: dialog.errorText.length > 0
                        wrapMode: Text.WordWrap
                        text: dialog.errorText
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall2
                        color: Theme.dangerFg
                    }
                }
                // 右：汇总 + 历史记录（可修复）
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Theme.spaceXs

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 86
                        radius: 8
                        color: Theme.bgSubtle
                        border.width: 1
                        border.color: Theme.borderBase

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spaceS
                            spacing: Theme.spaceLg

                            ColumnLayout {
                                spacing: 2
                                Text {
                                    text: qsTr("期初")
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro3
                                    color: Theme.textTertiary
                                }
                                Text {
                                    text: dialog.summary.openingText || "¥0.00"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontBody
                                    font.weight: Theme.weightSemibold
                                    font.features: ({ "tnum": 1 })
                                    color: Theme.textPrimary
                                }
                            }

                            ColumnLayout {
                                spacing: 2
                                Text {
                                    text: qsTr("累计收益")
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro3
                                    color: Theme.textTertiary
                                }
                                Text {
                                    text: dialog.summary.incomeText || "¥0.00"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontBody
                                    font.weight: Theme.weightSemibold
                                    font.features: ({ "tnum": 1 })
                                    color: Theme.income
                                }
                            }

                            ColumnLayout {
                                spacing: 2
                                Text {
                                    text: qsTr("累计佣金")
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro3
                                    color: Theme.textTertiary
                                }
                                Text {
                                    text: dialog.summary.commissionText || "¥0.00"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontBody
                                    font.weight: Theme.weightSemibold
                                    font.features: ({ "tnum": 1 })
                                    color: Theme.expense
                                }
                            }

                            ColumnLayout {
                                spacing: 2
                                Text {
                                    text: qsTr("记录天数")
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro3
                                    color: Theme.textTertiary
                                }
                                Text {
                                    text: String(dialog.summary.count || 0)
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontBody
                                    font.weight: Theme.weightSemibold
                                    font.features: ({ "tnum": 1 })
                                    color: Theme.textPrimary
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }

                    Text {
                        Layout.topMargin: Theme.spaceXs
                        text: qsTr("历史记录（点「修复」可改当日收益 / 佣金 / 市值，或删除）")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }

                    ListView {
                        id: recordList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 4
                        model: dialog.records
                        boundsBehavior: Flickable.StopAtBounds

                        delegate: Rectangle {
                            required property var modelData
                            width: recordList.width
                            height: 40
                            radius: 6
                            color: recordMouse.containsMouse ? Theme.bgHover : "transparent"
                            border.width: 1
                            border.color: Theme.borderHairline

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Theme.spaceSm
                                anchors.rightMargin: Theme.spaceSm
                                spacing: 8

                                Text {
                                    Layout.preferredWidth: 84
                                    text: modelData.dateText
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro2
                                    font.features: ({ "tnum": 1 })
                                    color: Theme.textPrimary
                                }

                                Text {
                                    Layout.preferredWidth: 88
                                    horizontalAlignment: Text.AlignRight
                                    text: modelData.incomeText
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSmall2
                                    font.features: ({ "tnum": 1 })
                                    color: modelData.incomeCents >= 0 ? Theme.income : Theme.expense
                                }

                                Text {
                                    Layout.preferredWidth: 76
                                    horizontalAlignment: Text.AlignRight
                                    text: qsTr("佣金 ") + modelData.commissionText
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro3
                                    color: Theme.textTertiary
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.marketValueText.length > 0
                                          ? qsTr("市值 ") + modelData.marketValueText : ""
                                    elide: Text.ElideRight
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontMicro3
                                    color: Theme.textTertiary
                                }

                                FlatButton {
                                    text: qsTr("修复")
                                    kind: "ghost"
                                    Layout.preferredWidth: 52
                                    onClicked: dialog.editRecord(modelData)
                                }
                                FlatButton {
                                    text: qsTr("删除")
                                    kind: "ghost"
                                    Layout.preferredWidth: 52
                                    onClicked: dialog.deleteRecord(modelData.id)
                                }
                            }

                            MouseArea {
                                id: recordMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                propagateComposedEvents: true
                                onClicked: function(mouse) { mouse.accepted = false }
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: recordList.count === 0
                            text: qsTr("还没有记录，先在左侧填今天的收益")
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            color: Theme.textTertiary
                        }
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.borderHairline }

            RowLayout {
                Layout.fillWidth: true

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: dialog.recordId > 0
                          ? qsTr("正在修复 %1 的记录，保存后联动流水会一起更新").arg(dialog.dateText)
                          : qsTr("每日收益会计入当月收入；佣金计入当月支出；删除记录会同步删除对应流水")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro3
                    color: Theme.textTertiary
                }

                Row {
                    spacing: Theme.spaceMd
                    FlatButton { text: qsTr("取消"); kind: "ghost"; onClicked: dialog.close() }
                    FlatButton {
                        text: dialog.recordId > 0 ? qsTr("保存修复") : qsTr("保存")
                        kind: "primary"
                        onClicked: dialog.submit()
                    }
                }
            }
        }
    }

    DatePicker {
        id: datePicker
        onPicked: function(value) { dialog.dateText = value }
    }

    Shortcut { sequence: "Ctrl+Return"; enabled: dialog.visible; onActivated: dialog.submit() }
    Shortcut { sequence: "Escape"; enabled: dialog.visible; onActivated: dialog.close() }
}