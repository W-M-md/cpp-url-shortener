cmake_minimum_required(VERSION 3.10)

# 项目名和版本
project(cpp-url-shortener VERSION 1.0)

# 使用 C++11 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 如果编译类型没指定，默认用 Release（性能更好）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 打印编译类型，方便调试
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# 找依赖库
find_package(Threads REQUIRED)      # pthread

# 定义可执行文件：名字叫 main，源文件是 main.cpp 和 threadpool.cpp
add_executable(main
    main.cpp
    threadpool.cpp
)

# 链接依赖
target_link_libraries(main
    Threads::Threads        # pthread
    mysqlclient             # MySQL C API
    spdlog                  # 日志库
    fmt                     # spdlog 的依赖
)