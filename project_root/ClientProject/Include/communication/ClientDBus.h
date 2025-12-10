#pragma once
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <functional>
#include <vector>
#include <gio/gio.h>
#include "TestData.h"
#include "FileTransfer.h"

class ClientDBus {
public:
    using ConnectionCallback = std::function<void(bool connected)>;
    
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
    
    // 断点续传相关方法
    TransferStatus GetTransferStatus(const std::string& transferId, const std::string& userid, const std::string& fileName);
    std::vector<int> GetMissingChunks(const std::string& transferId, const std::string& userid, const std::string& fileName);
    bool ResumeTransfer(const std::string& transferId, const std::string& userid, const std::string& videoPath);

    bool is_connected() const;
    void reconnect_worker();
    void enable_auto_reconnect(bool enable);
    void set_reconnect_interval(int seconds);
    void add_connection_callback(const ConnectionCallback& callback);
    
    // GDBus连接关闭回调处理
    void on_connection_closed(gboolean remote_peer_vanished, GError* error);
    
public:
    bool init();

private:
    GDBusConnection* conn_;
    std::recursive_mutex mutex_;
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> auto_reconnect_{false};
    std::atomic<bool> reconnect_thread_active_{false};
    std::atomic<int> reconnect_interval_{5}; // 默认5秒重连间隔
    std::thread reconnect_thread_;
    std::vector<ConnectionCallback> connection_callbacks_;
    std::mutex callback_mutex_;
    
    // 心跳检测相关
    std::atomic<bool> heartbeat_active_{false};
    std::thread heartbeat_thread_;
    std::atomic<int> heartbeat_interval_{3}; // 默认3秒心跳间隔
    
    // 心跳检测工作线程
    void heartbeat_worker();
};