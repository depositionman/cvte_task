#!/bin/bash

# 检查是否传入了项目名
if [ -z "$1" ]; then
    echo "Usage: $0 <project_name>"
    exit 1
fi

PROJECT_NAME="$1"
ROOT_DIR="./${PROJECT_NAME}"

echo "Creating project structure: ${ROOT_DIR}"

# 创建 ServiceProject 目录结构
mkdir -p "${ROOT_DIR}/ServiceProject/Include"
mkdir -p "${ROOT_DIR}/ServiceProject/Sources"

# 创建 ClientProject 目录结构
mkdir -p "${ROOT_DIR}/ClientProject/Include"
mkdir -p "${ROOT_DIR}/ClientProject/Sources"

# ServiceProject 头文件
touch "${ROOT_DIR}/ServiceProject/Include/ITestService.h"
touch "${ROOT_DIR}/ServiceProject/Include/ITestListener.h"
touch "${ROOT_DIR}/ServiceProject/Include/TestData.h"
touch "${ROOT_DIR}/ServiceProject/Include/DBusAdapter.h"
touch "${ROOT_DIR}/ServiceProject/Include/FileTransfer.h"
touch "${ROOT_DIR}/ServiceProject/Include/SafeData.h"

# ServiceProject 源文件
touch "${ROOT_DIR}/ServiceProject/Sources/ServerMain.cpp"
touch "${ROOT_DIR}/ServiceProject/Sources/TestService.cpp"
touch "${ROOT_DIR}/ServiceProject/Sources/DBusAdapter.cpp"
touch "${ROOT_DIR}/ServiceProject/Sources/FileTransfer.cpp"
touch "${ROOT_DIR}/ServiceProject/Sources/SafeData.cpp"

# ServiceProject CMakeLists.txt（占位）
cat > "${ROOT_DIR}/ServiceProject/CMakeLists.txt" <<EOF
# CMakeLists.txt for ServiceProject
cmake_minimum_required(VERSION 3.10)
project(ServiceProject)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GIO REQUIRED gio-2.0)

include_directories(\${CMAKE_CURRENT_SOURCE_DIR}/Include)
include_directories(\${GIO_INCLUDE_DIRS})

file(GLOB SOURCES "Sources/*.cpp")

add_executable(server \${SOURCES})
target_link_libraries(server \${GIO_LIBRARIES})
EOF

# ClientProject 头文件
touch "${ROOT_DIR}/ClientProject/Include/ClientDBus.h"
touch "${ROOT_DIR}/ClientProject/Include/CmdMenu.h"

# ClientProject 源文件
touch "${ROOT_DIR}/ClientProject/Sources/ClientMain.cpp"
touch "${ROOT_DIR}/ClientProject/Sources/ClientService.cpp"
touch "${ROOT_DIR}/ClientProject/Sources/ClientDBus.cpp"
touch "${ROOT_DIR}/ClientProject/Sources/CmdMenu.cpp"
touch "${ROOT_DIR}/ClientProject/Sources/FileSender.cpp"

# ClientProject CMakeLists.txt（占位）
cat > "${ROOT_DIR}/ClientProject/CMakeLists.txt" <<EOF
# CMakeLists.txt for ClientProject
cmake_minimum_required(VERSION 3.10)
project(ClientProject)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GIO REQUIRED gio-2.0)

# 引用 ServiceProject 的 Include（便于共享接口定义）
include_directories(\${CMAKE_CURRENT_SOURCE_DIR}/../ServiceProject/Include)
include_directories(\${GIO_INCLUDE_DIRS})

file(GLOB SOURCES "Sources/*.cpp")

add_executable(client \${SOURCES})
target_link_libraries(client \${GIO_LIBRARIES})
EOF

echo "✅ Project '${PROJECT_NAME}' created successfully!"
echo "📁 Structure:"
tree "${ROOT_DIR}"
