cmake_minimum_required(VERSION 3.5)
project(clientServerApp VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Найти Qt6
find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)

# Общие файлы окон
set(CommonWindows
    clientwindow.cpp
    clientwindow.h
    clientwindow.ui
    serverwindow.cpp
    serverwindow.h
    serverwindow.ui
)

# Создаем статическую библиотеку с окнами
add_library(CommonWindows STATIC ${CommonWindows})
target_link_libraries(CommonWindows PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

# ClientApp
set(ClientAppSources
    main.cpp
)
add_executable(ClientApp ${ClientAppSources})
target_link_libraries(ClientApp PRIVATE CommonWindows Qt${QT_VERSION_MAJOR}::Widgets)

# ServerApp
set(ServerAppSources
    main.cpp
)
add_executable(ServerApp ${ServerAppSources})
target_link_libraries(ServerApp PRIVATE CommonWindows Qt${QT_VERSION_MAJOR}::Widgets)
