# One test executable; every tests/unit/*_test.cpp is picked up automatically. Shared helpers live in tests/support.
file(GLOB TEST_SRC CONFIGURE_DEPENDS unit/*_test.cpp)
add_executable(chiasmos_tests ${TEST_SRC})
target_include_directories(chiasmos_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chiasmos_tests PRIVATE chiasmos_build chiasmos_output chiasmos_query chiasmos_index chiasmos_normalize chiasmos_core
                                           chiasmos_osmium GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(chiasmos_tests DISCOVERY_MODE PRE_TEST)
