cmake_minimum_required(VERSION 3.16)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network)

set(ClientAppSources
    main.cpp
    client.cpp
    client.h
)

add_executable(ClientApp ${ClientAppSources})

target_link_libraries(ClientApp PRIVATE
    Qt6::Core
    Qt6::Network
)
