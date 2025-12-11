# GDBus技术点代码实现说明

## 1. 基于GDBusConnection的服务注册

**代码位置**：`/home/wjl/project/project_root/ServiceProject/Sources/communication/DBusAdapter.cpp` - `DBusAdapter::init()`方法（第104-132行）

**实现方式**：
```cpp
// 获取GDBusConnection连接
connection_ = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);

// 注册DBus对象
registration_id_ = g_dbus_connection_register_object(
    connection_,
    "/com/example/TestService",  // 对象路径
    introspection_data_->interfaces[0],  // 接口定义
    &vtable,  // 方法调用表
    this,  // 用户数据
    nullptr, nullptr);

// 注册well-known bus name
name_owner_id_ = g_bus_own_name_on_connection(
    connection_,
    SERVICE_NAME,  // 服务名称: "com.example.TestService"
    G_BUS_NAME_OWNER_FLAGS_NONE,
    nullptr, nullptr, nullptr, nullptr
);
```

**技术细节**：
- 使用`g_bus_get_sync()`获取会话总线连接
- 通过`g_dbus_connection_register_object()`注册DBus对象和接口
- 使用`g_bus_own_name_on_connection()`注册服务名称，使客户端能通过名称找到服务

## 2. DBus名称服务冲突处理

**代码位置**：`/home/wjl/project/project_root/ServiceProject/Sources/communication/DBusAdapter.cpp` - `DBusAdapter::init()`方法（第126-132行）

**实现方式**：
```cpp
// 使用G_BUS_NAME_OWNER_FLAGS_NONE标志注册服务名称
name_owner_id_ = g_bus_own_name_on_connection(
    connection_,
    SERVICE_NAME, 
    G_BUS_NAME_OWNER_FLAGS_NONE,  // 冲突处理标志
    nullptr, nullptr, nullptr, nullptr
);
```

**技术细节**：
- 使用`G_BUS_NAME_OWNER_FLAGS_NONE`标志表示：
  - 如果服务名称已被占用，注册会失败
  - 代码中通过返回`false`处理注册失败情况
  - 如需更复杂的冲突处理，可使用`G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT`等标志

## 3. 同步/异步方法实现

### 3.1 服务端方法实现（同步）

**代码位置**：`/home/wjl/project/project_root/ServiceProject/Sources/communication/DBusAdapter.cpp` - `handle_method_call()`方法和`method_table`（第398-436行）

**实现方式**：
```cpp
// 方法调用处理函数
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

    // 查找并调用对应方法处理函数
    auto it = method_table.find(method_name);
    if (it != method_table.end()) {
        it->second(parameters, invocation, service);  // 同步调用方法处理函数
    } else {
        // 处理未知方法
    }
}

// 方法处理表（所有方法都是同步实现）
static const std::unordered_map<std::string, Handler> method_table = {
    {"SetTestBool", [](GVariant* params, GDBusMethodInvocation* inv, ITestService* svc) {
        // 处理方法调用
        g_dbus_method_invocation_return_value(inv, g_variant_new("(b)", result));
    }},
    // 其他方法...
};
```

### 3.2 客户端同步调用

**代码位置**：`/home/wjl/project/project_root/ClientProject/Sources/communication/ClientDBus.cpp` - 各种方法调用（如第424-463行）

**实现方式**：
```cpp
GVariant* result = g_dbus_connection_call_sync(
    conn_, SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, "SetTestString",
    g_variant_new("(s)", value.c_str()),
    G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &error);
```

**技术细节**：
- 服务端所有方法都是同步实现，处理完后立即返回结果
- 客户端使用`g_dbus_connection_call_sync()`进行同步调用
- 同步调用会阻塞当前线程直到收到响应
- 如需异步调用，客户端可使用`g_dbus_connection_call()`方法

## 4. TestService接口设计与实现

**代码位置**：
- 接口定义：`/home/wjl/project/project_root/ServiceProject/Sources/communication/ITestService.h`
- 接口实现：`/home/wjl/project/project_root/ServiceProject/Sources/service/TestService.cpp`

**实现方式**：
```cpp
// ITestService接口定义
class ITestService {
public:
    virtual ~ITestService() = default;
    virtual bool SetTestBool(bool value) = 0;
    virtual bool SetTestInt(int value) = 0;
    // 其他方法...
    virtual bool SendFileChunk(const FileChunk& chunk) = 0;
};

// TestService实现
class TestService : public ITestService {
    // 实现所有接口方法
    bool SetTestBool(bool value) override {
        // 实际业务逻辑
    }
    // 其他方法实现...
};
```

**技术细节**：
- 使用纯虚函数接口（ITestService）定义服务功能
- TestService类实现具体业务逻辑
- DBusAdapter通过ITestService指针调用实际业务方法
- 这种设计实现了DBus通信层与业务逻辑层的分离

## 5. 接口定义语言（XML）的使用

**代码位置**：`/home/wjl/project/project_root/ServiceProject/Sources/communication/DBusAdapter.cpp` - `introspection_xml`常量（第16-83行）

**实现方式**：
```xml
static const gchar introspection_xml[] = "<node>"
    "  <interface name='com.example.ITestService'>"
    "    <method name='SetTestBool'>"
    "      <arg type='b' name='value' direction='in'/>"
    "      <arg type='b' name='result' direction='out'/>"
    "    </method>"
    "    <!-- 其他方法和信号 -->"
    "  </interface>"
    "</node>";

// 解析XML生成接口定义
introspection_data_ = g_dbus_node_info_new_for_xml(introspection_xml, nullptr);
```

**技术细节**：
- 使用XML定义DBus接口、方法、参数和信号
- 通过`g_dbus_node_info_new_for_xml()`解析XML生成接口信息
- 这种方式确保了客户端和服务端接口定义的一致性

## 总结

以上技术点在代码中都有明确的实现，主要集中在DBusAdapter类中。通过这些实现，项目成功构建了基于GDBus的客户端-服务端通信架构，实现了数据传输和文件传输功能。