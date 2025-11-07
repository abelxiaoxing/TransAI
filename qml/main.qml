import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtQuick.Layouts
import Qt.labs.platform

import "."

import Controller


Window {
    id: mainWindow
    visible: true
    width: 400
    height: 600
    minimumHeight:500
    minimumWidth:400
    title: qsTr("GPT Translator")

    // 应用主题样式
    color: "#1E1E1E"

    property Component  popComponent: null
    property QtObject  popW: null


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
    Hotkey{
       id:hotkey
       onSelectedTextChanged: {
//            popWindow(selectedText, mousePos)
           appView.inputText = selectedText
           appView.startTrans()
           mainWindow.show()
           mainWindow.raise()
           mainWindow.requestActivate()

           // 确保窗口在可见位置
           ensureWindowVisible()
       }

    }



    Component.onCompleted: {
        hotkey.binding(app)
        hotkey.setShortcut(setting.shortCut)
    }

    MouseArea{
        id:mouseArea
       anchors.fill: parent
       property variant clickPos: "1,1"
       onClicked: {

       }

       onPressed: {
           clickPos  = Qt.point(mouseX ,mouseY)
       }

       onPositionChanged: {
           var delta = Qt.point(mouseX -clickPos.x, mouseY-clickPos.y)
           mainWindow.x += delta.x;
           mainWindow.y += delta.y;
       }

       onReleased: {
           // 拖拽结束时确保窗口在可见范围内
           ensureWindowVisible();
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
            mainWindow.visible = true
        }else{
            // 当窗口失去焦点且未置顶时，触发关闭动画
            if(!appView.pinned){
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
                    text: "Quit"
                    onTriggered: {
                        Qt.quit()
                    }
                }
            }

            onActivated:{
                mainWindow.show()
                mainWindow.raise()
                mainWindow.requestActivate()

                // 确保窗口在可见位置
                ensureWindowVisible()

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
                onBackClicked: {
                    swipeView.currentIndex = 0
                }
            }
        }
    }



}
