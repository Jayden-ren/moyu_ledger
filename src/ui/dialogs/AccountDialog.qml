import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 新增 / 编辑账户面板
Item {
    id: dialog
    anchors.fill: parent
    visible: false
    z: 900

    signal saved(string message)

    property int editingId: -1
    property string type: "savings"
    property string errorText: ""

    readonly property var typeOptions: [
        { key: "savings", label: qsTr("储蓄卡") },
        { key: "checking", label: qsTr("活期") },
        { key: "credit", label: qsTr("信用卡") },
        { key: "ewallet", label: qsTr("电子钱包") },
        { key: "cash", label: qsTr("现金") },
        { key: "investment", label: qsTr("投资") },
        { key: "wealth", label: qsTr("理财") },
        { key: "stock", label: qsTr("股票") },
        { key: "receivable", label: qsTr("应收（借出）") },
        { key: "payable", label: qsTr("应付（欠款）") }
    ]

    function open() {
        editingId = -1
        type = "savings"
        errorText = ""
        nameInput.text = ""
        openingInput.text = ""
        limitInput.text = ""
        visible = true
        nameInput.forceActiveFocus()
    }

    function openForAccount(detail) {
        if (!detail || detail.id === undefined)
            return
        editingId = detail.id
        type = detail.type || "savings"
        errorText = ""
        nameInput.text = detail.name || ""
        openingInput.text = detail.openingText || ""
        limitInput.text = detail.creditLimitText || ""
        visible = true
        nameInput.forceActiveFocus()
    }

    function close() {
        visible = false
        editingId = -1
    }

    function submit() {
        const result = editingId > 0
            ? Ledger.updateAccount(editingId, nameInput.text, type, openingInput.text, limitInput.text)
            : Ledger.addAccount(nameInput.text, type, openingInput.text, limitInput.text)
        if (result.ok) {
            close()
            dialog.saved(result.message)
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
        width: 460
        height: 360
        anchors.centerIn: parent
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
                text: dialog.editingId > 0 ? qsTr("编辑账户") : qsTr("新增账户")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontH1
                font.weight: Theme.weightSemibold
                color: Theme.textPrimary
            }

            Text {
                text: qsTr("账户名")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMicro3
                color: Theme.textTertiary
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.controlH
                radius: Theme.radiusBtn
                color: Theme.bgSurface
                border.width: 1
                border.color: nameInput.activeFocus ? Theme.accent : Theme.borderStrong

                TextInput {
                    id: nameInput
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spaceLg
                    anchors.rightMargin: Theme.spaceSm
                    verticalAlignment: TextInput.AlignVCenter
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    color: Theme.textPrimary
                    selectionColor: Theme.accentSoftStrong
                    onTextChanged: dialog.errorText = ""

                    Text {
                        anchors.fill: parent
                        verticalAlignment: Text.AlignVCenter
                        visible: nameInput.text.length === 0
                        text: qsTr("例如：招商银行储蓄卡")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall2
                        color: Theme.textTertiary
                    }
                }
            }

            Text {
                text: qsTr("类型")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMicro3
                color: Theme.textTertiary
            }

            Flow {
                Layout.fillWidth: true
                spacing: Theme.spaceS

                Repeater {
                    model: dialog.typeOptions

                    delegate: Rectangle {
                        required property var modelData
                        width: typeLabel.implicitWidth + Theme.spaceXl
                        height: 28
                        radius: Theme.radiusBtn
                        color: dialog.type === modelData.key ? Theme.accentSoft : Theme.bgSurface
                        border.width: 1
                        border.color: dialog.type === modelData.key ? Theme.accent : Theme.borderStrong

                        Text {
                            id: typeLabel
                            anchors.centerIn: parent
                            text: modelData.label
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            color: dialog.type === modelData.key ? Theme.accent : Theme.textPrimary
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: dialog.type = modelData.key
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spaceMd

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    Text {
                        text: qsTr("期初余额")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.controlH
                        radius: Theme.radiusBtn
                        color: Theme.bgSurface
                        border.width: 1
                        border.color: openingInput.activeFocus ? Theme.accent : Theme.borderStrong
                        TextInput {
                            id: openingInput
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spaceLg
                            anchors.rightMargin: Theme.spaceSm
                            verticalAlignment: TextInput.AlignVCenter
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            color: Theme.textPrimary
                            selectionColor: Theme.accentSoftStrong
                            Text {
                                anchors.fill: parent
                                verticalAlignment: Text.AlignVCenter
                                visible: openingInput.text.length === 0
                                text: "0.00"
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSmall2
                                color: Theme.textTertiary
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    visible: dialog.type === "credit"
                    Text {
                        text: qsTr("信用额度")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.controlH
                        radius: Theme.radiusBtn
                        color: Theme.bgSurface
                        border.width: 1
                        border.color: limitInput.activeFocus ? Theme.accent : Theme.borderStrong
                        TextInput {
                            id: limitInput
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

    Shortcut { sequence: "Ctrl+Return"; enabled: dialog.visible; onActivated: dialog.submit() }
    Shortcut { sequence: "Escape"; enabled: dialog.visible; onActivated: dialog.close() }
}