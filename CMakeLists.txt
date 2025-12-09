cmake_minimum_required(VERSION 3.10)

project(MyProject)

set(CMAKE_CXX_STANDARD 17)

include_directories(${PROJECT_SOURCE_DIR}/include)  # 包含你的头文件目录
include_directories(${PROJECT_SOURCE_DIR}/src)     # 包含src目录，以便找到data_loader.hpp

# CSV 库源文件
file(GLOB CSV_SOURCES "include/internal/*.cpp")

# 主程序源文件
add_executable(dig 
    src/main.cpp 
    src/dataload/data_loader.cpp
    src/apriori/apr.cpp
    src/fptree/fp.cpp
    ${CSV_SOURCES}
)
