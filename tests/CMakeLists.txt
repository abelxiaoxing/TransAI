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
