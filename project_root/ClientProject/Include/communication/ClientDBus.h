#pragma once
#include <string>
#include <mutex>
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

public:
    bool init();

private:
    GDBusConnection* conn_;
    std::mutex mutex_;
};