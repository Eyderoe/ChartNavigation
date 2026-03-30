cmake_minimum_required(VERSION 4.0)
project(ChartNavigation)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (WIN32)
    set(CMAKE_PREFIX_PATH "D:/Qt/Qt6/6.9.3/mingw_64")
elseif (APPLE)
    set(CMAKE_PREFIX_PATH "/Users/eyderoe/Qt/6.9.3/macos/lib/cmake")
endif ()

find_package(Boost)
find_package(QCoro6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS
        Core
        Gui
        Widgets
        PdfWidgets
        Pdf
        Concurrent
        Positioning)

find_package(Protobuf REQUIRED)
if (APPLE)
    find_package(absl REQUIRED)
endif ()
include_directories(${Protobuf_INCLUDE_DIRS})
set(PROTO_FILES src/connector/plane.proto)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_executable(${PROJECT_NAME} WIN32
        resources.qrc
        resources/qdarkstyle/dark/darkstyle.qrc
        resources/qdarkstyle/light/lightstyle.qrc
        src/main.cpp
        src/gui/main_widget.cpp
        src/gui/main_widget.hpp
        src/gui/main_widget.ui
        src/utils/affineTransformer.cpp
        src/utils/affineTransformer.hpp
        src/ui/pdfView.cpp
        src/ui/pdfView.hpp
        src/ui/themeColor.cpp
        src/ui/themeColor.hpp
        src/utils/stringProcess.cpp
        src/utils/stringProcess.hpp
        src/gui/options_widget.cpp
        src/gui/options_widget.hpp
        src/gui/options_widget.ui
        src/ui/enhancedTree.cpp
        src/ui/enhancedTree.hpp
        src/utils/randomGen.cpp
        src/utils/randomGen.hpp
        src/utils/constValue.hpp
        src/utils/geographic.cpp
        src/utils/geographic.hpp
        src/connector/interface.hpp
        src/connector/xpStd.cpp
        src/connector/xpStd.hpp
        src/connector/wlan.cpp
        src/connector/wlan.hpp
        src/connector/msfs.cpp
        src/connector/msfs.hpp
        src/connector/allAdapter.hpp
        src/gui/main_window.hpp
        src/gui/main_window.cpp
        src/gui/main_window.ui
        ${PROTO_SRCS} ${PROTO_HDRS}
        src/utils/settingManage.cpp
        src/utils/settingManage.hpp
        src/connector/real.cpp
        src/connector/real.hpp
)

target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:-mconsole>
        $<$<CONFIG:RelWithDebInfo>:-mconsole>
        $<$<CONFIG:Release>:-mwindows>
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT>
)

set(CMAKE_PCH_INSTANTIATE_TEMPLATES ON)
target_precompile_headers(${PROJECT_NAME} PRIVATE
        <QtCore>
        <QtGui>
        <QtWidgets>
        <QtPdf/QtPdf>
        <QtPdfWidgets/QPdfView>
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        ${Boost_SYSTEM_LIBRARY}
        ${Protobuf_LIBRARIES}
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::PdfWidgets
        Qt::Pdf
        Qt::Concurrent
        Qt::Positioning
        QCoro6::Core
)
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
elseif (APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE absl::log_internal_message absl::log_internal_check_op)
endif ()

target_include_directories(${PROJECT_NAME} PRIVATE
        ${Boost_INCLUDE_DIRS}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_autogen/include
)

if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(DEBUG_SUFFIX)
    if (MSVC AND CMAKE_BUILD_TYPE MATCHES "Debug")
        set(DEBUG_SUFFIX "d")
    endif ()
    set(QT_INSTALL_PATH "${CMAKE_PREFIX_PATH}")
    if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
        set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
            set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        endif ()
    endif ()
    if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
    endif ()
    foreach (QT_LIB Core Gui Widgets)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    endforeach (QT_LIB)
endif ()
