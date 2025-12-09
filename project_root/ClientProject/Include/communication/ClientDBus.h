#pragma once
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <gio/gio.h>
#include "TestData.h"
#include "FileTransfer.h"

class ClientDBus {
public:
    ClientDBus();
    ~ClientDBus();

    bool SetTestBool(bool value);
    bool SetTestInt(int value);
    bool SetTestDouble(double value);
    bool SetTestString(const std::string& value);
    bool SetTestInfo(const TestInfo& info);

    bool GetTestBool();
    int GetTestInt();
    double GetTestDouble();
    std::string GetTestString();
    TestInfo GetTestInfo();
    
    // 文件传输相关方法
    bool SendFileChunk(const FileChunk& chunk);

    // 连接管理方法
    bool is_connected() const { return is_connected_; }
    void enable_auto_reconnect(bool enable = true);
    void set_reconnect_interval(int seconds);

public:
    bool init();

private:
    bool connect();
    void disconnect();
    void auto_reconnect_loop();
    bool check_connection();
    void subscribe_signals();

private:
    GDBusConnection* conn_;
    std::mutex mutex_;
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> auto_reconnect_{false};
    std::atomic<int> reconnect_interval_{5}; // 默认5秒重连间隔
    std::thread reconnect_thread_;
};