#pragma once
#include "ITestService.h"
#include <gio/gio.h>

class DBusAdapter {
public:
    explicit DBusAdapter(ITestService* service);
    ~DBusAdapter();

    bool init();
    void runLoop();
    ITestService* getTestService() const { return test_service_; }

    // D-Bus信号广播接口声明
    void emitTestBoolChanged(bool value);
    void emitTestIntChanged(int value);
    void emitTestDoubleChanged(double value);
    void emitTestStringChanged(const std::string& value);
    void emitTestInfoChanged(const TestInfo& info);
private:
    ITestService* test_service_;
    GMainLoop* main_loop_ = nullptr;
    GDBusNodeInfo* introspection_data_;
    guint registration_id_;
    guint name_owner_id_;
    GDBusConnection* connection_;

    static void handle_method_call(GDBusConnection* connection,
                                  const gchar* sender,
                                  const gchar* object_path,
                                  const gchar* interface_name,
                                  const gchar* method_name,
                                  GVariant* parameters,
                                  GDBusMethodInvocation* invocation,
                                  gpointer user_data);
};