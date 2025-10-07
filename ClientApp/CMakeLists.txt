cmake_minimum_required(VERSION 3.5)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_CREATOR_SKIP_VCPKG_SETUP ON)#мб убрать

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

# ClientApp
set(ClientAppSources
    client.h
    client.cpp
    main.cpp)

add_executable(ClientApp ${ClientAppSources})
target_link_libraries(ClientApp PRIVATE
    Qt${QT_VERSION_MAJOR}::Network
)
