cmake_minimum_required(VERSION 3.16)

project(TransAI VERSION 1.0.0 LANGUAGES CXX)


set(CMAKE_AUTOMOC ON) # Meta-Object Compiler
set(CMAKE_AUTORCC ON) # Resource Compiler
set(CMAKE_AUTOUIC ON) # User Interface Compiler

set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED APP_VERSION OR APP_VERSION STREQUAL "")
    set(APP_VERSION "${PROJECT_VERSION}")
endif()
add_compile_definitions(APP_VERSION="${APP_VERSION}")

message(STATUS "APP_VERSION: ${APP_VERSION}")

# Windows VERSIONINFO needs four integers, and APP_VERSION may be a release tag
# such as "v1.0.0-rc1": drop the leading "v" and any suffix, fall back to the
# project version when that leaves nothing usable.
string(REGEX REPLACE "^v" "" TRANSAI_RC_VERSION "${APP_VERSION}")
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.?([0-9]*)" _ "${TRANSAI_RC_VERSION}")
if(CMAKE_MATCH_1)
    set(TRANSAI_RC_MAJOR "${CMAKE_MATCH_1}")
    set(TRANSAI_RC_MINOR "${CMAKE_MATCH_2}")
    set(TRANSAI_RC_PATCH "${CMAKE_MATCH_3}")
    if(TRANSAI_RC_PATCH STREQUAL "")
        set(TRANSAI_RC_PATCH "0")
    endif()
else()
    set(TRANSAI_RC_MAJOR "${PROJECT_VERSION_MAJOR}")
    set(TRANSAI_RC_MINOR "${PROJECT_VERSION_MINOR}")
    set(TRANSAI_RC_PATCH "${PROJECT_VERSION_PATCH}")
endif()

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
    src/app/ScreenshotPreviewProvider.h
    src/app/WindowDismissHelper.h
    src/settings/Setting.h
    src/settings/SecretStore.h
    src/ocr/OcrEngine.h
    src/ocr/OcrModelLocator.h
    src/ocr/OcrPostProcess.h
    src/translation/PromptBuilder.h
    src/translation/StreamParser.h
    src/translation/StreamingChatClient.h
    src/translation/TranslationService.h
    src/providers/ProviderDefaults.h
    src/providers/ApiError.h
    src/providers/ProviderEndpointBuilder.h
    src/providers/NetworkErrorMapper.h
    src/providers/ModelDiscoveryService.h
    src/providers/ThinkingPolicy.h
    src/input/GlobalShortcutManager.h
    src/input/GlobalClickMonitor.h
    src/input/ClipboardGuard.h
    src/input/SelectionTextProvider.h
    src/capture/ScreenshotGeometry.h
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
    src/app/ScreenshotPreviewProvider.cpp
    src/app/WindowDismissHelper.cpp
    src/settings/Setting.cpp
    src/ocr/OcrEngineFactory.cpp
    src/translation/PromptBuilder.cpp
    src/translation/StreamParser.cpp
    src/translation/StreamingChatClient.cpp
    src/translation/TranslationService.cpp
    src/providers/ProviderEndpointBuilder.cpp
    src/providers/NetworkErrorMapper.cpp
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

# QHotkey is vendored and linked statically: its own install rules (headers plus
# a CMake package config) would only add noise to the release tarball.
set(QHOTKEY_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/)

include(GNUInstallDirs)

# ---------------------------------------------------------------------------
# OCR backend selection
#
# Linux / Windows default to the **bundled** RapidOCR (PP-OCR on ncnn) backend:
# the models ship with the app, recognition runs in-process, and the user does
# not have to install anything. Building it pulls ncnn in through FetchContent
# (a CPU-only, Vulkan-less static build) unless a system ncnn is found or
# -DTRANSAI_BUNDLED_OCR=OFF is passed, which falls back to the old tesseract
# CLI backend. macOS keeps the Vision framework (zero size, system-provided).
# ---------------------------------------------------------------------------
option(TRANSAI_BUNDLED_OCR "Build the self-contained RapidOCR (ncnn) OCR backend" ON)
option(TRANSAI_OCR_FETCH_NCNN "Fetch and build ncnn when no system package is found" ON)
# ncnn duplicates its CPU kernels per instruction-set variant, and those
# variants dominate the linked size. Measured on x86-64 (PP-OCRv3 det+rec,
# 900x300 screenshot), the profiles cost roughly:
#   portable  ~6 MB added, works on any x86-64 (runtime SSE2/AVX dispatch)
#   full      ~17 MB added, ~1.5x faster (AVX2/AVX-512/FMA variants)
#   avx-only  ~3 MB added, same speed as portable, requires an AVX CPU (2011+)
# Screenshot OCR runs on a worker thread behind a network request, so latency is
# not critical: default to the portable profile.
set(TRANSAI_OCR_NCNN_ISA "portable" CACHE STRING
    "ncnn x86 SIMD profile for the OCR backend: portable | full | avx-only")
set_property(CACHE TRANSAI_OCR_NCNN_ISA PROPERTY STRINGS portable full avx-only)
set(TRANSAI_NCNN_GIT_TAG "20260526" CACHE STRING "ncnn git tag fetched for the OCR backend")
set(TRANSAI_OCR_MODEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res/ocr/models" CACHE PATH
    "Directory holding the bundled PP-OCR model files (scripts/fetch_ocr_models.sh)")

# Adds the bundled OCR backend to `target` when it can be built, and otherwise
# falls back to the tesseract CLI backend. Called from the platform blocks below.
function(transai_setup_ocr_backend target)
    set(use_ncnn OFF)

    if(TRANSAI_BUNDLED_OCR)
        find_package(ncnn QUIET)
        if(NOT TARGET ncnn AND TRANSAI_OCR_FETCH_NCNN)
            include(FetchContent)
            # Only two small PP-OCR models are ever executed: no Vulkan, no
            # tools, no OpenMP, no C API.
            set(NCNN_VULKAN OFF CACHE BOOL "" FORCE)
            set(NCNN_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
            set(NCNN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
            set(NCNN_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
            set(NCNN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
            set(NCNN_INSTALL_SDK OFF CACHE BOOL "" FORCE)
            set(NCNN_OPENMP OFF CACHE BOOL "" FORCE)
            set(NCNN_C_API OFF CACHE BOOL "" FORCE)
            if(TRANSAI_OCR_NCNN_ISA STREQUAL "portable")
                set(NCNN_AVX2 OFF CACHE BOOL "" FORCE)
                set(NCNN_AVXVNNI OFF CACHE BOOL "" FORCE)
                set(NCNN_AVX512 OFF CACHE BOOL "" FORCE)
                set(NCNN_FMA OFF CACHE BOOL "" FORCE)
                set(NCNN_FMA4 OFF CACHE BOOL "" FORCE)
                set(NCNN_F16C OFF CACHE BOOL "" FORCE)
                set(NCNN_XOP OFF CACHE BOOL "" FORCE)
            elseif(TRANSAI_OCR_NCNN_ISA STREQUAL "avx-only")
                set(NCNN_RUNTIME_CPU OFF CACHE BOOL "" FORCE)
                set(NCNN_AVX2 OFF CACHE BOOL "" FORCE)
                set(NCNN_AVXVNNI OFF CACHE BOOL "" FORCE)
                set(NCNN_AVX512 OFF CACHE BOOL "" FORCE)
                set(NCNN_FMA OFF CACHE BOOL "" FORCE)
                set(NCNN_FMA4 OFF CACHE BOOL "" FORCE)
                set(NCNN_F16C OFF CACHE BOOL "" FORCE)
                set(NCNN_XOP OFF CACHE BOOL "" FORCE)
            endif()
            set(NCNN_PIXEL_DRAWING OFF CACHE BOOL "" FORCE)
            set(NCNN_PIXEL_ROTATE OFF CACHE BOOL "" FORCE)
            set(NCNN_PIXEL_AFFINE OFF CACHE BOOL "" FORCE)
            message(STATUS "OCR backend: fetching ncnn ${TRANSAI_NCNN_GIT_TAG}")
            FetchContent_Declare(ncnn
                GIT_REPOSITORY https://github.com/Tencent/ncnn.git
                GIT_TAG ${TRANSAI_NCNN_GIT_TAG}
                GIT_SHALLOW TRUE)
            FetchContent_MakeAvailable(ncnn)
        endif()
        if(TARGET ncnn)
            set(use_ncnn ON)
        else()
            message(WARNING
                "OCR backend: ncnn unavailable, falling back to the tesseract CLI backend")
        endif()
    endif()

    if(use_ncnn)
        set(TRANSAI_OCR_BACKEND_ACTIVE "ncnn" PARENT_SCOPE)
        find_package(Qt6 REQUIRED COMPONENTS Concurrent)
        target_sources(${target} PRIVATE
            src/ocr/RapidOcrEngine.cpp
            src/ocr/OcrPostProcess.cpp
            src/ocr/OcrModelLocator.cpp)
        target_compile_definitions(${target} PRIVATE TRANSAI_OCR_BACKEND_NCNN)
        target_link_libraries(${target} PRIVATE ncnn Qt6::Concurrent)

        if(EXISTS "${TRANSAI_OCR_MODEL_DIR}/ch_PP-OCRv3_det_infer.param"
           AND EXISTS "${TRANSAI_OCR_MODEL_DIR}/ch_PP-OCRv3_rec_infer.param"
           AND EXISTS "${TRANSAI_OCR_MODEL_DIR}/ppocr_keys_v1.txt")
            # In-tree builds (and the tests) find the models straight from the
            # source tree; packaged builds use the install rule below.
            target_compile_definitions(${target} PRIVATE
                TRANSAI_OCR_SOURCE_MODEL_DIR="${TRANSAI_OCR_MODEL_DIR}")
            install(DIRECTORY "${TRANSAI_OCR_MODEL_DIR}/"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/TransAI/ocr/models")
            set(TRANSAI_OCR_MODELS_PRESENT ON PARENT_SCOPE)
            message(STATUS "OCR backend: bundled RapidOCR + ncnn (models: ${TRANSAI_OCR_MODEL_DIR})")
        else()
            message(WARNING
                "OCR backend: bundled RapidOCR is enabled but the models are missing from "
                "${TRANSAI_OCR_MODEL_DIR}; run scripts/fetch_ocr_models.sh (OCR will report "
                "itself unavailable until the models exist)")
        endif()
    else()
        set(TRANSAI_OCR_BACKEND_ACTIVE "tesseract" PARENT_SCOPE)
        message(STATUS "OCR backend: tesseract CLI")
        target_sources(${target} PRIVATE src/ocr/TesseractOcrEngine.cpp)
        target_compile_definitions(${target} PRIVATE TRANSAI_OCR_BACKEND_TESSERACT)
    endif()
endfunction()


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

    # OCR backend: bundled RapidOCR/ncnn when it can be built, otherwise the
    # tesseract CLI.
    transai_setup_ocr_backend(TransAI)
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
	MESSAGE(STATUS "current platform: Windows")

    # set(CMAKE_EXE_LINKER_FLAGS "-static") use target_link_libraries instead
    set(app_icon_resource_windows "${CMAKE_CURRENT_SOURCE_DIR}/res/logo/logo.rc")
    # VERSIONINFO for the exe (Explorer → properties). Generated because a .rc
    # file needs literal version numbers.
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/res/logo/version.rc.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)
    qt_add_executable(TransAI ${app_icon_resource_windows}
        "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
        ${SOURCES} ${HEADERS} ${RESOURCES}
    )

    # DPAPI-backed secret storage (CryptProtectData → crypt32).
    target_sources(TransAI PRIVATE src/settings/SecretStoreWindows.cpp)
    target_link_libraries(TransAI PRIVATE crypt32)
    # OCR backend: bundled RapidOCR/ncnn when it can be built, otherwise the
    # tesseract CLI.
    transai_setup_ocr_backend(TransAI)

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
    # Vision framework OCR backend (macOS always has one, no bundle cost).
    target_sources(TransAI PRIVATE src/ocr/VisionOcrEngine.mm)
    target_compile_definitions(TransAI PRIVATE TRANSAI_OCR_BACKEND_VISION)

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

# Third-party license texts next to the models (see LICENSES/README.md and
# THIRD-PARTY-NOTICES.md). Vendored in-tree so a release cannot ship without
# them, and so packaging never needs the network.
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/TransAI/licenses")

# Optional unit tests (pure-logic modules); skipped silently when Qt6::Test
# is not installed. Run with: ctest --test-dir build
find_package(Qt6 QUIET COMPONENTS Test)
if (TARGET Qt6::Test)
    enable_testing()
    add_subdirectory(tests)
endif()
