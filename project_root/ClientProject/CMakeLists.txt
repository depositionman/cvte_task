# 1. 指定CMake最低版本
cmake_minimum_required(VERSION 3.16)

# 2. 启用pkg-config来查找依赖包
find_package(PkgConfig REQUIRED)
pkg_check_modules(GIO2 REQUIRED gio-2.0)
pkg_check_modules(GLIB REQUIRED glib-2.0)

# 2. 工程名称（客户端工程）
project(ClientProject)

# 3. 设置C++标准（与服务端保持一致）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 4. 区分Debug/Release编译模式
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (Debug/Release)" FORCE)
endif()

# 5. 编译选项配置（与服务端完全一致）
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall -Wextra -Wpedantic")
    message(STATUS "Client Build Mode: Debug (gdb调试可用)")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -Wl,-s")
    message(STATUS "Client Build Mode: Release (无调试符号，体积最小)")
endif()

# 6. 头文件路径（关键：引用Service的Include目录，确保接口兼容）
include_directories(
    ${PROJECT_SOURCE_DIR}/Include                  # 客户端自身头文件
    ${PROJECT_SOURCE_DIR}/Include/communication    # 客户端dbus模块头文件
    ${PROJECT_SOURCE_DIR}/Include/filetransfer     # 客户端filetransfer模块头文件
    ${PROJECT_SOURCE_DIR}/../ServiceProject/Include  # 引用服务端的核心接口头文件（ITestService/TestData等）
    ${PROJECT_SOURCE_DIR}/../common/Include        # 引用公共头文件（TestData.h等）
    ${GIO2_INCLUDE_DIRS}                           # GIO头文件路径（通过pkg-config自动获取）
    ${GLIB_INCLUDE_DIRS}                           # GLib头文件路径（通过pkg-config自动获取）
)

# 7. 定义客户端可执行文件的源文件列表
set(CLIENT_EXEC_SOURCES
    Sources/main/ClientMain.cpp        # 客户端入口函数
    Sources/communication/ClientDBus.cpp        # 客户端gdbus连接/接口调用
    Sources/filetransfer/FileSender.cpp        # 文件读取/分包/共享内存写入
    ../common/Sources/MemoryPool.cpp        # 内存池实现
    ../common/Sources/ThreadPool.cpp        # 线程池实现
)

# 8. 生成client可执行文件
add_executable(client ${CLIENT_EXEC_SOURCES})

# 9. 链接客户端依赖的第三方库（与服务端一致）
target_link_libraries(client
    # gdbus核心库（通过pkg-config自动获取）
    ${GIO2_LIBRARIES}
    ${GIO2_LIBRARIES}
    # GLib库（DBus依赖，通过pkg-config自动获取）
    ${GLIB_LIBRARIES}
    # OpenSSL（MD5校验）
    ssl
    crypto
    # 线程库（共享内存/并发）
    pthread
)

# 10. 可选：安装规则（部署到车载系统）
install(TARGETS client
    RUNTIME DESTINATION /usr/bin
)
