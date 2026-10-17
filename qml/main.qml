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
    // 指针最近一次离开窗口的时间戳（Date.now() 毫秒），-1 表示本次显示以来未离开过。
    // Wayland 客户端无法感知窗口外的点击，该时间用于区分“鼠标移开导致的失焦”
    // 与“点击别处导致的失焦”（见 WindowDismissHelper）。
    property double lastPointerLeaveMs: -1

    function showMainWindow() {
        closeWindowAnimation.stop()
        mainWindow.opacity = 1
        lastPointerLeaveMs = -1
        mainWindow.show()
        mainWindow.raise()
        mainWindow.requestActivate()
        ensureWindowVisible()
    }

    // 失焦后是否自动隐藏的统一入口：置顶或拖拽中绝不隐藏，其余交给平台策略。
    // - X11：查询 X 服务器真实按键状态，仅左键点击导致的失焦才隐藏；
    // - Wayland：无法感知窗口外点击，仅当失焦明显不是指针离开引起时才隐藏；
    // - Windows/macOS：指针移动不会导致失焦，失焦即视为点击外部。
    function tryAutoHide() {
        if (mainWindow.active || appView.pinned || isDraggingWindow) {
            return
        }
        var sinceLeave = lastPointerLeaveMs < 0 ? -1 : Date.now() - lastPointerLeaveMs
        if (windowDismiss.shouldHideOnDeactivate(sinceLeave)) {
            closeWindowAnimation.start()
        }
    }

    function handleApplicationCommand(command) {
        console.log("Application command:", command)
        if (command === "translate-selection") {
            selectionTextProvider.requestSelectedText()
        } else if (command === "translate-screenshot") {
            startScreenshotCapture()
        } else if (command === "show") {
            showMainWindow()
        }
    }

    // Screenshot flow: hide the window first so it never captures itself,
    // give the compositor a beat to actually unmap it, then capture. On
    // Wayland the portal system UI takes over (region/window selection); on
    // other platforms the in-app region-selection overlays open instead.
    function startScreenshotCapture() {
        if (screenshotCapture.captureInProgress) {
            return
        }
        closeWindowAnimation.stop()
        mainWindow.visible = false
        screenshotDelayTimer.restart()
    }

    // In-app region selection (non-Wayland): one overlay per grabbed screen.
    property var regionOverlays: []

    function openRegionSelection() {
        closeRegionOverlays()
        const infos = screenshotCapture.grabbedScreenInfos()
        const screens = Qt.application.screens
        for (var i = 0; i < infos.length; ++i) {
            const overlay = regionSelectorComponent.createObject(null, {
                screenIndex: i,
                imageDataUrl: infos[i].dataUrl,
                previewWidth: infos[i].previewWidth,
                previewHeight: infos[i].previewHeight,
                screen: i < screens.length ? screens[i] : null
            })
            if (!overlay) {
                continue
            }
            overlay.accepted.connect(function(idx, x, y, w, h) {
                closeRegionOverlays()
                screenshotCapture.finishRegionSelection(idx, x, y, w, h)
            })
            overlay.canceled.connect(function() {
                closeRegionOverlays()
                screenshotCapture.cancelRegionSelection()
            })
            regionOverlays.push(overlay)
        }
        if (regionOverlays.length === 0) {
            screenshotCapture.cancelRegionSelection()
            appView.showScreenshotError(I18n.t("Screen capture is not available on this platform"))
            mainWindow.showMainWindow()
        }
    }

    function closeRegionOverlays() {
        for (var i = 0; i < regionOverlays.length; ++i) {
            regionOverlays[i].destroy()
        }
        regionOverlays = []
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
            } else if (actionId === "translateScreenshot") {
                mainWindow.handleApplicationCommand("translate-screenshot")
            }
        }
        onShortcutRegisterFailed: function(actionId, reason) {
            console.warn("Shortcut registration failed", actionId, reason)
            // Surface startup/registration failures in the settings UI instead
            // of leaving the user with a silently broken hotkey.
            settingView.hotkeyError = reason
        }
    }

    ScreenshotCaptureService {
        id: screenshotCapture
        onImageCaptured: function(image) {
            mainWindow.showMainWindow()
            appView.startScreenshotTranslation(image)
        }
        onCaptureCanceled: {
            // User dismissed the system screenshot UI; not an error, stay hidden.
        }
        onCaptureFailed: function(reason) {
            console.warn("Screenshot capture failed", reason)
            mainWindow.showMainWindow()
            appView.showScreenshotError(reason)
        }
        onRegionSelectionRequested: {
            mainWindow.openRegionSelection()
        }
    }

    Component {
        id: regionSelectorComponent
        RegionSelectorWindow {
        }
    }

    Timer {
        id: screenshotDelayTimer
        interval: 350
        repeat: false
        onTriggered: screenshotCapture.captureInteractive()
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

    // 物理左键按下（evdev 直通）：Wayland 下窗口完全收不到发生在别处的点击事件，
    // 这是唯一能感知“点击窗口外”的信号；指针在窗口内的点击由 hovered 过滤。
    // 注意 hovered 并不可靠：场景内弹窗（默认 Popup.Item，如模型下拉列表）展开时
    // 会独占悬停，指针落在弹窗上时 hovered 为 false，但那仍是窗口内点击。因此
    // 延迟一拍再判定——evdev 事件先于合成器转发的窗口事件到达，给本窗口的事件
    // 流留出记录这次按下的时间（见 outsideClickCheckTimer）。
    Connections {
        target: globalClickMonitor
        function onLeftButtonPressed() {
            if (!mainWindow.visible || windowHover.hovered || appView.pinned || isDraggingWindow) {
                return
            }
            outsideClickCheckTimer.restart()
        }
    }

    Timer {
        id: outsideClickCheckTimer
        interval: 120
        repeat: false
        onTriggered: {
            if (!mainWindow.visible || windowHover.hovered || appView.pinned || isDraggingWindow) {
                return
            }
            // 这次物理点击若同时投递到了本窗口（含场景内弹窗），则不是点击外部。
            // 阈值取 250ms：配对事件正常在数毫秒内到达，裕量足够；连续点击慢于
            // 该间隔时互不影响。
            const sincePress = windowDismiss.msecsSinceLastWindowPress()
            if (sincePress >= 0 && sincePress < 250) {
                return
            }
            closeWindowAnimation.start()
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

    QtObject {
        id: screenshotShortcutRegistrar
        function setShortcut(shortcut) {
            shortcutManager.unregisterShortcut("translateScreenshot")
            if (shortcut.length === 0) {
                return true
            }
            return shortcutManager.registerShortcut("translateScreenshot", shortcut)
        }
    }

    Component.onCompleted: {
        shortcutRegistrar.setShortcut(setting.shortCut)
        screenshotShortcutRegistrar.setShortcut(setting.screenshotShortCut)
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
            tryAutoHide()
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
            // 失焦不等于点击外部（跟随鼠标焦点时移开鼠标也会失焦），交给统一策略判断
            tryAutoHide()
        }
    }





    Item{
        anchors.fill: parent
        focus: true

        // 被动观察指针进出窗口，只记录离开时间戳，不拦截任何事件
        HoverHandler {
            id: windowHover
            onHoveredChanged: {
                if (!hovered) {
                    mainWindow.lastPointerLeaveMs = Date.now()
                }
            }
        }

        Keys.onPressed:(event)=> {
            if (event.key === Qt.Key_Escape) {
                // Esc 是显式操作，无论是否置顶都隐藏窗口
                closeWindowAnimation.start()
                event.accepted = true
                return
            }
            if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_Return ||
               (event.modifiers & Qt.MetaModifier) && event.key === Qt.Key_Return) {
              // Command+Enter or Ctrl+Enter pressed
            appView.startTrans()
            }
        }

        SystemTrayIcon {
            id: trayIcon
            visible:true
            icon.source: Qt.platform.os === "macos" ? "qrc:///res/tray.png" : "qrc:///res/logo/logo.png"
            // create menu for status bar

            menu: Menu {

                MenuItem {
                    text: I18n.t("Translate selection")
                    onTriggered: mainWindow.handleApplicationCommand("translate-selection")
                }

                MenuItem {
                    text: I18n.t("Translate screenshot")
                    onTriggered: mainWindow.handleApplicationCommand("translate-screenshot")
                }

                MenuItem {
                    text: I18n.t("Show")
                    onTriggered: mainWindow.showMainWindow()
                }

                MenuItem {
                    text: I18n.t("Quit")
                    onTriggered: {
                        // Qt.quit() bypasses the window's onClosing, so flush
                        // debounced settings edits here as well.
                        settingView.flushPendingSave()
                        Qt.quit()
                    }
                }
            }

            onActivated:{
                mainWindow.showMainWindow()
            }


        }

        // 页面切换容器：仅作为两页的宿主，无滑动手势，StackLayout 语义更准确
        StackLayout {
            id: pageStack
            currentIndex: 0
            anchors.fill: parent
            AppView{
                id:appView
                onSettingClicked: {
                    settingView.reload()
                    pageStack.currentIndex = 1
                }
                onCloseRequested: {
                    closeWindowAnimation.start()
                }
                // AppView 只上报置顶变化，由窗口本身应用 flags
                onPinRequested: function(pinned) {
                    if (pinned) {
                        mainWindow.flags = mainWindow.flags | Qt.WindowStaysOnTopHint
                    } else {
                        mainWindow.flags = mainWindow.flags & ~Qt.WindowStaysOnTopHint
                    }
                }
            }
            SettingView{
                id:settingView
                shortcutRegistrar: shortcutRegistrar
                screenshotShortcutRegistrar: screenshotShortcutRegistrar
                hotkeysSupported: shortcutManager.hotkeysSupported
                onBackClicked: {
                    settingView.flushPendingSave()
                    pageStack.currentIndex = 0
                }
            }
        }
    }



}
