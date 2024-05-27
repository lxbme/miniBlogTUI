cmake_minimum_required(VERSION 3.10)
project(miniBlogTUI)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)



# 查找包
find_package(Curses REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
        GIT_TAG 3b15fa82ea74739b574d705fea44959b58142eb8) # Replace with your desired git commit from: https://github.com/libcpr/cpr/releases
FetchContent_MakeAvailable(cpr)


# 包含头文件路径
include_directories(${CURSES_INCLUDE_DIR})

# 添加可执行文件
add_executable(miniBlogTUI src/main.cpp
)

# 链接库
target_link_libraries(miniBlogTUI PRIVATE nlohmann_json nlohmann_json::nlohmann_json)
target_link_libraries(miniBlogTUI PRIVATE cpr::cpr)
target_link_libraries(miniBlogTUI PRIVATE ${CURSES_LIBRARIES})
