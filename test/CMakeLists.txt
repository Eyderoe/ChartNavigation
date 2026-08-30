add_executable(unit_tests
        testMain.cpp
        stringProcess_test.cpp
        ../src/utils/stringProcess.cpp
        geographic_test.cpp
        ../src/utils/geographic.cpp
        mapDataQuery_test.cpp
        mapItemManage_test.cpp
        ../src/services/mapDataQuery.cpp
        ../src/services/mapItemManage.cpp
        ../src/utils/sqliteHelper.cpp
        noaaGlobe_test.cpp
        ../src/utils/noaaGlobe.cpp
        ../src/utils/noaaGlobe.hpp
)

target_include_directories(unit_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_compile_definitions(unit_tests PRIVATE
        CHARTNAVI_TEST_DATABASE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/chartNavi_test.db3"
)

target_precompile_headers(unit_tests PRIVATE
        <QtCore>
)

target_link_libraries(unit_tests PRIVATE
        GeographicLib::GeographicLib
        SQLiteCpp
        Qt::Core
        Qt::Gui
        Qt::Widgets
)

add_test(NAME AllTests COMMAND unit_tests)
