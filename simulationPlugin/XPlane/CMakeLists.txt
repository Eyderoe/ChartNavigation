cmake_minimum_required(VERSION 4.0)
project(X-Plugin)

set(CMAKE_CXX_STANDARD 20)
if (APPLE) # mac-intel暂时不可用 后期考虑Github Action
    add_definitions(-DAPL=1 -DIBM=0 -DLIN=0)
    set(CMAKE_OSX_ARCHITECTURES "arm64") # Intel和Silicon (x86_64;arm64)
    set(SDK_ROOT "/Users/eyderoe/CppLib/SDK")
elseif (WIN32)
    add_definitions(-DAPL=0 -DIBM=1 -DLIN=0)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    set(SDK_ROOT "E:\\cxx_project\\_lib\\SDK")
endif ()

# boost
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
# protobuf
find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})
# xplane-sdk
include_directories(
        ${SDK_ROOT}/CHeaders/XPLM
        ${SDK_ROOT}/CHeaders/Widgets
)

if (APPLE)
    add_definitions(-DAPL=1 -DIBM=0 -DLIN=0)
elseif (WIN32)
    add_definitions(-DAPL=0 -DIBM=1 -DLIN=0)
endif ()
add_definitions(
        -DXPLM303 # 11.5
        -DXPLM301
        -DXPLM300
        -DXPLM210 # 10.2
        -DXPLM200
)

set(PROTO_FILES plane.proto)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# 插件-------------------
add_library(ChartNaviPlugin SHARED
        "main.cpp"
        "XPlane.cpp"
        "XPlane.hpp"
        ${PROTO_SRCS} ${PROTO_HDRS}
)
set_target_properties(ChartNaviPlugin PROPERTIES PREFIX "" SUFFIX ".xpl")
target_link_libraries(ChartNaviPlugin ${Boost_LIBRARIES} ${Protobuf_LIBRARIES})
if (WIN32)
    target_link_libraries(ChartNaviPlugin ws2_32)
    target_link_libraries(ChartNaviPlugin "${SDK_ROOT}/Libraries/Win/XPLM_64.lib")
elseif (APPLE)
    target_link_libraries(ChartNaviPlugin "-undefined dynamic_lookup")
endif ()
target_include_directories(ChartNaviPlugin PRIVATE ${CMAKE_CURRENT_BINARY_DIR})  # protobuf
# UDP测试-------------------
add_executable(udpTest
        test/udpTest.cpp
        XPlane.hpp
        XPlane.cpp
        ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(udpTest ${Boost_LIBRARIES} ${Protobuf_LIBRARIES})
if (WIN32)
    target_link_libraries(udpTest ws2_32)
endif ()
# 函数测试-------------------
add_executable(funcTest
        test/funcTest.cpp
        func.hpp)