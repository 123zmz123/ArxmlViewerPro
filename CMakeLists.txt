cmake_minimum_required(VERSION 3.16)
project(ArxmlViewerPro LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Sql Xml)

qt_add_executable(ArxmlViewerPro src/main.cpp src/arxmlparser.cpp src/arxmldatabase.cpp src/arxmlutils.cpp)
target_link_libraries(ArxmlViewerPro PRIVATE Qt6::Core Qt6::Sql Qt6::Xml)
