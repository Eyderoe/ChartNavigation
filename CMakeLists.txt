cmake_minimum_required(VERSION 4.0)
project(ChartNavigation)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "D:/Qt/Qt6/6.9.3/mingw_64")
set(QT_FORCE_CMP0156_TO_VALUE NEW)

find_package(Boost)
find_package(Qt6 REQUIRED COMPONENTS
        Core
        Gui
        Widgets
        PdfWidgets
        Pdf)

qt_add_executable(ChartNavigation
        src/main.cpp
        src/gui/main_widget.cpp
        src/gui/main_widget.hpp
        src/gui/main_widget.ui
        src/utils/affineTransformer.cpp
        src/utils/affineTransformer.hpp
        src/gui/pdfView.cpp
        src/gui/pdfView.hpp
        src/gui/license.cpp
        src/gui/license.hpp
        src/gui/license.ui
        resources.qrc
        src/gui/themeColor.cpp
        src/gui/themeColor.hpp
)

set(CMAKE_PCH_INSTANTIATE_TEMPLATES ON)
target_precompile_headers(ChartNavigation PRIVATE
        <QtCore>
        <QtGui>
        <QtWidgets>
        <QtPdf/QtPdf>
        <QtPdfWidgets/QPdfView>
)

target_link_libraries(ChartNavigation PRIVATE
        ${Boost_LIBRARIES} ws2_32
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::PdfWidgets
        Qt::Pdf
)
target_include_directories(ChartNavigation PRIVATE
        ${Boost_INCLUDE_DIRS}
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
