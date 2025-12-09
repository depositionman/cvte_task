#include "TestService.h"
#include "DBusAdapter.h"
#include "FileReceiver.h"
#include <iostream>
#include "nlohmann/json.hpp"
#include "SafeData.h"
#include <string>

using json = nlohmann::json;

// ITestService接口实现 - Set方法
bool TestService::SetTestBool(bool param) {
    std::cout << "[TestService] SetTestBool: " << std::boolalpha << param << std::endl;
    // bool转字符串存储
    SafeData::getInstance()->setData("test_bool", param ? "1" : "0");
    broadcastTestBoolChanged(param);
    return true;
}

bool TestService::SetTestInt(int param) {
    std::cout << "[TestService] SetTestInt: " << param << std::endl;
    // 存入SafeData
    SafeData::getInstance()->setData("test_int", std::to_string(param));
    broadcastTestIntChanged(param);
    return true;
}

bool TestService::SetTestDouble(double param) {
    std::cout << "[TestService] SetTestDouble: " << param << std::endl;
    SafeData::getInstance()->setData("test_double", std::to_string(param));
    broadcastTestDoubleChanged(param); 
    return true;
}

bool TestService::SetTestString(const std::string& param) {
    std::cout << "[TestService] SetTestString: " << param << std::endl;
    SafeData::getInstance()->setData("test_string", param);
    broadcastTestStringChanged(param);
    return true;
}

bool TestService::SetTestInfo(const TestInfo& info) {
    std::cout << "[TestService] SetTestInfo: " 
              << "bool=" << std::boolalpha << info.bool_param 
              << ", int=" << info.int_param 
              << ", double=" << info.double_param 
              << ", string=" << info.string_param << std::endl;

    try {
        // 1. 将TestInfo序列化为JSON字符串
        json j = info;
        std::string json_str = j.dump(); 

        // 2. 线程安全存入SafeData
        SafeData::getInstance()->setData("test_info", json_str);

        // 3. 广播数据变更
        broadcastTestInfoChanged(info);

        return true;
    } catch (const json::exception& e) {
        std::cerr << "[SetTestInfo] JSON序列化失败: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[SetTestInfo] 异常: " << e.what() << std::endl;
        return false;
    }
}

// ITestService接口实现 - Get方法
bool TestService::GetTestBool() {
    std::cout << "[TestService] GetTestBool" << std::endl;
    std::string value;
    if (SafeData::getInstance()->getData("test_bool", value)) {
        // 兼容"1"/"0"或"true"/"false"两种存储格式
        return (value == "1" || value == "true" || value == "TRUE");
    }
    return false; 
}

int TestService::GetTestInt() {
    std::cout << "[TestService] GetTestInt" << std::endl;
    // 从SafeData读取
    std::string value;
    if (SafeData::getInstance()->getData("test_int", value)) {
        return std::stoi(value);
    }
    return 0; 
}

double TestService::GetTestDouble() {
    std::cout << "[TestService] GetTestDouble" << std::endl;
    std::string value;
    if (SafeData::getInstance()->getData("test_double", value)) {
        try {
            return std::stod(value); 
        } catch (const std::exception& e) {
            std::cerr << "[GetTestDouble] 解析double失败: " << e.what() << std::endl;
        }
    }
    return 0.0; 
}

std::string TestService::GetTestString() {
    std::cout << "[TestService] GetTestString" << std::endl;
    // 从SafeData读取
    std::string value;
    if (SafeData::getInstance()->getData("test_string", value)) {
        return value; 
    }
    return "";  
}

TestInfo TestService::GetTestInfo() {
    std::cout << "[TestService] GetTestInfo" << std::endl;
    TestInfo default_info{}; // 默认空值结构体

    // 1. 从SafeData读取JSON字符串
    std::string json_str;
    if (!SafeData::getInstance()->getData("test_info", json_str)) {
        std::cerr << "[GetTestInfo] 未找到test_info数据" << std::endl;
        return default_info;
    }

    try {
        // 2. JSON字符串解析为JSON对象
        json j = json::parse(json_str);

        // 3. 反序列化为TestInfo
        TestInfo info = j.get<TestInfo>();
        return info;
    } catch (const json::parse_error& e) {
        std::cerr << "[GetTestInfo] JSON解析失败: " << e.what() << " (str: " << json_str << ")" << std::endl;
    } catch (const json::type_error& e) {
        std::cerr << "[GetTestInfo] JSON类型不匹配: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[GetTestInfo] 反序列化失败: " << e.what() << std::endl;
    }

    // 异常时返回默认值
    return default_info;
}

// ITestService接口实现 - 文件传输方法
bool TestService::SendFileChunk(const FileChunk& chunk) {
    std::cout << "[TestService] SendFileChunk: fileName=" << chunk.fileName 
              << ", fileIndex=" << chunk.fileIndex 
              << ", totalChunks=" << chunk.totalChunks 
              << ", chunkLength=" << chunk.chunkLength 
              << ", fileLength=" << chunk.fileLength 
              << ", isLastChunk=" << std::boolalpha << chunk.isLastChunk << std::endl;
    
    // 设置输出目录为当前目录
    std::string outdir = ".";
    
    // 将文件块传递给FileReceiver处理
    ::receive_file_chunk(chunk, outdir);
    std::cout << "[TestService] 文件块已传递给FileReceiver处理" << std::endl;
    return true;
}

// 观察者模式相关方法
void TestService::registerListener(ITestListener* listener) {
    if (listener) {
        std::lock_guard<std::mutex> lock(listener_mutex_);
        listeners_.push_back(listener);
        std::cout << "[TestService] Listener registered" << std::endl;
    }
}

// 广播辅助函数
void TestService::broadcastTestBoolChanged(bool param) {
    std::lock_guard<std::mutex> lock(listener_mutex_);
    for (auto listener : listeners_) {
        if (listener) {
            listener->OnTestBoolChanged(param);
        }
    }
    // D-Bus信号广播
    extern DBusAdapter* g_dbus_adapter;
    if (g_dbus_adapter) {
        g_dbus_adapter->emitTestBoolChanged(param);
        std::cout << "[TestService] Emitting TestBoolChanged signal" << std::endl;
    }
}

void TestService::broadcastTestIntChanged(int param) {
    std::lock_guard<std::mutex> lock(listener_mutex_);
    for (auto listener : listeners_) {
        if (listener) {
            listener->OnTestIntChanged(param);
        }
    }
    extern DBusAdapter* g_dbus_adapter;
    if (g_dbus_adapter) {
        g_dbus_adapter->emitTestIntChanged(param);
    }
}

void TestService::broadcastTestDoubleChanged(double param) {
    std::lock_guard<std::mutex> lock(listener_mutex_);
    for (auto listener : listeners_) {
        if (listener) {
            listener->OnTestDoubleChanged(param);
        }
    }
    extern DBusAdapter* g_dbus_adapter;
    if (g_dbus_adapter) {
        g_dbus_adapter->emitTestDoubleChanged(param);
    }
}

void TestService::broadcastTestStringChanged(const std::string& param) {
    std::lock_guard<std::mutex> lock(listener_mutex_);
    for (auto listener : listeners_) {
        if (listener) {
            listener->OnTestStringChanged(param);
        }
    }
    extern DBusAdapter* g_dbus_adapter;
    if (g_dbus_adapter) {
        g_dbus_adapter->emitTestStringChanged(param);
    }
}

void TestService::broadcastTestInfoChanged(const TestInfo& param) {
    std::lock_guard<std::mutex> lock(listener_mutex_);
    for (auto listener : listeners_) {
        if (listener) {
            listener->OnTestInfoChanged(param);
        }
    }
    extern DBusAdapter* g_dbus_adapter;
    if (g_dbus_adapter) {
        g_dbus_adapter->emitTestInfoChanged(param);
    }
}