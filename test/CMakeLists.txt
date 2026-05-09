find_package(GeographicLib REQUIRED)

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
        ${GeographicLib_INCLUDE_DIRS}
)

target_link_libraries(unit_tests PRIVATE
        ${GeographicLib_LIBRARIES}
)

add_test(NAME AllTests COMMAND unit_tests)
