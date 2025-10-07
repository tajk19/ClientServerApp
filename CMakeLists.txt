cmake_minimum_required(VERSION 3.5)
project(ClientServerApp
LANGUAGES
  CXX
VERSION
  0.0.1
)

add_compile_options(-g)

add_subdirectory(ClientApp)
add_subdirectory(ServerApp)


set(APP_VERSION "${PROJECT_VERSION}" CACHE STRING "Test project version")
message(STATUS "Stuk2 version: ${STUK_VERSION}")
target_compile_definitions(ClientApp PRIVATE CONTROLLER_VERSION="${APP_VERSION}")
target_compile_definitions(ServerApp PRIVATE GUI_VERSION="${APP_VERSION}")
