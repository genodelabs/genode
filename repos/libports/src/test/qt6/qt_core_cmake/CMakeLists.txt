cmake_minimum_required(VERSION 3.10.0)

project(test-qt_core_cmake LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core REQUIRED)

add_executable(test-qt_core_cmake
    main.cpp
)

target_link_libraries(test-qt_core_cmake Qt6::Core)
