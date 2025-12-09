#include "ClientDBus.h"
#include <iostream>
#include <cstring>
#include <mutex>

static const char* SERVICE_NAME = "com.example.TestService";
static const char* OBJECT_PATH = "/com/example/TestService";
static const char* INTERFACE_NAME = "com.example.ITestService";

ClientDBus::ClientDBus() : conn_(nullptr) {
    init();
}

ClientDBus::~ClientDBus() {
    if (conn_) {
        g_object_unref(conn_);
        conn_ = nullptr;
    }
}

bool ClientDBus::init() {
    GError* error = nullptr;
    conn_ = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (!conn_) {
        std::cerr << "[ClientDBus] GDBus connection error: " << (error ? error->message : "unknown") << std::endl;
        if (error) g_error_free(error);
        return false;
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
            std::cout << "[Client] 收到 service 广播 TestInfoChanged: bool=" << (b ? "true" : "false")
                      << ", int=" << i << ", double=" << d << ", string=" << s << std::endl;
        },
        nullptr,
        nullptr
    );
    return true;
}

bool ClientDBus::SetTestBool(bool value) {
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "SetTestBool",
        g_variant_new("(b)", value),
        G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);
    if (!result) {
        std::cerr << "SetTestBool call failed: " << (error ? error->message : "unknown") << std::endl;
        if (error) g_error_free(error);
        return false;
    }
    gboolean ret;
    g_variant_get(result, "(b)", &ret);
    g_variant_unref(result);
    return ret;
}

bool ClientDBus::SetTestInt(int value) {
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "SetTestInt",
        g_variant_new("(i)", value),
        G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);
    if (!result) {
        std::cerr << "SetTestInt call failed: " << (error ? error->message : "unknown") << std::endl;
        if (error) g_error_free(error);
        return false;
    }
    gboolean ret;
    g_variant_get(result, "(b)", &ret);
    g_variant_unref(result);
    return ret;
}

bool ClientDBus::SetTestDouble(double value) {
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "SetTestDouble",
        g_variant_new("(d)", value),
        G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);
    if (!result) {
        std::cerr << "SetTestDouble call failed: " << (error ? error->message : "unknown") << std::endl;
        if (error) g_error_free(error);
        return false;
    }
    gboolean ret;
    g_variant_get(result, "(b)", &ret);
    g_variant_unref(result);
    return ret;
}

bool ClientDBus::SetTestString(const std::string& value) {
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "SetTestString",
        g_variant_new("(s)", value.c_str()),
        G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);
    if (!result) {
        std::cerr << "SetTestString call failed: " << (error ? error->message : "unknown") << std::endl;
        if (error) g_error_free(error);
        return false;
    }
    gboolean ret;
    g_variant_get(result, "(b)", &ret);
    g_variant_unref(result);
    return ret;
}

bool ClientDBus::SetTestInfo(const TestInfo& info) {
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "SetTestInfo",
        g_variant_new("((bids))", info.bool_param, info.int_param, info.double_param, info.string_param.c_str()),
        G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);
    if (!result) {
        std::cerr << "SetTestInfo call failed: " << (error ? error->message : "unknown") << std::endl;
        if (error) g_error_free(error);
        return false;
    }
    gboolean ret;
    g_variant_get(result, "(b)", &ret);
    g_variant_unref(result);
    return ret;
}

bool ClientDBus::GetTestBool() {
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "GetTestBool",
        nullptr,
        G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);
    if (!result) {
        std::cerr << "GetTestBool call failed: " << (error ? error->message : "unknown") << std::endl;
        if (error) g_error_free(error);
        return false;
    }
    gboolean ret;
    g_variant_get(result, "(b)", &ret);
    g_variant_unref(result);
    return ret;
}

int ClientDBus::GetTestInt() {
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "GetTestInt",
        nullptr,
        G_VARIANT_TYPE("(i)"), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);
    if (!result) {
        std::cerr << "GetTestInt call failed: " << (error ? error->message : "unknown") << std::endl;
        if (error) g_error_free(error);
        return 0;
    }
    gint32 ret;
    g_variant_get(result, "(i)", &ret);
    g_variant_unref(result);
    return ret;
}

double ClientDBus::GetTestDouble() {
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "GetTestDouble",
        nullptr,
        G_VARIANT_TYPE("(d)"), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);
    if (!result) {
        std::cerr << "GetTestDouble call failed: " << (error ? error->message : "unknown") << std::endl;
        if (error) g_error_free(error);
        return 0.0;
    }
    gdouble ret;
    g_variant_get(result, "(d)", &ret);
    g_variant_unref(result);
    return ret;
}

std::string ClientDBus::GetTestString() {
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "GetTestString",
        nullptr,
        G_VARIANT_TYPE("(s)"), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);
    if (!result) {
        std::cerr << "GetTestString call failed: " << (error ? error->message : "unknown") << std::endl;
        if (error) g_error_free(error);
        return std::string();
    }
    const gchar* ret;
    g_variant_get(result, "(s)", &ret);
    std::string str = ret;
    g_variant_unref(result);
    return str;
}

TestInfo ClientDBus::GetTestInfo() {
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "GetTestInfo",
        nullptr,
        G_VARIANT_TYPE("((bids))"), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);
    TestInfo info{};
    if (!result) {
        std::cerr << "GetTestInfo call failed: " << (error ? error->message : "unknown") << std::endl;
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
    GError* error = nullptr;
    GVariant* result = nullptr;

    std::unique_lock<std::mutex> lock(mutex_);

    if (!conn_) {
        std::cerr << "[ClientDBus] DBus连接无效" << std::endl;
        return false;
    }

    std::cout << "[ClientDBus] 发送文件块，索引: " << chunk.fileIndex
              << " 大小: " << chunk.chunkLength << std::endl;

    try {
        // 构建字节数组
        GVariantBuilder* array_builder = g_variant_builder_new(G_VARIANT_TYPE("ay"));
        for (size_t i = 0; i < chunk.chunkLength; ++i) {
            g_variant_builder_add(array_builder, "y", (guchar)chunk.data[i]);
        }
        GVariant* byte_array = g_variant_builder_end(array_builder);
        g_variant_builder_unref(array_builder);

        GVariant* params = g_variant_new(
            "(@ayssiuiiub)",
            byte_array,
            chunk.userid,
            chunk.fileName,
            chunk.fileIndex,
            (guint)chunk.totalChunks,
            (gint)chunk.chunkLength,
            chunk.fileLength,
            (guint)chunk.fileMode,
            chunk.isLastChunk
        );

        result = g_dbus_connection_call_sync(
            conn_,
            SERVICE_NAME,
            OBJECT_PATH,
            INTERFACE_NAME,
            "SendFileChunk",
            params,
            G_VARIANT_TYPE("(b)"),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            &error
        );

        if (!result) {
            std::cerr << "SendFileChunk call failed: " << (error ? error->message : "unknown") << std::endl;
            if (error) g_error_free(error);
            return false;
        }

        gboolean ret;
        g_variant_get(result, "(b)", &ret);
        g_variant_unref(result);

        std::cout << "[ClientDBus] DBus调用成功" << std::endl;
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