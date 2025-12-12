#include <iostream>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <vector>
#include "DBusAdapter.h"
#include "TestService.h"
#include "SafeData.h"
#include "FileReceiver.h"
#include "FileTransfer.h"

DBusAdapter* g_dbus_adapter = nullptr;
TestService* g_test_service = nullptr;

// 信号处理
void signalHandler(int sig) {
    if (sig == SIGINT) {
        std::cout << "\n[Server] 接收到退出信号，正在清理资源..." << std::endl;
        if (g_dbus_adapter) {
            delete g_dbus_adapter;
            g_dbus_adapter = nullptr;
        }
        if (g_test_service) {
            delete g_test_service;
            g_test_service = nullptr;
        }
        cleanup_file_receiver();
        std::cout << "[Server] FileReceiver资源已清理" << std::endl;
        std::cout << "[Server] 资源清理完成，退出成功" << std::endl;
        exit(0);
    }
}

// 定期检查传输状态的线程函数
void check_transfer_status_thread() {
    // 从客户端代码获取的硬编码参数
    const std::string transferId = "video_transfer_001";
    const std::string userId = "test_user";
    const std::string fileName = "testvideo.mp4";
    
    while (true) {
        // 每1秒检查一次
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        std::cout << "[Server] 正在检查传输状态..." << std::endl;
        
        // 直接调用get_missing_chunks函数获取缺失块
        std::vector<int> missing_chunks = get_missing_chunks(transferId, userId, fileName);
        
        // 获取传输状态
        TransferStatus status = get_transfer_status(transferId, userId, fileName);
        
        std::cout << "  传输ID: " << transferId << std::endl;
        std::cout << "  用户ID: " << userId << std::endl;
        std::cout << "  文件名: " << fileName << std::endl;
        std::cout << "  总块数: " << status.totalChunks << std::endl;
        std::cout << "  已接收块数: " << status.receivedChunks << std::endl;
        std::cout << "  缺失块数: " << missing_chunks.size() << std::endl;
        
        if (!missing_chunks.empty()) {
            std::cout << "  缺失的块索引: ";
            for (int chunk : missing_chunks) {
                std::cout << chunk << " ";
            }
            std::cout << std::endl;
        } else {
            std::cout << "  没有缺失的块" << std::endl;
        }
        
        // 打印bitmap信息（前10个块）
        std::cout << "  块状态bitmap（前10个）: ";
        for (int i = 0; i < 10 && i < status.totalChunks; ++i) {
            std::cout << (status.chunkBitmap[i] ? "1" : "0");
        }
        if (status.totalChunks > 10) {
            std::cout << "...";
        }
        std::cout << std::endl;
        std::cout << "[Server] 传输状态检查完成\n" << std::endl;
    }
}

// 程序入口
int main() {
    // 1. 注册信号处理
    signal(SIGINT, signalHandler);

    std::cout << "[Server] 启动中..." << std::endl;

    // 2. 初始化线程安全数据存储
    SafeData::getInstance();

    // 3. 初始化TestService - 先传递nullptr，稍后设置DBusAdapter
    g_test_service = new TestService(nullptr);
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

    // 5. 初始化DBus适配器
    g_dbus_adapter = new DBusAdapter(g_test_service);
    if (!g_dbus_adapter || !g_dbus_adapter->init()) {
        std::cerr << "[Server] DBus适配器初始化失败！" << std::endl;
        delete g_test_service;
        return -1;
    }
    // 设置TestService的DBusAdapter指针
    g_test_service->setDBusAdapter(g_dbus_adapter);

    // 6. 启动传输状态检查线程
    std::thread status_check_thread(check_transfer_status_thread);
    status_check_thread.detach(); // 分离线程，让它在后台运行

    std::cout << "[Server] 启动成功！等待客户端连接..." << std::endl;

    // 7. 启动DBus事件循环
    g_dbus_adapter->runLoop();

    // 8. 释放资源
    delete g_dbus_adapter;
    delete g_test_service;

    return 0;
}