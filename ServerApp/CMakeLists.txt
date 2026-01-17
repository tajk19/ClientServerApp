cmake_minimum_required(VERSION 3.16)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

set(ServerAppSources
    main.cpp
    serverwindow.cpp
    serverwindow.h
    serverwindow.ui
    tcpserver.cpp
    tcpserver.h
)

add_executable(ServerApp ${ServerAppSources})

target_link_libraries(ServerApp PRIVATE
    Qt6::Widgets
    Qt6::Network
)
