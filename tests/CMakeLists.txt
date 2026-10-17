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
)
target_include_directories(tst_setting PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_setting PRIVATE Qt6::Test)
add_test(NAME tst_setting COMMAND tst_setting)
