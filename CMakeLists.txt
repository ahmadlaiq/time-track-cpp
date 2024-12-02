cmake_minimum_required(VERSION 3.20)
project(TimeTrackerApp)

# Update the path to your Qt installation
set(CMAKE_PREFIX_PATH "C:/Qt/6.8.0/mingw_64/lib/cmake/Qt6")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets Network REQUIRED)

qt_add_executable(TimeTrackerApp
    src/main.cpp
    src/TimeTrackerApp.cpp
    src/TimeTrackerApp.h
)

target_link_libraries(TimeTrackerApp PRIVATE Qt6::Widgets Qt6::Network)
