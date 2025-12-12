#include "ClientDBus.h"
#include <iostream>
#include <cstring>
#include <mutex>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

// 全局互斥锁，用于保护std::cout
static std::mutex cout_mutex;

static const char* SERVICE_NAME = "com.example.TestService";
static const char* OBJECT_PATH = "/com/example/TestService";
static const char* INTERFACE_NAME = "com.example.ITestService";

// GDBus连接关闭回调函数
static void connection_closed_callback(GDBusConnection* connection, gboolean remote_peer_vanished, GError* error, gpointer user_data) {
    ClientDBus* client = static_cast<ClientDBus*>(user_data);
    if (client) {
        client->on_connection_closed(remote_peer_vanished, error);
    }
}

ClientDBus::ClientDBus() : conn_(nullptr) {
    // 启用自动重连
    auto_reconnect_ = true;
    
    // 启动心跳检测
    heartbeat_active_ = true;
    heartbeat_thread_ = std::thread([this]() {
        this->heartbeat_worker();
    });
    
    init();
}

ClientDBus::~ClientDBus() {
    // 停止心跳检测
    heartbeat_active_ = false;
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }
    
    // 停止重连线程
    auto_reconnect_ = false;
    if (reconnect_thread_.joinable()) {
        reconnect_thread_.join();
    }
    
    if (conn_) {
        g_object_unref(conn_);
        conn_ = nullptr;
    }
}

// GDBus连接关闭处理
void ClientDBus::on_connection_closed(gboolean remote_peer_vanished, GError* error) {
    std::cout << "[ClientDBus] GDBus连接已断开" << std::endl;
    if (error) {
        std::cerr << "[ClientDBus] 连接错误: " << error->message << std::endl;
    }
    
    // 清理连接资源
    if (conn_) {
        // 断开信号连接
        g_signal_handlers_disconnect_by_data(conn_, this);
        g_object_unref(conn_);
        conn_ = nullptr;
    }
    
    // 设置连接状态
    is_connected_ = false;
    
    // 通知连接状态变化
    {
        std::lock_guard<std::mutex> callback_lock(callback_mutex_);
        for (const auto& callback : connection_callbacks_) {
            callback(false);
        }
    }
    
    // 如果启用了自动重连，启动重连线程
    if (auto_reconnect_) {
        // 等待现有重连线程结束（如果有）
        if (reconnect_thread_.joinable()) {
            if (reconnect_thread_active_) {
                // 如果重连线程正在运行，等待它结束
                reconnect_thread_.join();
            }
        }
        
        // 启动新的重连线程
        reconnect_thread_ = std::thread([this]() {
            this->reconnect_worker();
        });
    }
}

bool ClientDBus::init() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    // 清理现有连接
    if (conn_) {
        g_object_unref(conn_);
        conn_ = nullptr;
    }
    
    GError* error = nullptr;
    conn_ = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (!conn_) {
        std::cerr << "[ClientDBus] GDBus连接错误: " << (error ? error->message : "unknown") << std::endl;
        if (error) g_error_free(error);
        
        // 连接失败，设置连接状态
        is_connected_ = false;
        
        // 通知连接状态变化
        {
            std::lock_guard<std::mutex> callback_lock(callback_mutex_);
            for (const auto& callback : connection_callbacks_) {
                callback(false);
            }
        }
        
        // 如果启用了自动重连，启动重连线程
        if (auto_reconnect_) {
            if (!reconnect_thread_.joinable()) {
                reconnect_thread_ = std::thread([this]() {
                    this->reconnect_worker();
                });
            }
        }
        
        return false;
    }
    
    // 连接成功，设置连接状态
    is_connected_ = true;
    std::cout << "[ClientDBus] GDBus连接成功，服务名: " << SERVICE_NAME << std::endl;
    
    // 设置GDBus连接关闭信号监听
    g_signal_connect(conn_, "closed", G_CALLBACK(connection_closed_callback), this);
    
    // 通知连接状态变化
    {
        std::lock_guard<std::mutex> callback_lock(callback_mutex_);
        for (const auto& callback : connection_callbacks_) {
            callback(true);
        }
    }
    
    // 订阅服务端广播的信号
    g_dbus_connection_signal_subscribe(
        conn_,
        SERVICE_NAME,
        INTERFACE_NAME,
        "TestBoolChanged",
        OBJECT_PATH,
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,
        [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*, GVariant* parameters, gpointer) {
            gboolean value;
            g_variant_get(parameters, "(b)", &value);
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "[Client] 收到 service 广播 TestBoolChanged: " << (value ? "true" : "false") << std::endl;
        },
        nullptr,
        nullptr
    );
    // TestIntChanged
    g_dbus_connection_signal_subscribe(
        conn_,
        SERVICE_NAME,
        INTERFACE_NAME,
        "TestIntChanged",
        OBJECT_PATH,
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,
        [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*, GVariant* parameters, gpointer) {
            gint32 value;
            g_variant_get(parameters, "(i)", &value);
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "[Client] 收到 service 广播 TestIntChanged: " << value << std::endl;
        },
        nullptr,
        nullptr
    );
    // TestDoubleChanged
    g_dbus_connection_signal_subscribe(
        conn_,
        SERVICE_NAME,
        INTERFACE_NAME,
        "TestDoubleChanged",
        OBJECT_PATH,
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,
        [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*, GVariant* parameters, gpointer) {
            gdouble value;
            g_variant_get(parameters, "(d)", &value);
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "[Client] 收到 service 广播 TestDoubleChanged: " << value << std::endl;
        },
        nullptr,
        nullptr
    );
    // TestStringChanged
    g_dbus_connection_signal_subscribe(
        conn_,
        SERVICE_NAME,
        INTERFACE_NAME,
        "TestStringChanged",
        OBJECT_PATH,
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,
        [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*, GVariant* parameters, gpointer) {
            const gchar* value;
            g_variant_get(parameters, "(s)", &value);
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "[Client] 收到 service 广播 TestStringChanged: " << value << std::endl;
        },
        nullptr,
        nullptr
    );
    // TestInfoChanged
    g_dbus_connection_signal_subscribe(
        conn_,
        SERVICE_NAME,
        INTERFACE_NAME,
        "TestInfoChanged",
        OBJECT_PATH,
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,
        [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*, GVariant* parameters, gpointer) {
            gboolean b; gint32 i; gdouble d; const gchar* s;
            g_variant_get(parameters, "((bids))", &b, &i, &d, &s);
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "[Client] 收到 service 广播 TestInfoChanged: bool=" << (b ? "true" : "false")
                      << ", int=" << i << ", double=" << d << ", string=" << s << std::endl;
        },
        nullptr,
        nullptr
    );
    return true;
}

// 重连工作线程
void ClientDBus::reconnect_worker() {
    const int max_retries = 10;
    int retry_count = 0;
    
    reconnect_thread_active_ = true;
    
    while (retry_count < max_retries && auto_reconnect_) {
        if (retry_count > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(reconnect_interval_));
        }
        
        std::cout << "[ClientDBus] 尝试重连 (" << (retry_count + 1) << "/" << max_retries << ")" << std::endl;
        
        if (init()) {
            std::cout << "[ClientDBus] 重连成功" << std::endl;
            
            // 重连成功后检查服务是否真正可用
            if (is_connected()) {
                std::cout << "[ClientDBus] 服务验证成功，连接已完全恢复" << std::endl;
                break;
            } else {
                std::cout << "[ClientDBus] 服务验证失败，继续重连" << std::endl;
            }
        } else {
            std::cout << "[ClientDBus] 重连失败" << std::endl;
        }
        
        retry_count++;
    }
    
    if (retry_count >= max_retries) {
        std::cout << "[ClientDBus] 达到最大重连次数，停止重连" << std::endl;
    }
    
    reconnect_thread_active_ = false;
}

bool ClientDBus::is_connected() const {
    // 检查连接状态标志
    if (!is_connected_) {
        return false;
    }
    
    // 检查连接对象是否有效
    if (!conn_) {
        return false;
    }
    
    // 检查服务是否可用（通过简单的ping操作）
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "GetTestBool",
        nullptr,
        G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, 1000, nullptr, &error); // 1秒超时
    
    if (!result) {
        // 如果是连接错误，服务不可用
        if (error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED)) {
            if (error) g_error_free(error);
            return false;
        }
        
        // 其他错误（如服务不存在）也视为不可用
        if (error) g_error_free(error);
        return false;
    }
    
    g_variant_unref(result);
    return true;
}

// 启用/禁用自动重连
void ClientDBus::enable_auto_reconnect(bool enable) {
    auto_reconnect_ = enable;
    std::cout << "[ClientDBus] 自动重连 " << (enable ? "启用" : "禁用") << std::endl;
}

// 设置重连间隔
void ClientDBus::set_reconnect_interval(int seconds) {
    if (seconds > 0) {
        reconnect_interval_ = seconds;
        std::cout << "[ClientDBus] 重连间隔设置为: " << seconds << "秒" << std::endl;
    }
}

// 添加连接状态回调
void ClientDBus::add_connection_callback(const ConnectionCallback& callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    connection_callbacks_.push_back(callback);
    std::cout << "[ClientDBus] 添加连接状态回调" << std::endl;
}

bool ClientDBus::SetTestBool(bool value) {
    // 检查连接状态
    if (!is_connected_) {
        std::cerr << "[ClientDBus] SetTestBool失败: 连接已断开" << std::endl;
        return false;
    }
    
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "SetTestBool",
        g_variant_new("(b)", value),
        G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &error); // 5秒超时
    if (!result) {
        std::cerr << "[ClientDBus] SetTestBool调用失败: " << (error ? error->message : "unknown") << std::endl;
        
        // 如果是连接错误，标记为断开
        if (error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED)) {
            is_connected_ = false;
            std::cerr << "[ClientDBus] 检测到连接断开，将尝试重连" << std::endl;
            
            // 启动重连线程
            if (auto_reconnect_ && (!reconnect_thread_.joinable() || !reconnect_thread_active_)) {
                if (reconnect_thread_.joinable()) {
                    reconnect_thread_.join();
                }
                reconnect_thread_ = std::thread([this]() {
                    this->reconnect_worker();
                });
            }
        }
        
        if (error) g_error_free(error);
        return false;
    }
    gboolean ret;
    g_variant_get(result, "(b)", &ret);
    g_variant_unref(result);
    return ret;
}

bool ClientDBus::SetTestInt(int value) {
    // 检查连接状态
    if (!is_connected_) {
        std::cerr << "[ClientDBus] SetTestInt失败: 连接已断开" << std::endl;
        return false;
    }
    
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "SetTestInt",
        g_variant_new("(i)", value),
        G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &error); // 5秒超时
    if (!result) {
        std::cerr << "[ClientDBus] SetTestInt调用失败: " << (error ? error->message : "unknown") << std::endl;
        
        // 如果是连接错误，标记为断开
        if (error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED)) {
            is_connected_ = false;
            std::cerr << "[ClientDBus] 检测到连接断开，将尝试重连" << std::endl;
            
            // 启动重连线程
            if (auto_reconnect_ && (!reconnect_thread_.joinable() || !reconnect_thread_active_)) {
                if (reconnect_thread_.joinable()) {
                    reconnect_thread_.join();
                }
                reconnect_thread_ = std::thread([this]() {
                    this->reconnect_worker();
                });
            }
        }
        
        if (error) g_error_free(error);
        return false;
    }
    gboolean ret;
    g_variant_get(result, "(b)", &ret);
    g_variant_unref(result);
    return ret;
}

bool ClientDBus::SetTestDouble(double value) {
    // 检查连接状态
    if (!is_connected_) {
        std::cerr << "[ClientDBus] SetTestDouble失败: 连接已断开" << std::endl;
        return false;
    }
    
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "SetTestDouble",
        g_variant_new("(d)", value),
        G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &error); // 5秒超时
    if (!result) {
        std::cerr << "[ClientDBus] SetTestDouble调用失败: " << (error ? error->message : "unknown") << std::endl;
        
        // 如果是连接错误，标记为断开
        if (error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED)) {
            is_connected_ = false;
            std::cerr << "[ClientDBus] 检测到连接断开，将尝试重连" << std::endl;
            
            // 启动重连线程
            if (auto_reconnect_ && (!reconnect_thread_.joinable() || !reconnect_thread_active_)) {
                if (reconnect_thread_.joinable()) {
                    reconnect_thread_.join();
                }
                reconnect_thread_ = std::thread([this]() {
                    this->reconnect_worker();
                });
            }
        }
        
        if (error) g_error_free(error);
        return false;
    }
    gboolean ret;
    g_variant_get(result, "(b)", &ret);
    g_variant_unref(result);
    return ret;
}

bool ClientDBus::SetTestString(const std::string& value) {
    // 检查连接状态
    if (!is_connected_) {
        std::cerr << "[ClientDBus] SetTestString失败: 连接已断开" << std::endl;
        return false;
    }
    
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "SetTestString",
        g_variant_new("(s)", value.c_str()),
        G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &error); // 5秒超时
    if (!result) {
        std::cerr << "[ClientDBus] SetTestString调用失败: " << (error ? error->message : "unknown") << std::endl;
        
        // 如果是连接错误，标记为断开
        if (error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED)) {
            is_connected_ = false;
            std::cerr << "[ClientDBus] 检测到连接断开，将尝试重连" << std::endl;
            
            // 启动重连线程
            if (auto_reconnect_ && (!reconnect_thread_.joinable() || !reconnect_thread_active_)) {
                if (reconnect_thread_.joinable()) {
                    reconnect_thread_.join();
                }
                reconnect_thread_ = std::thread([this]() {
                    this->reconnect_worker();
                });
            }
        }
        
        if (error) g_error_free(error);
        return false;
    }
    gboolean ret;
    g_variant_get(result, "(b)", &ret);
    g_variant_unref(result);
    return ret;
}

bool ClientDBus::SetTestInfo(const TestInfo& info) {
    // 检查连接状态
    if (!is_connected_) {
        std::cerr << "[ClientDBus] SetTestInfo失败: 连接已断开" << std::endl;
        return false;
    }
    
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "SetTestInfo",
        g_variant_new("((bids))", info.bool_param, info.int_param, info.double_param, info.string_param.c_str()),
        G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &error); // 5秒超时
    if (!result) {
        std::cerr << "[ClientDBus] SetTestInfo调用失败: " << (error ? error->message : "unknown") << std::endl;
        
        // 如果是连接错误，标记为断开
        if (error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED)) {
            is_connected_ = false;
            std::cerr << "[ClientDBus] 检测到连接断开，将尝试重连" << std::endl;
            
            // 启动重连线程
            if (auto_reconnect_ && (!reconnect_thread_.joinable() || !reconnect_thread_active_)) {
                if (reconnect_thread_.joinable()) {
                    reconnect_thread_.join();
                }
                reconnect_thread_ = std::thread([this]() {
                    this->reconnect_worker();
                });
            }
        }
        
        if (error) g_error_free(error);
        return false;
    }
    gboolean ret;
    g_variant_get(result, "(b)", &ret);
    g_variant_unref(result);
    return ret;
}

bool ClientDBus::GetTestBool() {
    // 检查连接状态
    if (!is_connected_) {
        std::cerr << "[ClientDBus] GetTestBool失败: 连接已断开" << std::endl;
        return false;
    }
    
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "GetTestBool",
        nullptr, G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &error); // 5秒超时
    if (!result) {
        std::cerr << "[ClientDBus] GetTestBool调用失败: " << (error ? error->message : "unknown") << std::endl;
        
        // 如果是连接错误，标记为断开
        if (error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED)) {
            is_connected_ = false;
            std::cerr << "[ClientDBus] 检测到连接断开，将尝试重连" << std::endl;
        }
        
        if (error) g_error_free(error);
        return false;
    }
    gboolean ret;
    g_variant_get(result, "(b)", &ret);
    g_variant_unref(result);
    return ret;
}

int ClientDBus::GetTestInt() {
    // 检查连接状态
    if (!is_connected_) {
        std::cerr << "[ClientDBus] GetTestInt失败: 连接已断开" << std::endl;
        return 0;
    }
    
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "GetTestInt",
        nullptr, G_VARIANT_TYPE("(i)"), G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &error); // 5秒超时
    if (!result) {
        std::cerr << "[ClientDBus] GetTestInt调用失败: " << (error ? error->message : "unknown") << std::endl;
        
        // 如果是连接错误，标记为断开
        if (error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED)) {
            is_connected_ = false;
            std::cerr << "[ClientDBus] 检测到连接断开，将尝试重连" << std::endl;
            
            // 启动重连线程
            if (auto_reconnect_ && (!reconnect_thread_.joinable() || !reconnect_thread_active_)) {
                if (reconnect_thread_.joinable()) {
                    reconnect_thread_.join();
                }
                reconnect_thread_ = std::thread([this]() {
                    this->reconnect_worker();
                });
            }
        }
        
        if (error) g_error_free(error);
        return 0;
    }
    gint32 ret;
    g_variant_get(result, "(i)", &ret);
    g_variant_unref(result);
    return ret;
}

double ClientDBus::GetTestDouble() {
    // 检查连接状态
    if (!is_connected_) {
        std::cerr << "[ClientDBus] GetTestDouble失败: 连接已断开" << std::endl;
        return 0.0;
    }
    
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "GetTestDouble",
        nullptr, G_VARIANT_TYPE("(d)"), G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &error); // 5秒超时
    if (!result) {
        std::cerr << "[ClientDBus] GetTestDouble调用失败: " << (error ? error->message : "unknown") << std::endl;
        
        // 如果是连接错误，标记为断开
        if (error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED)) {
            is_connected_ = false;
            std::cerr << "[ClientDBus] 检测到连接断开，将尝试重连" << std::endl;
            
            // 启动重连线程
            if (auto_reconnect_ && (!reconnect_thread_.joinable() || !reconnect_thread_active_)) {
                if (reconnect_thread_.joinable()) {
                    reconnect_thread_.join();
                }
                reconnect_thread_ = std::thread([this]() {
                    this->reconnect_worker();
                });
            }
        }
        
        if (error) g_error_free(error);
        return 0.0;
    }
    gdouble ret;
    g_variant_get(result, "(d)", &ret);
    g_variant_unref(result);
    return ret;
}

std::string ClientDBus::GetTestString() {
    // 检查连接状态
    if (!is_connected_) {
        std::cerr << "[ClientDBus] GetTestString失败: 连接已断开" << std::endl;
        return "";
    }
    
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "GetTestString",
        nullptr, G_VARIANT_TYPE("(s)"), G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &error); // 5秒超时
    if (!result) {
        std::cerr << "[ClientDBus] GetTestString调用失败: " << (error ? error->message : "unknown") << std::endl;
        
        // 如果是连接错误，标记为断开
        if (error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED)) {
            is_connected_ = false;
            std::cerr << "[ClientDBus] 检测到连接断开，将尝试重连" << std::endl;
            
            // 启动重连线程
            if (auto_reconnect_ && (!reconnect_thread_.joinable() || !reconnect_thread_active_)) {
                if (reconnect_thread_.joinable()) {
                    reconnect_thread_.join();
                }
                reconnect_thread_ = std::thread([this]() {
                    this->reconnect_worker();
                });
            }
        }
        
        if (error) g_error_free(error);
        return "";
    }
    const gchar* ret;
    g_variant_get(result, "(s)", &ret);
    std::string result_str(ret);
    g_variant_unref(result);
    return result_str;
}

TestInfo ClientDBus::GetTestInfo() {
    TestInfo info;
    
    // 检查连接状态
    if (!is_connected_) {
        std::cerr << "[ClientDBus] GetTestInfo失败: 连接已断开" << std::endl;
        return info;
    }
    
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "GetTestInfo",
        nullptr, G_VARIANT_TYPE("((bids))"), G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &error); // 5秒超时
    if (!result) {
        std::cerr << "[ClientDBus] GetTestInfo调用失败: " << (error ? error->message : "unknown") << std::endl;
        
        // 如果是连接错误，标记为断开
        if (error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED)) {
            is_connected_ = false;
            std::cerr << "[ClientDBus] 检测到连接断开，将尝试重连" << std::endl;
            
            // 启动重连线程
            if (auto_reconnect_ && (!reconnect_thread_.joinable() || !reconnect_thread_active_)) {
                if (reconnect_thread_.joinable()) {
                    reconnect_thread_.join();
                }
                reconnect_thread_ = std::thread([this]() {
                    this->reconnect_worker();
                });
            }
        }
        
        if (error) g_error_free(error);
        return info;
    }
    gboolean b; gint32 i; gdouble d; const gchar* s;
    g_variant_get(result, "((bids))", &b, &i, &d, &s);
    info.bool_param = b;
    info.int_param = i;
    info.double_param = d;
    info.string_param = s;
    g_variant_unref(result);
    return info;
}

bool ClientDBus::SendFileChunk(const FileChunk& chunk) {
    // 检查连接状态
    if (!is_connected_) {
        std::cerr << "[ClientDBus] SendFileChunk失败: 连接已断开" << std::endl;
        return false;
    }
    
    GError* error = nullptr;
    GVariant* result = nullptr;

    std::unique_lock<std::recursive_mutex> lock(mutex_);

    if (!conn_) {
        std::cerr << "[ClientDBus] DBus连接无效" << std::endl;
        return false;
    }

    // std::cout << "[ClientDBus] 发送文件块: " << chunk.fileName
    //           << ", 索引: " << chunk.fileIndex
    //           << ", 大小: " << chunk.chunkLength
    //           << ", 传输ID: " << (chunk.transferId[0] ? chunk.transferId : "无") << std::endl;

    try {
        // 构建字节数组
        GVariantBuilder* array_builder = g_variant_builder_new(G_VARIANT_TYPE("ay"));
        for (size_t i = 0; i < chunk.chunkLength; ++i) {
            g_variant_builder_add(array_builder, "y", (guchar)chunk.data[i]);
        }
        GVariant* byte_array = g_variant_builder_end(array_builder);
        g_variant_builder_unref(array_builder);

        GVariant* params = g_variant_new(
            "(@ayssiuiiubs)",
            byte_array,
            chunk.userid,
            chunk.fileName,
            chunk.fileIndex,
            (guint)chunk.totalChunks,
            (gint)chunk.chunkLength,
            chunk.fileLength,
            (guint)chunk.fileMode,
            chunk.isLastChunk,
            chunk.transferId
        );

        // std::cout << "[ClientDBus] filemode:" << chunk.fileMode << std::endl;

        result = g_dbus_connection_call_sync(
            conn_,
            SERVICE_NAME,
            OBJECT_PATH,
            INTERFACE_NAME,
            "SendFileChunk",
            params,
            G_VARIANT_TYPE("(b)"),
            G_DBUS_CALL_FLAGS_NONE,
            5000, // 5秒超时
            nullptr,
            &error
        );

        if (!result) {
            std::cerr << "[ClientDBus] SendFileChunk调用失败: " << (error ? error->message : "unknown") << std::endl;
            
            // 如果是连接错误，标记为断开
            if (error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED)) {
                is_connected_ = false;
                std::cerr << "[ClientDBus] 检测到连接断开，将尝试重连" << std::endl;
                
                // 启动重连线程
                if (auto_reconnect_ && (!reconnect_thread_.joinable() || !reconnect_thread_active_)) {
                    if (reconnect_thread_.joinable()) {
                        reconnect_thread_.join();
                    }
                    reconnect_thread_ = std::thread([this]() {
                        this->reconnect_worker();
                    });
                }
            }
            
            if (error) g_error_free(error);
            return false;
        }

        gboolean ret;
        g_variant_get(result, "(b)", &ret);
        g_variant_unref(result);

        // std::cout << "[ClientDBus] DBus调用成功" << std::endl;
        return ret;
    } catch (const std::exception& e) {
        std::cerr << "[ClientDBus] 异常: " << e.what() << std::endl;
        if (error) g_error_free(error);
        return false;
    } catch (...) {
        std::cerr << "[ClientDBus] 未知异常" << std::endl;
        if (error) g_error_free(error);
        return false;
    }
}

TransferStatus ClientDBus::GetTransferStatus(const std::string& transferId, const std::string& userid, const std::string& fileName)
{
    GError* error = nullptr;
    TransferStatus status{};
    
    std::unique_lock<std::recursive_mutex> lock(mutex_);
    
    if (!is_connected_) {
        std::cerr << "[ClientDBus] 连接已断开，无法获取传输状态" << std::endl;
        return status;
    }

    if (!conn_) {
        std::cerr << "[ClientDBus] DBus连接无效" << std::endl;
        return status;
    }
    
    std::cout << "[ClientDBus] 获取传输状态，传输ID: " << transferId 
              << " 用户: " << userid << " 文件: " << fileName << std::endl;
    
    GVariant* result = g_dbus_connection_call_sync(
        conn_,
        SERVICE_NAME,
        OBJECT_PATH,
        INTERFACE_NAME,
        "GetTransferStatus",
        g_variant_new("(sss)", transferId.c_str(), userid.c_str(), fileName.c_str()),
        G_VARIANT_TYPE("((sisiiuibtt))"),
        G_DBUS_CALL_FLAGS_NONE,
        5000, // 5秒超时
        nullptr,
        &error
    );
    
    if (!result) {
        std::cerr << "[ClientDBus] GetTransferStatus调用失败: " << (error ? error->message : "unknown") << std::endl;
        
        // 如果是连接错误，标记为断开
        if (error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED)) {
            is_connected_ = false;
            std::cerr << "[ClientDBus] 检测到连接断开，将尝试重连" << std::endl;
            
            // 启动重连线程
            if (auto_reconnect_ && (!reconnect_thread_.joinable() || !reconnect_thread_active_)) {
                if (reconnect_thread_.joinable()) {
                    reconnect_thread_.join();
                }
                reconnect_thread_ = std::thread([this]() {
                    this->reconnect_worker();
                });
            }
        }
        
        if (error) g_error_free(error);
        return status;
    }
    
    // 解析返回的状态信息，格式为(sisiiuibtt)
    const gchar* returnedTransferId;
    gint32 statusCode;
    const gchar* statusMessage;
    gint32 totalChunks, receivedChunks, receivedLength;
    guint fileLength;
    gboolean isCompleted;
    guint64 startTime, lastUpdateTime;
    
    g_variant_get(result, "((sisiiuibtt))", 
                  &returnedTransferId, &statusCode, &statusMessage,
                  &totalChunks, &receivedChunks, &fileLength, &receivedLength, 
                  &isCompleted, &startTime, &lastUpdateTime);
    
    status.totalChunks = totalChunks;
    status.receivedChunks = receivedChunks;
    status.fileLength = fileLength;
    status.receivedLength = receivedLength;
    status.statusCode = statusCode;
    status.isCompleted = isCompleted;
    
    // 初始化位图
    status.chunkBitmap.resize(totalChunks, false);
    
    g_variant_unref(result);
    
    // std::cout << "[ClientDBus] 传输状态获取成功: " 
    //           << "传输ID=" << returnedTransferId 
    //           << ", 状态码=" << statusCode 
    //           << ", 状态消息=" << statusMessage
    //           << ", 总块数=" << totalChunks 
    //           << ", 已接收=" << receivedChunks 
    //           << ", 文件长度=" << fileLength 
    //           << ", 已接收长度=" << receivedLength 
    //           << ", 是否完成=" << (isCompleted ? "是" : "否") 
    //           << ", 开始时间=" << startTime 
    //           << ", 最后更新时间=" << lastUpdateTime << std::endl;
    
    return status;
}

std::vector<int> ClientDBus::GetMissingChunks(const std::string& transferId, const std::string& userid, const std::string& fileName)
{
    GError* error = nullptr;
    std::vector<int> missingChunks{};
    
    std::unique_lock<std::recursive_mutex> lock(mutex_);
    
    if (!is_connected_) {
        std::cerr << "[ClientDBus] 连接已断开，无法获取缺失块列表" << std::endl;
        return missingChunks;
    }

    if (!conn_) {
        std::cerr << "[ClientDBus] DBus连接无效" << std::endl;
        return missingChunks;
    }
    
    std::cout << "[ClientDBus] 获取缺失块列表，传输ID: " << transferId 
              << " 用户: " << userid << " 文件: " << fileName << std::endl;
    
    GVariant* result = g_dbus_connection_call_sync(
        conn_,
        SERVICE_NAME,
        OBJECT_PATH,
        INTERFACE_NAME,
        "GetMissingChunks",
        g_variant_new("(sss)", transferId.c_str(), userid.c_str(), fileName.c_str()),
        G_VARIANT_TYPE("(ai)"),
        G_DBUS_CALL_FLAGS_NONE,
        5000, // 5秒超时
        nullptr,
        &error
    );
    
    if (!result) {
        std::cerr << "[ClientDBus] GetMissingChunks调用失败: " << (error ? error->message : "unknown") << std::endl;
        
        // 如果是连接错误，标记为断开
        if (error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED)) {
            is_connected_ = false;
            std::cerr << "[ClientDBus] 检测到连接断开，将尝试重连" << std::endl;
            
            // 启动重连线程
            if (auto_reconnect_ && (!reconnect_thread_.joinable() || !reconnect_thread_active_)) {
                if (reconnect_thread_.joinable()) {
                    reconnect_thread_.join();
                }
                reconnect_thread_ = std::thread([this]() {
                    this->reconnect_worker();
                });
            }
        }
        
        if (error) g_error_free(error);
        return missingChunks;
    }
    
    // 解析返回的缺失块数组
    GVariantIter* iter;
    g_variant_get(result, "(ai)", &iter);
    
    gint32 chunkIndex;
    while (g_variant_iter_loop(iter, "i", &chunkIndex)) {
        missingChunks.push_back(chunkIndex);
    }
    
    g_variant_iter_free(iter);
    g_variant_unref(result);
    
    std::cout << "[ClientDBus] 缺失块列表获取成功: 共" 
              << missingChunks.size() << "个缺失块" << std::endl;
    
    return missingChunks;
}

bool ClientDBus::ResumeTransfer(const std::string& transferId, const std::string& userid, const std::string& videoPath)
{
    std::unique_lock<std::recursive_mutex> lock(mutex_);
    
    if (!is_connected_) {
        std::cerr << "[ClientDBus] 连接已断开，无法进行断点续传" << std::endl;
        return false;
    }

    if (!conn_) {
        std::cerr << "[ClientDBus] DBus连接无效" << std::endl;
        return false;
    }

    size_t lastSlash = videoPath.find_last_of('/');
    std::string fileName = (lastSlash != std::string::npos) ? videoPath.substr(lastSlash + 1) : videoPath;
    
    std::cout << "[ClientDBus] 开始断点续传，传输ID: " << transferId 
              << " 用户: " << userid << " 文件: " << fileName << "文件路径：" << videoPath << std::endl;
    
    // 获取传输状态
    TransferStatus status = this->GetTransferStatus(transferId, userid, fileName);

    std::cout << "[ClientDBus] 断点续传状态获取完成" << std::endl;
    
    if (status.totalChunks == 0) {
        std::cerr << "[ClientDBus] 无法获取传输状态，传输可能不存在" << std::endl;
        return false;
    }
    
    if (status.isCompleted) {
        std::cout << "[ClientDBus] 传输已完成，无需恢复" << std::endl;
        return true;
    }
    
    // 获取缺失块列表
    std::vector<int> missingChunks = this->GetMissingChunks(transferId, userid, fileName);

    std::cout << "[ClientDBus] 断点续传缺失块获取完成" << std::endl;
    
    if (missingChunks.empty()) {
        std::cout << "[ClientDBus] 没有缺失块，传输可能已完成" << std::endl;
        return true;
    }
    
    std::cout << "[ClientDBus] 断点续传准备完成，缺失块数: " << missingChunks.size() << std::endl;

    // 打开文件
    int fd = open(videoPath.c_str(), O_RDONLY);
    if (fd < 0) {
        std::cerr << "[ClientDBus] 无法打开文件: " << videoPath << std::endl;
        return false;
    }
    
    // 获取文件信息
    struct stat file_stat;
    if (fstat(fd, &file_stat) < 0) {
        std::cerr << "[ClientDBus] 无法获取文件信息: " << videoPath << std::endl;
        close(fd);
        return false;
    }
    
    // 遍历缺失块，重新发送
    for (int chunk_index : missingChunks) {
        // 检查连接是否仍然可用
        if (!is_connected_) {
            std::cerr << "[ClientDBus] 连接断开，停止断点续传" << std::endl;
            close(fd);
            return false;
        }
        
        // 计算块偏移量
        off_t offset = chunk_index * FILE_CHUNK_SIZE;
        
        // 构造文件块
        FileChunk chunk{};
        
        // 设置文件块信息
        chunk.fileIndex = chunk_index;
        chunk.totalChunks = status.totalChunks;
        chunk.fileLength = status.fileLength;
        chunk.fileMode = 0644;
        chunk.isLastChunk = (chunk_index == status.totalChunks - 1);
        
        // 安全复制字符串
        strncpy(chunk.userid, userid.c_str(), sizeof(chunk.userid) - 1);
        chunk.userid[sizeof(chunk.userid) - 1] = '\0';
        strncpy(chunk.fileName, fileName.c_str(), sizeof(chunk.fileName) - 1);
        chunk.fileName[sizeof(chunk.fileName) - 1] = '\0';
        
        // 设置传输ID
        strncpy(chunk.transferId, transferId.c_str(), sizeof(chunk.transferId) - 1);
        chunk.transferId[sizeof(chunk.transferId) - 1] = '\0';
        
        // 读取文件数据到chunk.data
        lseek(fd, offset, SEEK_SET);
        int read_len = read(fd, chunk.data, sizeof(chunk.data));
        if (read_len < 0) {
            std::cerr << "[ClientDBus] 文件读取失败: " << fileName << std::endl;
            close(fd);
            return false;
        }
        chunk.chunkLength = read_len;
        
        // 发送文件块
        std::cout << "[ClientDBus] 重新发送文件块: " << chunk.fileName
                  << " 索引: " << chunk.fileIndex
                  << " 大小: " << chunk.chunkLength
                  << " 传输ID: " << chunk.transferId << std::endl;
        
        // 尝试发送文件块，如果失败则等待重连
        int max_retries = 5;
        int retry_count = 0;
        bool sent = false;
        
        while (retry_count < max_retries) {
            if (SendFileChunk(chunk)) {
                // 发送成功
                sent = true;
                break;
            } else {
                // 发送失败，等待重连
                retry_count++;
                std::cout << "[ClientDBus] 发送失败，重试第" << retry_count << "次..." << std::endl;
                
                // 等待2秒后重试
                std::this_thread::sleep_for(std::chrono::seconds(2));
                
                // 检查连接是否恢复
                if (!is_connected_) {
                    std::cerr << "[ClientDBus] 连接断开，停止断点续传" << std::endl;
                    close(fd);
                    return false;
                }
            }
        }
        
        if (!sent) {
            std::cerr << "[ClientDBus] 发送文件块失败，已达到最大重试次数: " << chunk.fileName << " 索引: " << chunk.fileIndex << std::endl;
            close(fd);
            return false;
        }
    }
    
    // 关闭文件
    close(fd);
    
    std::cout << "[ClientDBus] 断点续传完成，已重发所有缺失块" << std::endl;
    
    return true;
}

// 心跳检测工作线程
void ClientDBus::heartbeat_worker() {
    std::cout << "[ClientDBus] 心跳检测线程启动，间隔: " << heartbeat_interval_ << "秒" << std::endl;
    
    while (heartbeat_active_) {
        std::this_thread::sleep_for(std::chrono::seconds(heartbeat_interval_));
        
        // 如果连接已断开，不需要进行心跳检测
        if (!is_connected_) {
            continue;
        }
        
        // 检查连接是否真正可用
        if (!is_connected()) {
            std::cout << "[ClientDBus] 心跳检测: 连接已断开" << std::endl;
            
            // 手动触发连接断开处理
            on_connection_closed(TRUE, nullptr);
            continue;
        }
        
        // 连接正常，输出心跳日志
        std::cout << "[ClientDBus] 心跳检测: 连接正常" << std::endl;
    }
    
    std::cout << "[ClientDBus] 心跳检测线程停止" << std::endl;
}