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
target_compile_definitions(tst_i18n PRIVATE I18N_QML_SOURCE="${CMAKE_SOURCE_DIR}/qml/I18n.qml")
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
    ${CMAKE_SOURCE_DIR}/src/translation/TranslationRequest.cpp
    ${CMAKE_SOURCE_DIR}/src/translation/PromptBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/translation/StreamParser.cpp
    ${CMAKE_SOURCE_DIR}/src/providers/ProviderEndpointBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/providers/ThinkingPolicy.cpp
)
target_include_directories(tst_streamingchatclient PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_streamingchatclient PRIVATE Qt6::Network Qt6::Test)
add_test(NAME tst_streamingchatclient COMMAND tst_streamingchatclient)

add_executable(tst_translationhistory
    tst_translationhistory.cpp
    ${CMAKE_SOURCE_DIR}/src/app/TranslationHistory.cpp
)
target_include_directories(tst_translationhistory PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_translationhistory PRIVATE Qt6::Test)
add_test(NAME tst_translationhistory COMMAND tst_translationhistory)

# OCR language-selection helpers live in the (platform-neutral) tesseract
# backend source; the test does not need a tesseract binary.
add_executable(tst_ocrengine
    tst_ocrengine.cpp
    ${CMAKE_SOURCE_DIR}/src/ocr/OcrEngine.h
    ${CMAKE_SOURCE_DIR}/src/ocr/TesseractOcrEngine.cpp
)
target_include_directories(tst_ocrengine PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_ocrengine PRIVATE Qt6::Gui Qt6::Test)
add_test(NAME tst_ocrengine COMMAND tst_ocrengine)
