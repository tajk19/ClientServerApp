cmake_minimum_required(VERSION 3.16)
project(ClientServerApp
    LANGUAGES CXX
    VERSION 0.0.1
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(ClientApp)
add_subdirectory(ServerApp)
