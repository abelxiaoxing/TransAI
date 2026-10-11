import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtQuick.Layouts
import Qt.labs.platform

import TransAI


Window {
    id: mainWindow
    visible: !startHidden
    width: 460
    height: 700
    minimumHeight: 500
    minimumWidth: 400
    title: "TransAI"

    // 应用主题样式
    color: ThemeColors.background

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
        console.log("Application command:", command)
        if (command === "translate-selection") {
            selectionTextProvider.requestSelectedText()
        } else if (command === "show") {
            showMainWindow()
        }
    }


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
            appView.startTrans()
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
       property point clickPos: Qt.point(0, 0)

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

    onClosing: {
        // Persist debounced-but-unwritten settings edits before quitting.
        settingView.flushPendingSave()
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
