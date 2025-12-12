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
std::string videoPath = "/home/wjl/project/project_root/ClientProject/build/client";
std::string transferId = "video_transfer_001";
std::string userId = "test_user";

std::string folderPath = "/home/wjl/project/project_root/ClientProject/testfile";

void signalHandler(int sig) {
    if (sig == SIGINT) {
        std::cout << "\n[Client] 退出..." << std::endl;
        exit(0);
    }
}

void show_basic_test_menu() {
    std::cout << "\n=========================================" << std::endl;
    std::cout << "            基础功能测试二级菜单            " << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "1. 测试Bool类型数据" << std::endl;
    std::cout << "2. 测试Int类型数据" << std::endl;
    std::cout << "3. 测试Double类型数据" << std::endl;
    std::cout << "4. 测试String类型数据" << std::endl;
    std::cout << "5. 测试TestInfo结构体数据" << std::endl;
    std::cout << "6. 返回一级菜单" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "请输入您要执行的功能编号: ";
}

char get_basic_test_choice() {
    char choice;
    std::cin >> choice;
    return choice;
}

void show_operation_menu() {
    std::cout << "\n=========================================" << std::endl;
    std::cout << "            操作选择三级菜单            " << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "1. 测试Get功能" << std::endl;
    std::cout << "2. 测试Set功能" << std::endl;
    std::cout << "3. 返回二级菜单" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "请选择操作类型: ";
}

char get_operation_choice() {
    char choice;
    std::cin >> choice;
    return choice;
}

void handle_bool_operation(char operation) {
    switch(operation) {
        case '1': {
            std::cout << "测试Get Bool功能..." << std::endl;
            bool value = client.GetTestBool();
            std::cout << "当前Bool值: " << value << std::endl;
            break;
        }
        case '2': {
            bool value;
            std::cout << "请输入Bool值(0-假, 非0-真): ";
            std::cin >> value;
            client.SetTestBool(value);
            std::cout << "设置Bool值成功!" << std::endl;
            break;
        }
        default:
            std::cout << "无效的操作选择！" << std::endl;
            break;
    }
}

void handle_int_operation(char operation) {
    switch(operation) {
        case '1': {
            std::cout << "测试Get Int功能..." << std::endl;
            int value = client.GetTestInt();
            std::cout << "当前Int值: " << value << std::endl;
            break;
        }
        case '2': {
            int value;
            std::cout << "请输入Int值: ";
            std::cin >> value;
            client.SetTestInt(value);
            std::cout << "设置Int值成功!" << std::endl;
            break;
        }
        default:
            std::cout << "无效的操作选择！" << std::endl;
            break;
    }
}

void handle_double_operation(char operation) {
    switch(operation) {
        case '1': {
            std::cout << "测试Get Double功能..." << std::endl;
            double value = client.GetTestDouble();
            std::cout << "当前Double值: " << value << std::endl;
            break;
        }
        case '2': {
            double value;
            std::cout << "请输入Double值: ";
            std::cin >> value;
            client.SetTestDouble(value);
            std::cout << "设置Double值成功!" << std::endl;
            break;
        }
        default:
            std::cout << "无效的操作选择！" << std::endl;
            break;
    }
}

void handle_string_operation(char operation) {
    switch(operation) {
        case '1': {
            std::cout << "测试Get String功能..." << std::endl;
            std::string value = client.GetTestString();
            std::cout << "当前String值: " << value << std::endl;
            break;
        }
        case '2': {
            std::string value;
            std::cout << "请输入String值: ";
            std::cin.ignore(); // 忽略之前的换行符
            std::getline(std::cin, value);
            client.SetTestString(value);
            std::cout << "设置String值成功!" << std::endl;
            break;
        }
        default:
            std::cout << "无效的操作选择！" << std::endl;
            break;
    }
}

void handle_testinfo_operation(char operation) {
    switch(operation) {
        case '1': {
            std::cout << "测试Get TestInfo功能..." << std::endl;
            TestInfo ret = client.GetTestInfo();
            std::cout << "当前TestInfo值: " << std::endl;
            std::cout << "  bool=" << ret.bool_param
                      << ", int=" << ret.int_param
                      << ", double=" << ret.double_param
                      << ", string=" << ret.string_param << std::endl;
            break;
        }
        case '2': {
            bool bool_val;
            int int_val;
            double double_val;
            std::string string_val;
            
            std::cout << "请输入TestInfo结构体数据: " << std::endl;
            std::cout << "Bool值(0-假, 非0-真): ";
            std::cin >> bool_val;
            std::cout << "Int值: ";
            std::cin >> int_val;
            std::cout << "Double值: ";
            std::cin >> double_val;
            std::cout << "String值: ";
            std::cin.ignore(); // 忽略之前的换行符
            std::getline(std::cin, string_val);
            
            TestInfo info{bool_val, int_val, double_val, string_val};
            client.SetTestInfo(info);
            std::cout << "设置TestInfo值成功!" << std::endl;
            break;
        }
        default:
            std::cout << "无效的操作选择！" << std::endl;
            break;
    }
}

void handle_basic_test_choice(char choice) {
    switch(choice) {
        case '1': {
            std::cout << "\n=== 测试Bool类型数据 ===" << std::endl;
            char operation;
            do {
                show_operation_menu();
                operation = get_operation_choice();
                if (operation != '3') {
                    handle_bool_operation(operation);
                    std::cout << "\n按回车键继续..." << std::endl;
                    std::cin.ignore();
                    std::cin.get();
                } else {
                    std::cout << "返回二级菜单..." << std::endl;
                }
            } while (operation != '3');
            break;
        }
        case '2': {
            std::cout << "\n=== 测试Int类型数据 ===" << std::endl;
            char operation;
            do {
                show_operation_menu();
                operation = get_operation_choice();
                if (operation != '3') {
                    handle_int_operation(operation);
                    std::cout << "\n按回车键继续..." << std::endl;
                    std::cin.ignore();
                    std::cin.get();
                } else {
                    std::cout << "返回二级菜单..." << std::endl;
                }
            } while (operation != '3');
            break;
        }
        case '3': {
            std::cout << "\n=== 测试Double类型数据 ===" << std::endl;
            char operation;
            do {
                show_operation_menu();
                operation = get_operation_choice();
                if (operation != '3') {
                    handle_double_operation(operation);
                    std::cout << "\n按回车键继续..." << std::endl;
                    std::cin.ignore();
                    std::cin.get();
                } else {
                    std::cout << "返回二级菜单..." << std::endl;
                }
            } while (operation != '3');
            break;
        }
        case '4': {
            std::cout << "\n=== 测试String类型数据 ===" << std::endl;
            char operation;
            do {
                show_operation_menu();
                operation = get_operation_choice();
                if (operation != '3') {
                    handle_string_operation(operation);
                    std::cout << "\n按回车键继续..." << std::endl;
                    std::cin.ignore();
                    std::cin.get();
                } else {
                    std::cout << "返回二级菜单..." << std::endl;
                }
            } while (operation != '3');
            break;
        }
        case '5': {
            std::cout << "\n=== 测试TestInfo结构体数据 ===" << std::endl;
            char operation;
            do {
                show_operation_menu();
                operation = get_operation_choice();
                if (operation != '3') {
                    handle_testinfo_operation(operation);
                    std::cout << "\n按回车键继续..." << std::endl;
                    std::cin.ignore();
                    std::cin.get();
                } else {
                    std::cout << "返回二级菜单..." << std::endl;
                }
            } while (operation != '3');
            break;
        }
        case '6':
            std::cout << "返回一级菜单..." << std::endl;
            break;
        default:
            std::cout << "无效的选择，请重新输入！" << std::endl;
            break;
    }
}

void basic_test() {
    std::cout << "\n=== 基本DBus功能测试 ===" << std::endl;
    std::cout << "在此菜单中您可以手动输入数据并查看效果。" << std::endl;
    
    char choice;
    do {
        show_basic_test_menu();
        choice = get_basic_test_choice();
        handle_basic_test_choice(choice);
        
        if (choice != '6') {
            std::cout << "\n按回车键继续..." << std::endl;
            std::cin.ignore();
            std::cin.get();
        }
    } while (choice != '6');
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
    
    send_entry(videoPath, userId, fileStat.st_mode, transferId);
}

void send_folderPath_test() {
    std::cout << "\n=== 发送测试文件夹 ===" << std::endl;
    // 检查文件是否存在
    struct stat fileStat;
    if (stat(folderPath.c_str(), &fileStat) != 0) {
        std::cerr << "文件不存在: " << folderPath << std::endl;
        return;
    }
    
    std::cout << "开始发送文件: " << folderPath << std::endl;
    std::cout << "传输ID: " << transferId << std::endl;
    std::cout << "用户ID: " << userId << std::endl;
    
    // send_entry(folderPath, userId, 0644, transferId);
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

void show_menu() {
    std::cout << "\n=========================================" << std::endl;
    std::cout << "            客户端功能测试菜单            " << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "1. 运行基础功能测试（基本DBus功能）" << std::endl;
    std::cout << "2. 发送文件测试" << std::endl;
    std::cout << "3. 发送文件夹测试" << std::endl;
    std::cout << "4. 获取传输状态和缺失块列表" << std::endl;
    std::cout << "5. 断点续传功能测试" << std::endl;
    std::cout << "6. 退出程序" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "请输入您要执行的功能编号: ";
}

char get_user_choice() {
    char choice;
    std::cin >> choice;
    return choice;
}

void handle_menu_choice(char choice) {
    switch(choice) {
        case '1':
            basic_test();
            break;
        case '2':
            send_file_test();
            break;
        case '3':
            send_folderPath_test();
            break;
        case '4':
            gain_transferStatus_missblock();
            break;
        case '5':
            resume_send_file_test();
            break;
        case '6':
            std::cout << "[Client] 正在退出..." << std::endl;
            // 清理文件发送器
            std::cout << "\n=== 清理文件发送器 ===" << std::endl;
            cleanup_file_sender();
            std::cout << "文件发送器清理完成" << std::endl;
            exit(0);
            break;
        default:
            std::cout << "[Client] 无效的选择，请重新输入！" << std::endl;
            break;
    }
}

// 运行GLib主循环的线程函数
void glib_main_loop_thread() {
    GMainLoop* main_loop = g_main_loop_new(nullptr, FALSE);
    g_main_loop_run(main_loop);
    g_main_loop_unref(main_loop);
}

int main() {
    signal(SIGINT, signalHandler);
    std::cout << "[Client] 启动..." << std::endl;

    // 初始化文件发送器
    std::cout << "\n=== 初始化文件发送器 ===" << std::endl;
    if (!init_file_sender(4)) {
        std::cerr << "文件发送器初始化失败" << std::endl;
        return -1;
    }
    std::cout << "文件发送器初始化成功" << std::endl;
    
    // 设置DBus客户端实例
    set_dbus_client(&client);
    
    // 启动GLib主循环线程
    std::thread glib_thread(glib_main_loop_thread);
    glib_thread.detach(); // 分离线程，让它在后台运行
    
    std::cout << "[Client] GLib主循环已启动，正在监听D-Bus信号..." << std::endl;
    
    // 显示菜单循环
    while (true) {
        show_menu();
        char choice = get_user_choice();
        handle_menu_choice(choice);
        std::cout << "\n按回车键继续..." << std::endl;
        std::cin.ignore();
        std::cin.get();
    }

    
    
    return 0;
}