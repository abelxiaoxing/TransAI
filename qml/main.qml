import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtQuick.Layouts
import Qt.labs.platform

import "."

import TransAI


Window {
    id: mainWindow
    visible: !startHidden
    width: 460
    height: 700
    minimumHeight: 500
    minimumWidth: 400
    title: qsTr("GPT Translator")

    // 应用主题样式
    color: "#1E1E1E"

    property Component  popComponent: null
    property QtObject  popW: null
    property bool isDraggingWindow: false
    property bool usingSystemMove: false

    function showMainWindow() {
        closeWindowAnimation.stop()
        mainWindow.opacity = 1
        mainWindow.show()
        mainWindow.raise()
        mainWindow.requestActivate()
        ensureWindowVisible()
    }

    function handleApplicationCommand(command) {
        if (command === "translate-selection") {
            selectionTextProvider.requestSelectedText()
        } else if (command === "show") {
            showMainWindow()
        }
    }


//    flags:Qt.Window | Qt.FramelessWindowHint | Qt.WindowTitleHint | Qt.WindowSystemMenuHint

    // 确保窗口位置在屏幕可见范围内的函数
    function ensureWindowVisible() {
        var screenWidth = Screen.width;
        var screenHeight = Screen.height;
        var windowWidth = mainWindow.width;
        var windowHeight = mainWindow.height;

        // 确保窗口不会超出屏幕右边界
        if (mainWindow.x + windowWidth > screenWidth) {
            mainWindow.x = screenWidth - windowWidth;
        }

        // 确保窗口不会超出屏幕下边界
        if (mainWindow.y + windowHeight > screenHeight) {
            mainWindow.y = screenHeight - windowHeight;
        }

        // 确保窗口不会超出屏幕左边界
        if (mainWindow.x < 0) {
            mainWindow.x = 0;
        }

        // 确保窗口不会超出屏幕上边界
        if (mainWindow.y < 0) {
            mainWindow.y = 0;
        }
    }

    function popWindow(t, pos){

        if(popComponent !== null){
            popW.close()
             popComponent.destroy()
        }

        popComponent = Qt.createComponent("GPopWindow.qml")
        if (popComponent.status === Component.Ready) {
            popW = popComponent.createObject(mainWindow)
            if (popW) {
                popW.x = pos.x
                popW.y = pos.y
                popW.visible = true
                popW.show()
                popW.raise()
                popW.requestActivate()
                mainWindow.visible = false
            } else {
                console.error("Error creating new window:", popComponent.errorString())
            }
        } else {
            console.error("Error loading DynamicWindow component:", popComponent.errorString())
        }

    }
    function beginWindowDrag() {
        closeWindowAnimation.stop()
        mainWindow.opacity = 1
        dragStateResetTimer.stop()
        isDraggingWindow = true
        usingSystemMove = mainWindow.startSystemMove()
    }

    function finishWindowDrag() {
        usingSystemMove = false
        dragStateResetTimer.restart()
    }

    GlobalShortcutManager {
        id: shortcutManager
        onShortcutActivated: function(actionId) {
            if (actionId === "translateSelection") {
                mainWindow.handleApplicationCommand("translate-selection")
            }
        }
        onShortcutRegisterFailed: function(actionId, reason) {
            console.warn("Shortcut registration failed", actionId, reason)
        }
    }

    SelectionTextProvider {
        id: selectionTextProvider
        onSelectedTextReady: function(text) {
            appView.inputText = text
            appView.startTrans("selectedText")
            mainWindow.showMainWindow()
        }
        onSelectedTextFailed: function(reason) {
            console.warn("Selected text unavailable", reason)
            appView.showSelectionError(reason)
            mainWindow.showMainWindow()
        }
    }

    Connections {
        target: appCommandServer
        function onCommandReceived(command) {
            mainWindow.handleApplicationCommand(command)
        }
    }

    QtObject {
        id: shortcutRegistrar
        function setShortcut(shortcut) {
            shortcutManager.unregisterShortcut("translateSelection")
            if (shortcut.length === 0) {
                return true
            }
            return shortcutManager.registerShortcut("translateSelection", shortcut)
        }
    }

    Component.onCompleted: {
        shortcutRegistrar.setShortcut(setting.shortCut)
        if (initialCommand.length > 0) {
            Qt.callLater(function() {
                mainWindow.handleApplicationCommand(initialCommand)
            })
        }
    }

    MouseArea{
        id:mouseArea
       anchors.fill: parent
       property variant clickPos: "1,1"
       onClicked: {

       }

       acceptedButtons: Qt.LeftButton

       onPressed: {
           clickPos  = Qt.point(mouseX ,mouseY)
           beginWindowDrag()
       }

       onPositionChanged: {
           if (usingSystemMove) {
               return
           }

           var delta = Qt.point(mouseX -clickPos.x, mouseY-clickPos.y)
           mainWindow.x += delta.x;
           mainWindow.y += delta.y;
       }

       onReleased: {
           finishWindowDrag()
       }

       onCanceled: {
            finishWindowDrag()
       }
   }

  
    Timer {
        id: dragStateResetTimer
        interval: 120
        repeat: false
        onTriggered: {
            isDraggingWindow = false
            ensureWindowVisible()
            if (!mainWindow.active && !appView.pinned) {
                closeWindowAnimation.start()
            }
        }
    }

    // 窗口关闭动画
    SequentialAnimation {
        id: closeWindowAnimation

        NumberAnimation {
            target: mainWindow
            property: "opacity"
            to: 0
            duration: 250
            easing.type: Easing.OutQuad
        }

        ScriptAction {
            script: {
                mainWindow.visible = false;
                mainWindow.opacity = 1; // 重置透明度
            }
        }
    }

    onActiveChanged: {
        if(active){
            closeWindowAnimation.stop()
            mainWindow.opacity = 1
            mainWindow.visible = true
        }else{
            // 当窗口失去焦点且未置顶时，触发关闭动画
            if(!appView.pinned && !isDraggingWindow){
                closeWindowAnimation.start();
            }
        }
    }





    Item{
        anchors.fill: parent
        focus: true
        Keys.onPressed:(event)=> {
            if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_Return ||
               (event.modifiers & Qt.MetaModifier) && event.key === Qt.Key_Return) {
              // Command+Enter or Ctrl+Enter pressed
            appView.startTrans()
            }
        }

        SystemTrayIcon {
            id: trayIcon
            visible:true
            icon.source: (Qt.platform.os === "macos" || Qt.platform.os === "osx")?"qrc:///res/tray.png":"qrc:///res/logo/logo.png"
            // create menu for status bar

            menu: Menu {

                MenuItem {
                    text: "Translate selection"
                    onTriggered: mainWindow.handleApplicationCommand("translate-selection")
                }

                MenuItem {
                    text: "Show"
                    onTriggered: mainWindow.showMainWindow()
                }

                MenuItem {
                    text: "Quit"
                    onTriggered: {
                        Qt.quit()
                    }
                }
            }

            onActivated:{
                mainWindow.showMainWindow()

//                mainWindow.x = trayIcon.geometry.x - mainWindow.width/2
//                mainWindow.y = trayIcon.geometry.y + 50
//                mainWindow.visible = true
            }


        }

        SwipeView {
            id: swipeView
            currentIndex: 0
            anchors.fill: parent
            orientation: Qt.Horizontal
            interactive: false
            AppView{
                id:appView
                height:mainWindow.height
                onSettingClicked: {
                    settingView.reload()
                    swipeView.currentIndex = 1
                }
            }
            SettingView{
                id:settingView
                shortcutRegistrar: shortcutRegistrar
                onBackClicked: {
                    swipeView.currentIndex = 0
                }
            }
        }
    }



}
