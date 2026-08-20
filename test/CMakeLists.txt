add_executable(unit_tests
        testMain.cpp
        stringProcess_test.cpp
        noaaGlobe_test.cpp
        ../src/utils/noaaGlobe.cpp
        ../src/utils/noaaGlobe.hpp
)

target_include_directories(unit_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(unit_tests PRIVATE
        GeographicLib::GeographicLib
        Qt::Core
)

add_test(NAME AllTests COMMAND unit_tests)