find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

set(THEME_TEST_SOURCES
    test_thememanager.cpp
)

add_executable(test_thememanager ${THEME_TEST_SOURCES})

target_link_libraries(test_thememanager
    Qt6::Test
    Qt6::Widgets
    TransAI
)

add_test(NAME TestThemeManager COMMAND test_thememanager)