cmake_minimum_required(VERSION 3.16)
project(ArxmlViewerPro LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Sql Xml Widgets)

qt_add_executable(ArxmlViewerPro
    src/main.cpp
    src/ui/mainwindow.cpp src/ui/mainwindow.h src/ui/mainwindow.ui
    src/arxmlparser.cpp src/arxmldatabase.cpp src/arxmlutils.cpp
    src/resources.qrc
)
target_link_libraries(ArxmlViewerPro PRIVATE Qt6::Core Qt6::Sql Qt6::Xml Qt6::Widgets)
