#include "DBusAdapter.h"
#include "FileTransfer.h"
#include <iostream>
#include <cstring>
#include <unordered_map>
#include <functional>

// Service name for bus registration
static const char* SERVICE_NAME = "com.example.TestService";
// Introspection XML
static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='com.example.ITestService'>"
    "    <method name='SetTestBool'>"
    "      <arg type='b' name='value' direction='in'/>"
    "      <arg type='b' name='result' direction='out'/>"
    "    </method>"
    "    <method name='SetTestInt'>"
    "      <arg type='i' name='value' direction='in'/>"
    "      <arg type='b' name='result' direction='out'/>"
    "    </method>"
    "    <method name='SetTestDouble'>"
    "      <arg type='d' name='value' direction='in'/>"
    "      <arg type='b' name='result' direction='out'/>"
    "    </method>"
    "    <method name='SetTestString'>"
    "      <arg type='s' name='value' direction='in'/>"
    "      <arg type='b' name='result' direction='out'/>"
    "    </method>"
    "    <method name='SetTestInfo'>"
    "      <arg type='(bids)' name='info' direction='in'/>"
    "      <arg type='b' name='result' direction='out'/>"
    "    </method>"
    "    <method name='GetTestBool'>"
    "      <arg type='b' name='result' direction='out'/>"
    "    </method>"
    "    <method name='GetTestInt'>"
    "      <arg type='i' name='result' direction='out'/>"
    "    </method>"
    "    <method name='GetTestDouble'>"
    "      <arg type='d' name='result' direction='out'/>"
    "    </method>"
    "    <method name='GetTestString'>"
    "      <arg type='s' name='result' direction='out'/>"
    "    </method>"
    "    <method name='GetTestInfo'>"
    "      <arg type='(bids)' name='info' direction='out'/>"
    "    </method>"
    "    <method name='SendFileChunk'>"
    "      <arg type='ay' name='data' direction='in'/>"
    "      <arg type='s' name='userid' direction='in'/>"
    "      <arg type='s' name='fileName' direction='in'/>"
    "      <arg type='i' name='fileIndex' direction='in'/>"
    "      <arg type='u' name='totalChunks' direction='in'/>"
    "      <arg type='i' name='chunkLength' direction='in'/>"
    "      <arg type='i' name='fileLength' direction='in'/>"
    "      <arg type='u' name='fileMode' direction='in'/>"
    "      <arg type='b' name='isLastChunk' direction='in'/>"
    "      <arg type='s' name='transferId' direction='in'/>"
    "      <arg type='b' name='result' direction='out'/>"
    "    </method>"
    "    <method name='GetTransferStatus'>"
    "      <arg type='s' name='transferId' direction='in'/>"
    "      <arg type='s' name='userid' direction='in'/>"
    "      <arg type='s' name='fileName' direction='in'/>"
    "      <arg type='(sisiiuibtt)' name='status' direction='out'/>"
    "    </method>"
    "    <method name='GetMissingChunks'>"
    "      <arg type='s' name='transferId' direction='in'/>"
    "      <arg type='s' name='userid' direction='in'/>"
    "      <arg type='s' name='fileName' direction='in'/>"
    "      <arg type='ai' name='missingChunks' direction='out'/>"
    "    </method>"
    "    <signal name='TestBoolChanged'>"
    "      <arg type='b' name='value'/>"
    "    </signal>"
    "    <signal name='TestIntChanged'>"
    "      <arg type='i' name='value'/>"
    "    </signal>"
    "    <signal name='TestDoubleChanged'>"
    "      <arg type='d' name='value'/>"
    "    </signal>"
    "    <signal name='TestStringChanged'>"
    "      <arg type='s' name='value'/>"
    "    </signal>"
    "    <signal name='TestInfoChanged'>"
    "      <arg type='(bids)' name='info'/>"
    "    </signal>"
    "  </interface>"
    "</node>";

DBusAdapter::DBusAdapter(ITestService* service)
    : test_service_(service), main_loop_(nullptr), introspection_data_(nullptr),
      registration_id_(0), connection_(nullptr) {}

DBusAdapter::~DBusAdapter() {
    // 停止主循环
    if (main_loop_) {
        g_main_loop_quit(main_loop_);
        g_main_loop_unref(main_loop_);
        main_loop_ = nullptr;
    }
    
    // 注销D-Bus对象
    if (connection_ && registration_id_ != 0) {
        g_dbus_connection_unregister_object(connection_, registration_id_);
        registration_id_ = 0;
    }
    
    // 释放总线名称所有权 - 使用异步释放避免双重释放警告
    if (connection_) {
        // 只使用异步释放，避免同步释放可能导致的警告
        g_bus_unown_name(name_owner_id_);
        std::cout << "[DBusAdapter] 总线名称释放请求已发送" << std::endl;
    }
    
    // 强制关闭连接，确保客户端能检测到断开
    if (connection_) {
        // 关闭连接，触发客户端的连接断开检测
        g_dbus_connection_close_sync(connection_, nullptr, nullptr);
        g_object_unref(connection_);
        connection_ = nullptr;
    }
    
    // 释放自省数据
    if (introspection_data_) {
        g_dbus_node_info_unref(introspection_data_);
        introspection_data_ = nullptr;
    }
    
    std::cout << "[DBusAdapter] 资源清理完成" << std::endl;
}

bool DBusAdapter::init() {
    GError* error = nullptr;
    connection_ = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (!connection_) {
        std::cerr << "[DBusAdapter] Failed to get GDBus connection: " << error->message << std::endl;
        g_error_free(error);
        return false;
    }

    introspection_data_ = g_dbus_node_info_new_for_xml(introspection_xml, nullptr);

    GDBusInterfaceVTable vtable = {};
    vtable.method_call = handle_method_call;
    vtable.get_property = nullptr;
    vtable.set_property = nullptr;
    registration_id_ = g_dbus_connection_register_object(
        connection_,
        "/com/example/TestService",
        introspection_data_->interfaces[0],
        &vtable,
        this, // user_data
        nullptr, nullptr);

    if (registration_id_ == 0) {
        std::cerr << "[DBusAdapter] Failed to register object" << std::endl;
        return false;
    }

    main_loop_ = g_main_loop_new(nullptr, FALSE);
    // 注册 well-known bus name，确保 client 能找到 service
    name_owner_id_ = g_bus_own_name_on_connection(
        connection_,
        SERVICE_NAME, 
        G_BUS_NAME_OWNER_FLAGS_NONE,
        nullptr, nullptr, nullptr, nullptr
    );
    std::cout << "[DBusAdapter] GDBus service initialized successfully" << std::endl;
    return true;
}

void DBusAdapter::runLoop() {
    if (main_loop_) {
        g_main_loop_run(main_loop_);
    }
}


using Handler = std::function<void(GVariant*, GDBusMethodInvocation*, ITestService*)>;
static const std::unordered_map<std::string, Handler> method_table = {
    {"SetTestBool", [](GVariant* params, GDBusMethodInvocation* inv, ITestService* svc) {
        gboolean value;
        g_variant_get(params, "(b)", &value);
        bool result = svc->SetTestBool(value);
        g_dbus_method_invocation_return_value(inv, g_variant_new("(b)", result));
    }},
    {"SetTestInt", [](GVariant* params, GDBusMethodInvocation* inv, ITestService* svc) {
        gint32 value;
        g_variant_get(params, "(i)", &value);
        bool result = svc->SetTestInt(value);
        g_dbus_method_invocation_return_value(inv, g_variant_new("(b)", result));
    }},
    {"SetTestDouble", [](GVariant* params, GDBusMethodInvocation* inv, ITestService* svc) {
        gdouble value;
        g_variant_get(params, "(d)", &value);
        bool result = svc->SetTestDouble(value);
        g_dbus_method_invocation_return_value(inv, g_variant_new("(b)", result));
    }},
    {"SetTestString", [](GVariant* params, GDBusMethodInvocation* inv, ITestService* svc) {
        const gchar* value;
        g_variant_get(params, "(s)", &value);
        bool result = svc->SetTestString(value);
        g_dbus_method_invocation_return_value(inv, g_variant_new("(b)", result));
    }},
    {"SetTestInfo", [](GVariant* params, GDBusMethodInvocation* inv, ITestService* svc) {
        gboolean b; gint32 i; gdouble d; const gchar* s;
        g_variant_get(params, "((bids))", &b, &i, &d, &s);
        TestInfo info{b, i, d, s};
        bool result = svc->SetTestInfo(info);
        g_dbus_method_invocation_return_value(inv, g_variant_new("(b)", result));
    }},
    {"GetTestBool", [](GVariant*, GDBusMethodInvocation* inv, ITestService* svc) {
        bool result = svc->GetTestBool();
        g_dbus_method_invocation_return_value(inv, g_variant_new("(b)", result));
    }},
    {"GetTestInt", [](GVariant*, GDBusMethodInvocation* inv, ITestService* svc) {
        int result = svc->GetTestInt();
        g_dbus_method_invocation_return_value(inv, g_variant_new("(i)", result));
    }},
    {"GetTestDouble", [](GVariant*, GDBusMethodInvocation* inv, ITestService* svc) {
        double result = svc->GetTestDouble();
        g_dbus_method_invocation_return_value(inv, g_variant_new("(d)", result));
    }},
    {"GetTestString", [](GVariant*, GDBusMethodInvocation* inv, ITestService* svc) {
        std::string result = svc->GetTestString();
        g_dbus_method_invocation_return_value(inv, g_variant_new("(s)", result.c_str()));
    }},
    {"GetTestInfo", [](GVariant*, GDBusMethodInvocation* inv, ITestService* svc) {
        TestInfo info = svc->GetTestInfo();
        g_dbus_method_invocation_return_value(inv, g_variant_new("((bids))", info.bool_param, info.int_param, info.double_param, info.string_param.c_str()));
    }},
    {"SendFileChunk", [](GVariant* params, GDBusMethodInvocation* inv, ITestService* svc) {
        FileChunk chunk;
        
        // 声明变量用于接收数据
        GVariant* byte_array_variant = nullptr;
        gconstpointer data_ptr = nullptr;
        gsize data_size = 0;
        gchar* userid = nullptr;
        gchar* fileName = nullptr;
        gint fileIndex = 0;
        guint totalChunks = 0;
        gint chunkLength = 0;
        gint fileLength = 0;
        guint fileMode = 0;
        gboolean isLastChunk = FALSE;
        gchar* transferId = nullptr;
        
        g_variant_get(params, "(@ayssiuiiubs)", 
                    &byte_array_variant, 
                    &userid, 
                    &fileName, 
                    &fileIndex, 
                    &totalChunks, 
                    &chunkLength, 
                    &fileLength, 
                    &fileMode, 
                    &isLastChunk,
                    &transferId);
        
        // std::cout << "[DBusAdapter] transferId: " << (transferId ? transferId : "null") << std::endl;
        // std::cout << "[ClientDBus] filemode:" << chunk.fileMode << std::endl;

        if (byte_array_variant) {
            data_ptr = g_variant_get_fixed_array(byte_array_variant, &data_size, sizeof(guchar));
        }
        
        if (data_ptr && data_size > 0) {
            size_t copy_size = (data_size > sizeof(chunk.data)) ? sizeof(chunk.data) : data_size;
            memcpy(chunk.data, data_ptr, copy_size);
        }
        
        if (userid) {
            strncpy(chunk.userid, userid, sizeof(chunk.userid) - 1);
            chunk.userid[sizeof(chunk.userid) - 1] = '\0';  
        } else {
            chunk.userid[0] = '\0';
        }
        
        if (fileName) {
            strncpy(chunk.fileName, fileName, sizeof(chunk.fileName) - 1);
            chunk.fileName[sizeof(chunk.fileName) - 1] = '\0'; 
        } else {
            chunk.fileName[0] = '\0';
        }
        
        if (transferId) {
            strncpy(chunk.transferId, transferId, sizeof(chunk.transferId) - 1);
            chunk.transferId[sizeof(chunk.transferId) - 1] = '\0';
        } else {
            chunk.transferId[0] = '\0';
        }
        
        // 设置其他字段
        chunk.fileIndex = fileIndex;
        chunk.totalChunks = totalChunks;
        chunk.chunkLength = chunkLength;
        chunk.fileLength = fileLength;
        chunk.fileMode = fileMode;
        chunk.isLastChunk = isLastChunk;
        
        // 清理GLib分配的资源
        if (byte_array_variant) {
            g_variant_unref(byte_array_variant);
        }
        g_free(userid);
        g_free(fileName);
        g_free(transferId);
        
        // 调用业务逻辑方法
        bool result = svc->SendFileChunk(chunk);
        
        // 返回结果
        g_dbus_method_invocation_return_value(inv, g_variant_new("(b)", result));
    }},
    {"GetTransferStatus", [](GVariant* params, GDBusMethodInvocation* inv, ITestService* svc) {
        gchar* transferId = nullptr;
        gchar* userid = nullptr;
        gchar* fileName = nullptr;
        
        g_variant_get(params, "(sss)", &transferId, &userid, &fileName);
        
        // 调用业务逻辑获取传输状态
        TransferStatus status = svc->GetTransferStatus(
            transferId ? transferId : "", 
            userid ? userid : "", 
            fileName ? fileName : "");
        
        // 返回传输状态，格式为(sisiiuibtt)
        g_dbus_method_invocation_return_value(inv, 
            g_variant_new("((sisiiuibtt))", 
                transferId ? transferId : "", 
                status.statusCode, 
                "传输状态", 
                status.totalChunks, 
                status.receivedChunks, 
                (guint)status.fileLength, 
                status.receivedLength, 
                status.isCompleted,
                (guint64)time(nullptr) - 3600, // 开始时间（示例：1小时前）
                (guint64)time(nullptr)));      // 最后更新时间
        
        g_free(transferId);
        g_free(userid);
        g_free(fileName);
    }},
    {"GetMissingChunks", [](GVariant* params, GDBusMethodInvocation* inv, ITestService* svc) {
        gchar* transferId = nullptr;
        gchar* userid = nullptr;
        gchar* fileName = nullptr;
        
        g_variant_get(params, "(sss)", &transferId, &userid, &fileName);
        
        // 调用业务逻辑获取缺失块列表
        std::vector<int> missingChunks = svc->GetMissingChunks(
            transferId ? transferId : "", 
            userid ? userid : "", 
            fileName ? fileName : "");
        
        // 将vector<int>转换为GVariant数组
        GVariantBuilder* builder = g_variant_builder_new(G_VARIANT_TYPE("ai"));
        for (int chunkIndex : missingChunks) {
            g_variant_builder_add(builder, "i", chunkIndex);
        }
        
        // 返回缺失块列表
        g_dbus_method_invocation_return_value(inv, 
            g_variant_new("(ai)", builder));
        
        g_variant_builder_unref(builder);
        g_free(transferId);
        g_free(userid);
        g_free(fileName);
    }}
};

void DBusAdapter::handle_method_call(GDBusConnection* /*connection*/,
                                     const gchar* sender,
                                     const gchar* /*object_path*/,
                                     const gchar* /*interface_name*/,
                                     const gchar* method_name,
                                     GVariant* parameters,
                                     GDBusMethodInvocation* invocation,
                                     gpointer user_data) {
    DBusAdapter* adapter = static_cast<DBusAdapter*>(user_data);
    ITestService* service = adapter->getTestService();

    // 记录客户端标识信息
    if (sender) {
        std::cout << "[DBusAdapter] 收到来自客户端 " << sender << " 的方法调用: " << method_name << std::endl;
    } else {
        std::cout << "[DBusAdapter] 收到方法调用: " << method_name << " (发送者未知)" << std::endl;
    }

    auto it = method_table.find(method_name);
    if (it != method_table.end()) {
        it->second(parameters, invocation, service);
    } else {
        std::cerr << "[DBusAdapter] Unknown method: " << method_name << std::endl;
        g_dbus_method_invocation_return_error(invocation, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Unknown method");
    }
}

// D-Bus信号广播接口
void DBusAdapter::emitTestBoolChanged(bool value) {
    if (connection_) {
        g_dbus_connection_emit_signal(
            connection_,
            nullptr, // broadcast to all
            "/com/example/TestService",
            "com.example.ITestService",
            "TestBoolChanged",
            g_variant_new("(b)", value),
            nullptr
        );
    }
}

void DBusAdapter::emitTestIntChanged(int value) {
    if (connection_) {
        g_dbus_connection_emit_signal(
            connection_, nullptr,
            "/com/example/TestService",
            "com.example.ITestService",
            "TestIntChanged",
            g_variant_new("(i)", value), nullptr);
    }
}

void DBusAdapter::emitTestDoubleChanged(double value) {
    if (connection_) {
        g_dbus_connection_emit_signal(
            connection_, nullptr,
            "/com/example/TestService",
            "com.example.ITestService",
            "TestDoubleChanged",
            g_variant_new("(d)", value), nullptr);
    }
}

void DBusAdapter::emitTestStringChanged(const std::string& value) {
    if (connection_) {
        g_dbus_connection_emit_signal(
            connection_, nullptr,
            "/com/example/TestService",
            "com.example.ITestService",
            "TestStringChanged",
            g_variant_new("(s)", value.c_str()), nullptr);
    }
}

void DBusAdapter::emitTestInfoChanged(const TestInfo& info) {
    if (connection_) {
        g_dbus_connection_emit_signal(
            connection_, nullptr,
            "/com/example/TestService",
            "com.example.ITestService",
            "TestInfoChanged",
            g_variant_new("((bids))", info.bool_param, info.int_param, info.double_param, info.string_param.c_str()), nullptr);
    }
}
