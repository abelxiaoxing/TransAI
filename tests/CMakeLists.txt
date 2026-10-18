# Unit tests. Added only when Qt6::Test is available; the parent
# CMakeLists.txt guards on the same condition.
add_executable(tst_streamparser
    tst_streamparser.cpp
    ${CMAKE_SOURCE_DIR}/src/translation/StreamParser.cpp
)
target_include_directories(tst_streamparser PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_streamparser PRIVATE Qt6::Test)
add_test(NAME tst_streamparser COMMAND tst_streamparser)

add_executable(tst_thinkingpolicy
    tst_thinkingpolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/providers/ThinkingPolicy.cpp
)
target_include_directories(tst_thinkingpolicy PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_thinkingpolicy PRIVATE Qt6::Test)
add_test(NAME tst_thinkingpolicy COMMAND tst_thinkingpolicy)

add_executable(tst_providerendpointbuilder
    tst_providerendpointbuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/providers/ProviderEndpointBuilder.cpp
)
target_include_directories(tst_providerendpointbuilder PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_providerendpointbuilder PRIVATE Qt6::Test)
add_test(NAME tst_providerendpointbuilder COMMAND tst_providerendpointbuilder)

# I18n.qml is exercised as a QML singleton; the test stages a module
# directory at runtime, so it only needs the source path to copy from.
add_executable(tst_i18n
    tst_i18n.cpp
)
target_compile_definitions(tst_i18n PRIVATE
    I18N_QML_SOURCE="${CMAKE_SOURCE_DIR}/qml/I18n.qml"
    I18N_QML_DIR="${CMAKE_SOURCE_DIR}/qml")
target_link_libraries(tst_i18n PRIVATE Qt6::Quick Qt6::Test)
add_test(NAME tst_i18n COMMAND tst_i18n)

add_executable(tst_visionmodelcapability
    tst_visionmodelcapability.cpp
)
target_include_directories(tst_visionmodelcapability PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_visionmodelcapability PRIVATE Qt6::Test)
add_test(NAME tst_visionmodelcapability COMMAND tst_visionmodelcapability)

add_executable(tst_imagepayloadbuilder
    tst_imagepayloadbuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/vision/ImagePayloadBuilder.cpp
)
target_include_directories(tst_imagepayloadbuilder PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_imagepayloadbuilder PRIVATE Qt6::Gui Qt6::Test)
add_test(NAME tst_imagepayloadbuilder COMMAND tst_imagepayloadbuilder)

# Setting only pulls in header-only helpers (ProviderDefaults,
# VisionModelCapability), so Qt6::Test (Core) is enough.
add_executable(tst_setting
    tst_setting.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/Setting.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/SecretStoreNull.cpp
)
target_include_directories(tst_setting PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_setting PRIVATE Qt6::Test)
add_test(NAME tst_setting COMMAND tst_setting)

# End-to-end streaming test against a scripted in-process HTTP server; needs
# Network for QTcpServer/QTcpSocket and the full translation module chain.
add_executable(tst_streamingchatclient
    tst_streamingchatclient.cpp
    ${CMAKE_SOURCE_DIR}/src/translation/TranslationService.cpp
    ${CMAKE_SOURCE_DIR}/src/translation/StreamingChatClient.cpp
    ${CMAKE_SOURCE_DIR}/src/translation/PromptBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/translation/StreamParser.cpp
    ${CMAKE_SOURCE_DIR}/src/providers/NetworkErrorMapper.cpp
    ${CMAKE_SOURCE_DIR}/src/providers/ProviderEndpointBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/providers/ThinkingPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/vision/VisionTranslationService.cpp
    ${CMAKE_SOURCE_DIR}/src/vision/ImagePayloadBuilder.cpp
)
target_include_directories(tst_streamingchatclient PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_streamingchatclient PRIVATE Qt6::Gui Qt6::Network Qt6::Test)
add_test(NAME tst_streamingchatclient COMMAND tst_streamingchatclient)

add_executable(tst_translationhistory
    tst_translationhistory.cpp
    ${CMAKE_SOURCE_DIR}/src/app/TranslationHistory.cpp
)
target_include_directories(tst_translationhistory PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_translationhistory PRIVATE Qt6::Test)
add_test(NAME tst_translationhistory COMMAND tst_translationhistory)

# OCR language-selection helpers live in the (platform-neutral) tesseract
# fallback backend source; the test does not need a tesseract binary.
add_executable(tst_ocrengine
    tst_ocrengine.cpp
    ${CMAKE_SOURCE_DIR}/src/ocr/OcrEngine.h
    ${CMAKE_SOURCE_DIR}/src/ocr/TesseractOcrEngine.cpp
)
target_include_directories(tst_ocrengine PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_ocrengine PRIVATE Qt6::Gui Qt6::Test)
add_test(NAME tst_ocrengine COMMAND tst_ocrengine)

# Pure post-processing of the bundled OCR backend (score map -> boxes, reading
# order, CTC decoding, model discovery). No ncnn, no model files.
add_executable(tst_ocrpostprocess
    tst_ocrpostprocess.cpp
    ${CMAKE_SOURCE_DIR}/src/ocr/OcrPostProcess.cpp
    ${CMAKE_SOURCE_DIR}/src/ocr/OcrModelLocator.cpp
)
target_include_directories(tst_ocrpostprocess PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_ocrpostprocess PRIVATE Qt6::Test)
add_test(NAME tst_ocrpostprocess COMMAND tst_ocrpostprocess)

# End-to-end recognition with the bundled models. Only built when the bundled
# backend is active; the test skips itself when the model files are absent, so
# a fresh checkout without scripts/fetch_ocr_models.sh still passes.
if(TRANSAI_OCR_BACKEND_ACTIVE STREQUAL "ncnn")
    add_executable(tst_rapidocr
        tst_rapidocr.cpp
        ${CMAKE_SOURCE_DIR}/src/ocr/OcrEngine.h
        ${CMAKE_SOURCE_DIR}/src/ocr/RapidOcrEngine.cpp
        ${CMAKE_SOURCE_DIR}/src/ocr/OcrPostProcess.cpp
        ${CMAKE_SOURCE_DIR}/src/ocr/OcrModelLocator.cpp
    )
    target_include_directories(tst_rapidocr PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_compile_definitions(tst_rapidocr PRIVATE
        TRANSAI_OCR_SOURCE_MODEL_DIR="${TRANSAI_OCR_MODEL_DIR}")
    target_link_libraries(tst_rapidocr PRIVATE Qt6::Gui Qt6::Test Qt6::Concurrent ncnn)
    add_test(NAME tst_rapidocr COMMAND tst_rapidocr)
    # QPainter needs a font database, which needs a platform plugin.
    set_tests_properties(tst_rapidocr PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

add_executable(tst_networkerrormapper
    tst_networkerrormapper.cpp
    ${CMAKE_SOURCE_DIR}/src/providers/NetworkErrorMapper.cpp
)
target_include_directories(tst_networkerrormapper PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_networkerrormapper PRIVATE Qt6::Network Qt6::Test)
add_test(NAME tst_networkerrormapper COMMAND tst_networkerrormapper)

# Pure geometry: the preview-pixel → original-image mapping used by the
# in-app region selection overlay (header-only helper).
add_executable(tst_screenshotgeometry
    tst_screenshotgeometry.cpp
)
target_include_directories(tst_screenshotgeometry PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_screenshotgeometry PRIVATE Qt6::Gui Qt6::Test)
add_test(NAME tst_screenshotgeometry COMMAND tst_screenshotgeometry)

# Window-dismiss policy needs a QGuiApplication (offscreen) for QWindow.
add_executable(tst_windowdismiss
    tst_windowdismiss.cpp
    ${CMAKE_SOURCE_DIR}/src/app/WindowDismissHelper.cpp
)
target_include_directories(tst_windowdismiss PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_windowdismiss PRIVATE Qt6::Gui Qt6::Test)
# WindowDismissHelper queries the X server directly on Linux.
if(TARGET X11::X11)
    target_link_libraries(tst_windowdismiss PRIVATE X11::X11)
endif()
add_test(NAME tst_windowdismiss COMMAND tst_windowdismiss)

# Single-instance command socket: forwarding + stale-socket recovery.
add_executable(tst_applicationcommandserver
    tst_applicationcommandserver.cpp
    ${CMAKE_SOURCE_DIR}/src/app/ApplicationCommandServer.cpp
)
target_include_directories(tst_applicationcommandserver PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_applicationcommandserver PRIVATE Qt6::Network Qt6::Test)
add_test(NAME tst_applicationcommandserver COMMAND tst_applicationcommandserver)

# Hotkey uniqueness arbitration (header-only): one chord = one action.
add_executable(tst_shortcutconflict
    tst_shortcutconflict.cpp
)
target_include_directories(tst_shortcutconflict PRIVATE ${CMAKE_SOURCE_DIR}/src)
# No GUI in this suite: linking only Qt6::Test makes QTEST_MAIN use a
# QCoreApplication, which is what lets it run on a headless CI runner.
target_link_libraries(tst_shortcutconflict PRIVATE Qt6::Test)
add_test(NAME tst_shortcutconflict COMMAND tst_shortcutconflict)
