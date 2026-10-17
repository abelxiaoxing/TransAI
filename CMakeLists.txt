cmake_minimum_required(VERSION 3.16)

project(TransAI VERSION 0.1 LANGUAGES CXX)


set(CMAKE_AUTOMOC ON) # Meta-Object Compiler
set(CMAKE_AUTORCC ON) # Resource Compiler
set(CMAKE_AUTOUIC ON) # User Interface Compiler

set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED APP_VERSION OR APP_VERSION STREQUAL "")
    set(APP_VERSION "${PROJECT_VERSION}")
endif()
add_compile_definitions(APP_VERSION="${APP_VERSION}")

message(STATUS "APP_VERSION: ${APP_VERSION}")

if (CMAKE_SYSTEM_NAME MATCHES "Linux"
    AND NOT DEFINED Qt6_DIR AND NOT DEFINED CMAKE_PREFIX_PATH)
    # Auto-detect a system Qt6 via qmake. Explicitly passed -DQt6_DIR or
    # -DCMAKE_PREFIX_PATH always win; if neither is given and qmake has no
    # Qt6, find_package below fails with a hint to pass one of them.
    execute_process(
        COMMAND qmake -query QT_INSTALL_PREFIX
        OUTPUT_VARIABLE SYSTEM_QT_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(EXISTS "${SYSTEM_QT_PREFIX}/lib/cmake/Qt6")
        set(Qt6_DIR "${SYSTEM_QT_PREFIX}/lib/cmake/Qt6")
        message(STATUS "Using system Qt6: ${SYSTEM_QT_PREFIX}")
    endif()
endif()
find_package(Qt6 REQUIRED COMPONENTS Quick Network)
if(Qt6_VERSION_MAJOR EQUAL 6 AND Qt6_VERSION_MINOR GREATER_EQUAL 5)
    qt6_policy(SET QTP0001 OLD)
endif()


set(HEADERS
    src/PropertyMacros.h
    src/app/ApplicationCommandServer.h
    src/app/ClipboardHelper.h
    src/app/TranslationHistory.h
    src/app/WindowDismissHelper.h
    src/settings/Setting.h
    src/settings/SecretStore.h
    src/ocr/OcrEngine.h
    src/translation/TranslationRequest.h
    src/translation/PromptBuilder.h
    src/translation/StreamParser.h
    src/translation/StreamingChatClient.h
    src/translation/TranslationService.h
    src/providers/ProviderDefaults.h
    src/providers/ApiError.h
    src/providers/ProviderEndpointBuilder.h
    src/providers/ModelDiscoveryService.h
    src/providers/ThinkingPolicy.h
    src/input/GlobalShortcutManager.h
    src/input/GlobalClickMonitor.h
    src/input/ClipboardGuard.h
    src/input/SelectionTextProvider.h
    src/capture/ScreenshotCaptureService.h
    src/capture/PortalScreenshotBackend.h
    src/vision/ImagePayloadBuilder.h
    src/vision/VisionModelCapability.h
    src/vision/VisionTranslationService.h
)
set(SOURCES
    src/main.cpp
    src/app/ApplicationCommandServer.cpp
    src/app/ClipboardHelper.cpp
    src/app/TranslationHistory.cpp
    src/app/WindowDismissHelper.cpp
    src/settings/Setting.cpp
    src/translation/TranslationRequest.cpp
    src/translation/PromptBuilder.cpp
    src/translation/StreamParser.cpp
    src/translation/StreamingChatClient.cpp
    src/translation/TranslationService.cpp
    src/providers/ProviderEndpointBuilder.cpp
    src/providers/ModelDiscoveryService.cpp
    src/providers/ThinkingPolicy.cpp
    src/input/GlobalShortcutManager.cpp
    src/input/GlobalClickMonitor.cpp
    src/input/ClipboardGuard.cpp
    src/input/SelectionTextProvider.cpp
    src/capture/ScreenshotCaptureService.cpp
    src/capture/PortalScreenshotBackend.cpp
    src/vision/ImagePayloadBuilder.cpp
    src/vision/VisionTranslationService.cpp
)

set(RESOURCES
    TransAI.qrc
)

add_subdirectory(lib/)


IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
	MESSAGE(STATUS "current platform: Linux ")
    find_package(X11 REQUIRED)

    qt_add_executable(TransAI
        ${SOURCES} ${HEADERS} ${RESOURCES}
    )

    # API-key secure storage: libsecret backend when the dev package is
    # installed; otherwise a null backend (plaintext fallback, as before).
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSECRET libsecret-1)
    if (LIBSECRET_FOUND)
        message(STATUS "SecretStore backend: libsecret")
        target_sources(TransAI PRIVATE src/settings/SecretStoreLinux.cpp)
        # glib headers use `signals` as a struct field; Qt's keyword macros
        # must not leak into this translation unit.
        set_source_files_properties(src/settings/SecretStoreLinux.cpp
            PROPERTIES COMPILE_DEFINITIONS QT_NO_KEYWORDS)
        target_compile_definitions(TransAI PRIVATE TRANSAI_HAVE_LIBSECRET)
        target_include_directories(TransAI PRIVATE ${LIBSECRET_INCLUDE_DIRS})
        target_link_libraries(TransAI PRIVATE ${LIBSECRET_LIBRARIES})
    else()
        message(STATUS "SecretStore backend: none (libsecret not found)")
        target_sources(TransAI PRIVATE src/settings/SecretStoreNull.cpp)
    endif()

    # OCR backend: tesseract CLI (optional at runtime).
    target_sources(TransAI PRIVATE src/ocr/TesseractOcrEngine.cpp)
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
	MESSAGE(STATUS "current platform: Windows")

    # set(CMAKE_EXE_LINKER_FLAGS "-static") use target_link_libraries instead
    set(app_icon_resource_windows "${CMAKE_CURRENT_SOURCE_DIR}/res/logo/logo.rc")
    qt_add_executable(TransAI ${app_icon_resource_windows}
        ${SOURCES} ${HEADERS} ${RESOURCES}
    )

    # DPAPI-backed secret storage (CryptProtectData → crypt32).
    target_sources(TransAI PRIVATE src/settings/SecretStoreWindows.cpp)
    target_link_libraries(TransAI PRIVATE crypt32)
    # OCR backend: tesseract CLI when the user has it installed.
    target_sources(TransAI PRIVATE src/ocr/TesseractOcrEngine.cpp)

    set_target_properties(TransAI PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Darwin")
	MESSAGE(STATUS "current platform: Darwin")
    enable_language(OBJCXX)
    find_library( APP_SERVICES_LIBRARY ApplicationServices )
    find_library( SECURITY_LIBRARY Security )
    find_library( VISION_LIBRARY Vision )

    set(MACOSX_BUNDLE_ICON_FILE logo.icns)
    # And the following tells CMake where to find and install the file itself.
    set(app_icon_macos "${CMAKE_CURRENT_SOURCE_DIR}/res/logo/logo.icns")
    set_source_files_properties(${app_icon_macos} PROPERTIES
           MACOSX_PACKAGE_LOCATION "Resources")

    qt_add_executable(TransAI MACOSX_BUNDLE ${app_icon_macos}
        ${SOURCES} ${HEADERS} ${RESOURCES}
    )

    # Keychain Services backend for the API key.
    target_sources(TransAI PRIVATE src/settings/SecretStoreMac.mm)
    # Vision framework OCR backend.
    target_sources(TransAI PRIVATE src/ocr/VisionOcrEngine.mm)

    set_target_properties(TransAI PROPERTIES
        MACOSX_BUNDLE TRUE
    )

ENDIF()

# Common bundle metadata shared by all platforms.
set_target_properties(TransAI PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER tinyx.me
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

target_include_directories(TransAI PRIVATE "src")


IF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(TransAI
        PRIVATE
        Qt6::Quick
        Qt6::Network

        qhotkey
        user32
        )
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    pkg_check_modules(XTST REQUIRED xtst)
    # Wayland screenshot capture goes through xdg-desktop-portal over D-Bus.
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    target_include_directories(TransAI PRIVATE ${X11_INCLUDE_DIR} ${XTST_INCLUDE_DIRS})

    target_link_libraries(TransAI
        PRIVATE
        Qt6::Quick
        Qt6::Network
        Qt6::DBus

        qhotkey
        ${X11_LIBRARIES}
        ${XTST_LIBRARIES}
        )

ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    target_link_libraries(TransAI
        PRIVATE
        Qt6::Quick
        Qt6::Network

        qhotkey
        ${APP_SERVICES_LIBRARY}
        ${SECURITY_LIBRARY}
        ${VISION_LIBRARY}
        )
ENDIF()

install(TARGETS TransAI
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Optional unit tests (pure-logic modules); skipped silently when Qt6::Test
# is not installed. Run with: ctest --test-dir build
find_package(Qt6 QUIET COMPONENTS Test)
if (TARGET Qt6::Test)
    enable_testing()
    add_subdirectory(tests)
endif()
