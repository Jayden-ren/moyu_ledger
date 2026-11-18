import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 账本切换面板：列出账本、切换、新建（默认账本不可删除）
Rectangle {
    id: panel
    width: 280
    radius: Theme.radiusPanel
    color: Theme.bgSurface
    border.width: 1
    border.color: Theme.borderBase
    visible: false
    z: 1000

    property string errorText: ""

    signal closed()

    function open() {
        errorText = ""
        newName.text = ""
        visible = true
    }

    function close() {
        visible = false
        panel.closed()
    }

    implicitHeight: content.implicitHeight + Theme.spaceSm * 2

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.spaceSm
        spacing: 2

        Text {
            text: qsTr("切换账本")
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMicro3
            font.weight: Theme.weightMedium
            color: Theme.textTertiary
            Layout.leftMargin: Theme.spaceS
            Layout.bottomMargin: Theme.spaceXs
        }

        Repeater {
            model: Ledger.ledgers

            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true
                height: 34
                radius: Theme.radiusBtn
                color: modelData.isCurrent ? Theme.bgSelection : hover.containsMouse ? Theme.bgHover : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spaceSm
                    anchors.rightMargin: Theme.spaceSm
                    spacing: Theme.spaceS

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: modelData.color && modelData.color.length > 0 ? modelData.color : Theme.accent
                    }

                    Text {
                        Layout.fillWidth: true
                        text: modelData.name
                        elide: Text.ElideRight
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall2
                        font.weight: modelData.isCurrent ? Theme.weightSemibold : Theme.weightRegular
                        color: modelData.isCurrent ? Theme.accent : Theme.textPrimary
                    }

                    Text {
                        text: modelData.accountCount + qsTr(" 账户")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }

                    Text {
                        visible: modelData.isDefault
                        text: qsTr("默认")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }
                }

                MouseArea {
                    id: hover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        const result = Ledger.switchLedger(modelData.id)
                        if (result.ok) {
                            panel.close()
                        } else {
                            panel.errorText = result.message
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spaceXs
            Layout.bottomMargin: Theme.spaceXs
            height: 1
            color: Theme.borderHairline
        }

        // 新建账本
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceXs

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                radius: Theme.radiusBtn
                color: Theme.bgSurface
                border.width: 1
                border.color: newName.activeFocus ? Theme.accent : Theme.borderStrong

                TextInput {
                    id: newName
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spaceSm
                    anchors.rightMargin: Theme.spaceSm
                    verticalAlignment: TextInput.AlignVCenter
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    color: Theme.textPrimary
                    selectionColor: Theme.accentSoftStrong
                    onAccepted: createLedger()
                    onTextChanged: panel.errorText = ""

                    Text {
                        anchors.fill: parent
                        verticalAlignment: Text.AlignVCenter
                        visible: newName.text.length === 0
                        text: qsTr("新账本名称，如 家庭账本")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall2
                        color: Theme.textTertiary
                    }
                }
            }

            FlatButton {
                text: qsTr("新建")
                kind: "primary"
                onClicked: createLedger()
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spaceS
            text: panel.errorText.length > 0
                  ? panel.errorText
                  : qsTr("示例：家庭账本 / 育婴账本 / 恋爱账本")
            wrapMode: Text.WordWrap
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMicro3
            color: panel.errorText.length > 0 ? Theme.dangerFg : Theme.textTertiary
        }
    }

    function createLedger() {
        const name = newName.text.trim()
        if (name.length === 0) {
            panel.errorText = qsTr("请输入账本名称")
            return
        }
        const result = Ledger.addLedger(name)
        if (result.ok) {
            newName.text = ""
            panel.close()
        } else {
            panel.errorText = result.message
        }
    }
}