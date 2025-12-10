#include <iostream>
#include <csignal>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <glib.h>
#include <cstring>
#include <sys/stat.h>
#include "ClientDBus.h"
#include "TestData.h"
#include "FileSender.h"

ClientDBus client;
std::string videoPath = "/home/wjl/project/project_root/ClientProject/testvideo.mp4";
std::string transferId = "video_transfer_001";
std::string userId = "test_user";

void signalHandler(int sig) {
    if (sig == SIGINT) {
        std::cout << "\n[Client] 退出..." << std::endl;
        exit(0);
    }
}

void basic_test() {
    // 测试基本DBus功能
    std::cout << "\n=== 基本DBus功能测试 ===" << std::endl;
    client.SetTestBool(true);
    std::cout << "GetTestBool: " << client.GetTestBool() << std::endl;

    client.SetTestInt(42);
    std::cout << "GetTestInt: " << client.GetTestInt() << std::endl;

    client.SetTestDouble(3.14);
    std::cout << "GetTestDouble: " << client.GetTestDouble() << std::endl;

    client.SetTestString("hello dbus");
    std::cout << "GetTestString: " << client.GetTestString() << std::endl;

    TestInfo info{true, 123, 4.56, "struct test"};
    client.SetTestInfo(info);
    TestInfo ret = client.GetTestInfo();
    std::cout << "GetTestInfo: bool=" << ret.bool_param
              << ", int=" << ret.int_param
              << ", double=" << ret.double_param
              << ", string=" << ret.string_param << std::endl;
}

void send_file_test() {
    std::cout << "\n=== 发送测试视频文件 ===" << std::endl;
    // 检查文件是否存在
    struct stat fileStat;
    if (stat(videoPath.c_str(), &fileStat) != 0) {
        std::cerr << "文件不存在: " << videoPath << std::endl;
        return;
    }
    
    std::cout << "开始发送文件: " << videoPath << std::endl;
    std::cout << "传输ID: " << transferId << std::endl;
    std::cout << "用户ID: " << userId << std::endl;
    
    send_entry(videoPath.c_str(), userId.c_str(), 0644, transferId.c_str());
}

void gain_transferStatus_missblock() {
    // 获取文件名
    size_t lastSlash = videoPath.find_last_of('/');
    std::string fileName = (lastSlash != std::string::npos) ? videoPath.substr(lastSlash + 1) : videoPath;
    
    // 测试获取传输状态
    std::cout << "\n--- 测试获取传输状态 ---" << std::endl;
    TransferStatus status = client.GetTransferStatus(transferId, userId, fileName);
    std::cout << "传输状态: 总块数=" << status.totalChunks 
              << ", 已接收块数=" << status.receivedChunks
              << ", 文件长度=" << status.fileLength
              << ", 已接收长度=" << status.receivedLength
              << ", 状态码=" << status.statusCode
              << ", 是否完成=" << (status.isCompleted ? "是" : "否") << std::endl;
    
    // 测试获取缺失块列表
    std::cout << "\n--- 测试获取缺失块列表 ---" << std::endl;
    std::vector<int> missingChunks = client.GetMissingChunks(transferId, userId, fileName);
    std::cout << "缺失块总数: " << missingChunks.size() << std::endl;
    
    if (!missingChunks.empty()) {
        std::cout << "缺失块索引: ";
        for (size_t i = 0; i < missingChunks.size(); i++) {
            std::cout << missingChunks[i];
            if (i < missingChunks.size() - 1) {
                std::cout << ", ";
            }
        }
        std::cout << std::endl;
    }
}

void resume_send_file_test() {
    std::cout << "\n=== 断点续传功能测试 ===" << std::endl;
    if (client.ResumeTransfer(transferId, userId, videoPath)) {
        std::cout << "断点续传启动成功" << std::endl;
    } else {
        std::cout << "断点续传启动失败" << std::endl;
    }
}

int main() {
    signal(SIGINT, signalHandler);
    std::cout << "[Client] 启动..." << std::endl;

    // 基础测试（bool,int,double,string,struct）
    basic_test();
    
    // 初始化文件发送器
    std::cout << "\n=== 初始化文件发送器 ===" << std::endl;
    if (!init_file_sender(4)) {
        std::cerr << "文件发送器初始化失败" << std::endl;
        return -1;
    }
    std::cout << "文件发送器初始化成功" << std::endl;
    
    // 设置DBus客户端实例
    set_dbus_client(&client);
    
    // 发送文件测试
    // send_file_test();
    
    // 测试获取传输状态和缺失块列表
    // gain_transferStatus_missblock();
    
    // 测试断点续传功能
    resume_send_file_test();
    
    
    // 清理文件发送器
    std::cout << "\n=== 清理文件发送器 ===" << std::endl;
    cleanup_file_sender();
    std::cout << "文件发送器清理完成" << std::endl;
     
     // 保持程序运行，等待用户退出
     GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
     g_main_loop_run(loop);
     g_main_loop_unref(loop);
     return 0;
}