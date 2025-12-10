# 1. 指定CMake最低版本（Ubuntu22.04默认3.16+，满足要求）
cmake_minimum_required(VERSION 3.16)
# 2. 启用pkg-config来查找依赖包
find_package(PkgConfig REQUIRED)
pkg_check_modules(GIO2 REQUIRED gio-2.0)
find_package(nlohmann_json 3.0.0 REQUIRED)
message(STATUS "Found nlohmann_json: ${nlohmann_json_VERSION}")

# 2. 工程名称（服务端工程）
project(ServiceProject)
# 2.5. 查找dbus-glib依赖（为了使用dbus_connection_setup_with_g_main）
pkg_check_modules(DBUS_GLIB REQUIRED dbus-glib-1)
# 3. 设置C++标准（C++17，兼容现代特性且车载环境支持）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # 强制使用指定的C++标准
set(CMAKE_CXX_EXTENSIONS OFF)        # 禁用编译器扩展，保证跨平台
# 4. 区分Debug/Release编译模式（核心需求）
# 默认Debug模式，支持gdb单步调试；Release模式移除符号、优化体积
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (Debug/Release)" FORCE)
endif()
# 5. 编译选项配置
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug：保留调试符号，关闭优化，开启所有警告（便于调试）
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall -Wextra -Wpedantic")
    message(STATUS "Service Build Mode: Debug (gdb调试可用)")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Release：优化性能，移除所有符号，压缩体积（车载部署推荐）
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -Wl,-s")
    # -Wl,-s：通过链接器移除符号表；-O2：二级优化，平衡性能和体积
    message(STATUS "Service Build Mode: Release (无调试符号，体积最小)")
endif()
# 6. 头文件路径（指定Include目录，让编译器找到头文件）
include_directories(
    ${PROJECT_SOURCE_DIR}/Include  # 服务端自身头文件
    ${PROJECT_SOURCE_DIR}/Include/core  # 核心业务逻辑头文件
    ${PROJECT_SOURCE_DIR}/Include/communication  # 通信模块头文件
    ${PROJECT_SOURCE_DIR}/Include/filetransfer  # 文件传输模块头文件
    ${PROJECT_SOURCE_DIR}/../common/Include  # 公共头文件（TestData.h等）
    ${DBUS_INCLUDE_DIRS}           # DBus头文件路径（通过pkg-config自动获取）
    ${DBUS_GLIB_INCLUDE_DIRS}      # DBus-GLib头文件路径（通过pkg-config自动获取）
    ${GLIB_INCLUDE_DIRS}           # GLib头文件路径（通过pkg-config自动获取）
    # 注意：nlohmann_json的头文件路径无需手动添加！
    # find_package后，链接时会自动将头文件路径注入目标
)
# 7. 定义源文件列表（生成libtraining动态库的源文件）
set(LIB_TRAINING_SOURCES
    Sources/core/TestService.cpp       # ITestService接口实现
    Sources/communication/DBusAdapter.cpp       # gdbus适配层
    Sources/core/SafeData.cpp          # 线程安全数据存储
    Sources/filetransfer/FileReceiver.cpp      
    ../common/Sources/ThreadPool.cpp        # 线程池实现
    ../common/Sources/MemoryPool.cpp        # 内存池实现
)
# 8. 生成动态库libtraining.so（核心需求：服务端动态库）
add_library(training SHARED ${LIB_TRAINING_SOURCES})
# 9. 链接动态库依赖的第三方库
target_link_libraries(training
    # gdbus核心库（必须，通过pkg-config自动获取）
    ${GIO2_LIBRARIES}  # 新增：链接GIO2库
    # GLib库（DBus依赖，通过pkg-config自动获取）
    ${GLIB_LIBRARIES}
    # OpenSSL库（MD5校验）
    ssl
    crypto
    # 线程库（多Client并发/互斥锁需要）
    pthread
    # -------------------------- 新增：链接nlohmann_json库 --------------------------
    nlohmann_json::nlohmann_json  # 系统安装版的链接目标（固定名称）
    # --------------------------------------------------------------------------------
)
target_include_directories(training PRIVATE ${GIO2_INCLUDE_DIRS})
target_compile_options(training PRIVATE ${GIO2_CFLAGS_OTHER})
# 10. 定义server可执行文件的源文件
set(SERVER_EXEC_SOURCES
    Sources/main/ServerMain.cpp        # 服务端入口函数
)
# 11. 生成server可执行文件
add_executable(server ${SERVER_EXEC_SOURCES})
# 12. 链接server可执行文件依赖的库（包含自身的libtraining）
target_link_libraries(server
    training  # 链接本地生成的libtraining动态库
    ${DBUS_LIBRARIES}  # DBus库（通过pkg-config自动获取）
    ${DBUS_GLIB_LIBRARIES}  # DBus-GLib库
    ${GLIB_LIBRARIES}  # GLib库（通过pkg-config自动获取）
    pthread   # 线程依赖
    # -------------------------- 可选：server直接用json时添加 --------------------------
    # 若server自身代码未直接使用json，仅通过libtraining间接依赖，可省略此行
    # 但为了兼容性和清晰性，建议保留
    nlohmann_json::nlohmann_json
    # --------------------------------------------------------------------------------
)
# 13. 可选：安装规则（便于部署到车载系统）
# 安装libtraining.so到系统库目录，server到可执行目录
install(TARGETS training
    LIBRARY DESTINATION /usr/lib  # 动态库安装路径
)
install(TARGETS server
    RUNTIME DESTINATION /usr/bin  # 可执行文件安装路径
)

# 14. 安装头文件到系统头文件目录
install(DIRECTORY ${PROJECT_SOURCE_DIR}/Include/
    DESTINATION /usr/include/libtraining  # 头文件安装路径
    FILES_MATCHING PATTERN "*.h"  # 只安装.h文件
)

# 15. 安装公共头文件
install(DIRECTORY ${PROJECT_SOURCE_DIR}/../common/Include/
    DESTINATION /usr/include/libtraining/common  # 公共头文件安装路径
    FILES_MATCHING PATTERN "*.h"  # 只安装.h文件
)
