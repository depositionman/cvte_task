#include <iostream>
#include <unistd.h>
#include "DBusAdapter.h"
#include "TestService.h"
#include "SafeData.h"
#include "FileReceiver.h"

// 全局变量：DBus适配器实例（确保生命周期与程序一致）
DBusAdapter* g_dbus_adapter = nullptr;
// 全局变量：TestService实例（核心接口实现）
TestService* g_test_service = nullptr;

// 信号处理：捕获Ctrl+C，优雅退出
void signalHandler(int sig) {
    if (sig == SIGINT) {
        std::cout << "\n[Server] 接收到退出信号，正在清理资源..." << std::endl;
        // 释放资源
        if (g_dbus_adapter) {
            delete g_dbus_adapter;
            g_dbus_adapter = nullptr;
        }
        if (g_test_service) {
            delete g_test_service;
            g_test_service = nullptr;
        }
        // 清理FileReceiver
        cleanup_file_receiver();
        std::cout << "[Server] FileReceiver资源已清理" << std::endl;
        std::cout << "[Server] 资源清理完成，退出成功" << std::endl;
        exit(0);
    }
}

// 程序入口
int main() {
    // 1. 注册信号处理
    signal(SIGINT, signalHandler);

    std::cout << "[Server] 启动中..." << std::endl;

    // 2. 初始化线程安全数据存储
    SafeData::getInstance();

    // 3. 初始化TestService
    g_test_service = new TestService();
    if (!g_test_service) {
        std::cerr << "[Server] TestService初始化失败！" << std::endl;
        return -1;
    }

    // 4. 初始化FileReceiver
    if (init_file_receiver(4) != 0) {
        std::cerr << "[Server] FileReceiver初始化失败！" << std::endl;
        delete g_test_service;
        g_test_service = nullptr;
        return -1;
    }
    std::cout << "[Server] FileReceiver初始化成功" << std::endl;

    // 4. 初始化DBus适配器
    g_dbus_adapter = new DBusAdapter(g_test_service);
    if (!g_dbus_adapter || !g_dbus_adapter->init()) {
        std::cerr << "[Server] DBus适配器初始化失败！" << std::endl;
        delete g_test_service;
        return -1;
    }

    std::cout << "[Server] 启动成功！等待客户端连接..." << std::endl;

    // 5. 启动DBus事件循环
    g_dbus_adapter->runLoop();

    // 6. 释放资源
    delete g_dbus_adapter;
    delete g_test_service;

    return 0;
}