cmake_minimum_required(VERSION 3.1)

project(tld-tests LANGUAGES C)


# Setup unit-tests
add_executable(unit-test-suite utests/unit_tests.c) 
target_link_libraries(unit-test-suite PUBLIC ${tld})
set_property(TARGET unit-test-suite PROPERTY C_STANDARD 11)

set_target_properties(unit-test-suite PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
)

add_test(
    NAME unit-test
    COMMAND unit-test-suite
)
