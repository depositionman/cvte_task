#include <iostream>
#include <csignal>
#include <unistd.h>
#include <glib.h>
#include "ClientDBus.h"
#include "TestData.h"
#include "FileSender.h"

void signalHandler(int sig) {
    if (sig == SIGINT) {
        std::cout << "\n[Client] 退出..." << std::endl;
        exit(0);
    }
}

int main() {
    signal(SIGINT, signalHandler);
    std::cout << "[Client] 启动..." << std::endl;

    ClientDBus client;
    // 测试 Set/Get
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

    // 测试 FileSender 发送文件
      std::cout << "[Client] 初始化文件发送器..." << std::endl;
      if (init_file_sender()) {
          std::cout << "[Client] 文件发送器初始化成功，开始发送文件..." << std::endl;
          send_entry("/home/wjl/project/project_root/ClientProject/testimg.webp", "test_user", 0644);
          std::cout << "[Client] 文件发送完成" << std::endl;
          // 清理资源
          cleanup_file_sender();
      } else {
          std::cerr << "[Client] 文件发送器初始化失败" << std::endl;
      }
    
    std::cout << "[Client] 测试完成，按Ctrl+C退出。" << std::endl;
    GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    return 0;
}