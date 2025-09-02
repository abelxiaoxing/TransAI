import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Controller


import QtQuick.Controls.Material
import "."

Item {
    id:root
    property bool pinned: false
    property alias inputText: inputArea.text
    signal settingClicked;
    function startTrans(){
        if(inputArea.text.length > 0 && transBtn.visible)
            transBtn.clicked()
    }

    function getMode(){
        return 0
    }

    Component.onCompleted: {
        inputAni.to = root.height /3
        inputAni.start()
    }

    Item{
        id:header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right:parent.right
        anchors.margins: 15

        height:30

        Text {
            text: "Translation"
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            font.bold: true
            color: "green"
        }


        IconButton{
            id:settingBtn
            width: 18
            height:18
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            normalUrl:"qrc:///res/setting1.png"
            hoveredUrl:"qrc:///res/setting1.png"
            pressedUrl:"qrc:///res/setting2.png"
            onClicked: {
                settingClicked();
            }
        }

    }

    Item{
        id:inputItem
        anchors.margins: 10
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        clip:true
        Rectangle {
            radius: 6
            color: "transparent"
            border.width : 1
            border.color: "green"
            anchors.fill: parent
        }
        GTextEdit{
            id:inputArea
            anchors.fill: parent
            autoScroll:false
            textedit.focus : true
        }
        NumberAnimation on height {
            id:inputAni
            to: root.height/3
            duration:200
        }
    }

    Item{
        id:tItem
        width: 22
        height:22
        anchors.top: inputItem.bottom
        anchors.topMargin: 10
        anchors.right:inputItem.right
        state:"no"
        Image{
            id:tyes
            anchors.fill: parent
            source:"qrc:///res/thumbtack_yes.png"
            visible:tItem.state === "yes"
            MouseArea{
                anchors.fill: parent
                onClicked: {
                    mainWindow.flags = mainWindow.flags & (0xFFFFFFFF ^ Qt.WindowStaysOnTopHint)
                    tItem.state = "no"
                    pinned = false

                }
            }
        }
        Image{
            id:tyno
            anchors.fill: parent
            source:"qrc:///res/thumbtack_no.png"
            visible:tItem.state === "no"
            MouseArea{
                anchors.fill: parent
                onClicked: {
                    mainWindow.flags = mainWindow.flags |Qt.WindowStaysOnTopHint
                    tItem.state = "yes"
                    pinned = true

                }
            }
        }
    }

    Text{
        id:indictor
        text:"Translated"
        anchors.left: inputItem.left
        anchors.top:inputItem.bottom
        font.bold: true
        color:"green"
        anchors.topMargin: 40

    }

    

    GTextEdit{
        id:result
        anchors.left: inputItem.left
        anchors.right: inputItem.right
        anchors.top:indictor.bottom
        anchors.topMargin: 5
        anchors.bottom: langSelector.top
        anchors.bottomMargin: 10
        autoScroll:true
        readOnly:true
    }

    Button{
        id:transBtn
        anchors.bottom: parent.bottom
        anchors.right:parent.right
        anchors.margins:10
        text:(Qt.platform.os == "macos" || Qt.platform.os == "osx")?"Translate ⌘R":"Translate ^R"
        font.capitalization: Font.MixedCase
        enabled:inputArea.text.length > 0
        onClicked: {
            api.sendMessage(inputArea.text, getMode())
        }
        height:50
        Material.background: Material.Green
        Material.foreground :(Qt.platform.os === "linux")?"black":"white" //linux can't display button use software render
    }
    BusyIndicator {
        anchors.verticalCenter: stopBtn.verticalCenter
        anchors.right:stopBtn.left
        anchors.rightMargin: 10
        running: api.isRequesting
        visible:api.isRequesting
        width:transBtn.height - 10
        height:width
    }
    Button{
        id:stopBtn
        visible:false
        anchors.bottom: parent.bottom
        anchors.right:parent.right
        anchors.margins:10
        text:"stop"
        onClicked: {
            api.abort()
        }
        Material.background: Material.Green
        Material.foreground :(Qt.platform.os === "linux")?"black":"white" //linux can't display button use software render
    }

    ComboBox {
        id:langSelector
        anchors.bottom: parent.bottom
        anchors.left:parent.left
        anchors.margins:10
        currentIndex: 0
        model:["简体中文","繁体中文", "English", "Japanse", "German", "Korean", "Español", "français"]
        onCurrentTextChanged: {
            api.transToLang = currentText
        }
        height:40
    }

    APIController{
        id:api
        onResponseDataChanged: {
            result.text = responseData
        }
        onResponseErrorChanged: {
            if(responseError != ""){
                result.text = responseError + ":\n" + result.text
            }
        }
        onIsRequestingChanged: {
            if(isRequesting){
                transBtn.visible = false
                stopBtn.visible = true
            }else{
                transBtn.visible = true
                stopBtn.visible = false
            }
        }
        Component.onCompleted: {
            api.apiKey = setting.apiKey
            api.model = setting.model
            api.apiServer = setting.apiServer
        }
    }
    Connections{
        target: setting
        function onApiServerChanged(){
            api.apiServer = setting.apiServer
        }
        function onApiKeyChanged(){
            api.apiKey = setting.apiKey
        }
        function onModelChanged(){
            api.model = setting.model
        }
    }
}
