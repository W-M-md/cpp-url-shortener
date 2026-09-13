cmake_minimum_required(VERSION 3.10)
project(urlshort)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(MySQL REQUIRED)

add_executable(main main.cpp)

target_link_libraries(main ${MYSQL_LIBRARIES})

target_include_directories(main PRIVATE ${MYSQL_INCLUDE_DIRS})