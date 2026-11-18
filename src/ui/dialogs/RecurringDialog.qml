import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 定期账单：新增 / 编辑规则（按月在第 N 天自动生成一笔）
Item {
    id: dialog
    anchors.fill: parent
    visible: false
    z: 900

    signal notifyRequested(string message)

    property int editingId: -1
    property string kind: "expense"
    property int selectedAccountId: -1
    property int selectedCategoryId: -1
    property int dayOfMonth: 1
    property string errorText: ""

    function open() {
        editingId = -1
        kind = "expense"
        selectedAccountId = Ledger.defaultAccountId()
        selectedCategoryId = Ledger.defaultCategoryId("expense")
        dayOfMonth = 1
        amountInput.text = ""
        noteInput.text = ""
        errorText = ""
        visible = true
        amountInput.forceActiveFocus()
    }

    function openForRule(rule) {
        if (!rule || rule.id === undefined)
            return
        editingId = rule.id
        kind = rule.kind
        selectedAccountId = rule.accountId
        selectedCategoryId = rule.categoryId
        dayOfMonth = rule.dayOfMonth
        amountInput.text = rule.amountInput
        noteInput.text = rule.note
        errorText = ""
        visible = true
        amountInput.forceActiveFocus()
    }

    function close() { visible = false }

    function submit() {
        const result = editingId > 0
            ? Ledger.updateRecurring(editingId, kind, amountInput.text, selectedAccountId,
                                     selectedCategoryId, noteInput.text, dayOfMonth)
            : Ledger.addRecurring(kind, amountInput.text, selectedAccountId, selectedCategoryId,
                                  noteInput.text, dayOfMonth)
        if (result.ok) {
            close()
            dialog.notifyRequested(result.message)
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
        width: 520
        anchors.centerIn: parent
        height: 430
        radius: Theme.radiusPanel
        color: Theme.bgSurface
        border.width: 1
        border.color: Theme.borderBase

        MouseArea { anchors.fill: parent }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.pagePadding
            spacing: Theme.spaceMd

            Text {
                text: dialog.editingId > 0 ? qsTr("编辑定期账单") : qsTr("新增定期账单")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontH1
                font.weight: Theme.weightSemibold
                color: Theme.textPrimary
            }

            // 收/支
            Row {
                spacing: Theme.spaceXs

                Repeater {
                    model: [{ key: "expense", label: qsTr("支出") }, { key: "income", label: qsTr("收入") }]

                    delegate: Rectangle {
                        required property var modelData
                        width: kindLabel.implicitWidth + Theme.spaceXl
                        height: 28
                        radius: Theme.radiusBtn
                        color: dialog.kind === modelData.key ? Theme.accentSoft : Theme.bgSurface
                        border.width: 1
                        border.color: dialog.kind === modelData.key ? Theme.accent : Theme.borderStrong

                        Text {
                            id: kindLabel
                            anchors.centerIn: parent
                            text: modelData.label
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            color: dialog.kind === modelData.key ? Theme.accent : Theme.textPrimary
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: dialog.kind = modelData.key
                        }
                    }
                }
            }

            // 金额 + 日期
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spaceMd

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    Text { text: qsTr("金额"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.controlH
                        radius: Theme.radiusBtn
                        color: Theme.bgSurface
                        border.width: 1
                        border.color: amountInput.activeFocus ? Theme.accent : Theme.borderStrong
                        TextInput {
                            id: amountInput
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spaceLg
                            verticalAlignment: TextInput.AlignVCenter
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            color: Theme.textPrimary
                            selectionColor: Theme.accentSoftStrong
                        }
                    }
                }

                ColumnLayout {
                    Layout.preferredWidth: 120
                    spacing: 3
                    Text { text: qsTr("每月几号（1-28）"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.controlH
                        radius: Theme.radiusBtn
                        color: Theme.bgSurface
                        border.width: 1
                        border.color: Theme.borderStrong
                        TextInput {
                            id: dayInput
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spaceLg
                            verticalAlignment: TextInput.AlignVCenter
                            text: dialog.dayOfMonth
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            font.features: ({ "tnum": 1 })
                            color: Theme.textPrimary
                            selectionColor: Theme.accentSoftStrong
                            validator: IntValidator { bottom: 1; top: 28 }
                            onTextChanged: dialog.dayOfMonth = Math.max(1, Math.min(28, parseInt(text) || 1))
                        }
                    }
                }
            }

            // 备注
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Text { text: qsTr("备注（如：房租、网飞订阅）"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.controlH
                    radius: Theme.radiusBtn
                    color: Theme.bgSurface
                    border.width: 1
                    border.color: noteInput.activeFocus ? Theme.accent : Theme.borderStrong
                    TextInput {
                        id: noteInput
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spaceLg
                        verticalAlignment: TextInput.AlignVCenter
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall2
                        color: Theme.textPrimary
                        selectionColor: Theme.accentSoftStrong
                    }
                }
            }

            // 账户
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spaceXs
                Text { text: qsTr("账户"); font.family: Theme.fontFamily; font.pixelSize: Theme.fontMicro3; color: Theme.textTertiary }
                Repeater {
                    model: Ledger.accountModel
                    delegate: Rectangle {
                        required property string name
                        required property int accountId
                        width: accLabel.implicitWidth + Theme.spaceLg
                        height: 24
                        radius: Theme.radiusBtn
                        color: dialog.selectedAccountId === accountId ? Theme.accentSoft : Theme.bgSurface
                        border.width: 1
                        border.color: dialog.selectedAccountId === accountId ? Theme.accent : Theme.borderStrong
                        Text {
                            id: accLabel
                            anchors.centerIn: parent
                            text: name
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: dialog.selectedAccountId === accountId ? Theme.accent : Theme.textPrimary
                        }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: dialog.selectedAccountId = accountId }
                    }
                }
                Item { Layout.fillWidth: true }
            }

            Text {
                Layout.fillWidth: true
                text: dialog.errorText
                visible: dialog.errorText.length > 0
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall2
                color: Theme.dangerFg
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Row {
                    spacing: Theme.spaceMd
                    FlatButton { text: qsTr("取消"); kind: "ghost"; onClicked: dialog.close() }
                    FlatButton { text: qsTr("保存"); kind: "primary"; onClicked: dialog.submit() }
                }
            }
        }
    }

    Shortcut { sequence: "Escape"; enabled: dialog.visible; onActivated: dialog.close() }
}