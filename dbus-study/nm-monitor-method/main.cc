#include <iostream>
#include <dbus/dbus.h>

// a(iso)
void HandleReturnMessage(DBusMessage* reply)
{
    // 1. 初始化迭代器，并指向第一个元素
    DBusMessageIter iter;
    dbus_bool_t b = dbus_message_iter_init(reply, &iter);
    if (!b)
    {
        std::cerr << "消息体中没有消息" << std::endl;
        return;
    }

    // 2. 判断第一个元素类型
    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY)
    {
        std::cerr << "不是 array 类型" << std::endl;
        return;
    }

    // 3. 进入数组
    DBusMessageIter arrayIter;
    dbus_message_iter_recurse(&iter, &arrayIter);
    while (dbus_message_iter_get_arg_type(&arrayIter) != DBUS_TYPE_INVALID)
    {
        // 4. 进入结构体
        DBusMessageIter structIter;
        dbus_message_iter_recurse(&arrayIter, &structIter);
        if (dbus_message_iter_get_arg_type(&structIter) != DBUS_TYPE_INT32)
        {
            std::cerr << "结构体中第一个元素不是 int32 类型" << std::endl;
            dbus_message_iter_next(&arrayIter);
            continue;
        }

        // 5. 获取第一个元素的 value，移动到下一个元素
        int32_t index = 0;
        dbus_message_iter_get_basic(&structIter, &index);
        dbus_message_iter_next(&structIter);

        // 6. 检查下一个元素类型
        if (dbus_message_iter_get_arg_type(&structIter) != DBUS_TYPE_STRING)
        {
            std::cerr << "结构体中第二个元素不是 string 类型" << std::endl;
            dbus_message_iter_next(&arrayIter);
            continue;
        }

        // 7. 获取第二个元素 value，移动到下一个元素
        const char* name = nullptr;
        dbus_message_iter_get_basic(&structIter, &name);
        dbus_message_iter_next(&structIter);

        // 8. 检查下一个元素类型
        if (dbus_message_iter_get_arg_type(&structIter) != DBUS_TYPE_OBJECT_PATH)
        {
            std::cerr << "结构体中第三个元素不是 object path 类型" << std::endl;
            dbus_message_iter_next(&arrayIter);
            continue;
        }

        // 9. 获取第三个元素 value
        const char* obj_path = nullptr;
        dbus_message_iter_get_basic(&structIter, &obj_path);

        std::cout << index << " " << name << " " << obj_path << std::endl;

        // 移动到数组下一个元素
        dbus_message_iter_next(&arrayIter);
    }
    
}

int main()
{
    // 1. 连接 dbus
    DBusError error;
    dbus_error_init(&error);
    DBusConnection* conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
    if (dbus_error_is_set(&error))
    {
        std::cerr << "链接 system bus 失败, " << error.message << std::endl;

        dbus_error_free(&error);
        return -1;
    }
    if (conn == nullptr)
    {
        std::cerr << "链接 system bus 失败, conn 是 空指针" << std::endl;
        return -1;
    }

    // 2. dbus 的系统总线中的方法
    const char* bus_name = "org.freedesktop.network1";
    const char* path = "/org/freedesktop/network1";
    const char* interface = "org.freedesktop.network1.Manager";
    const char* method = "ListLinks";
    DBusMessage* message = dbus_message_new_method_call(bus_name, path, interface, method);
    if (message == nullptr)
    {
        std::cerr << "创建 method call 失败 \n";
        return 1;
    }
    std::cout << "创建 method call 成功 \n";

    // 3. 将 method 发送到 dbus 的守护进程中
    DBusPendingCall* pending = nullptr;
    const dbus_bool_t b = dbus_connection_send_with_reply(conn, message, &pending, 3000);
    if (!b)
    {
        dbus_message_unref(message);
        std::cerr << "发送 method call 到 dbus 错误" << std::endl;
        return 1;
    }

    dbus_message_unref(message);
    std::cout << "发送 method call 到 dbus 守护进程成功\n";

    // 4. 开始阻塞等待
    dbus_pending_call_block(pending);

    // 5. 等待完成
    DBusMessage* reply = dbus_pending_call_steal_reply(pending);
    if(reply == nullptr)
    {
        std::cerr <<"没有收到回复\n";
        return 1;
    }
    dbus_pending_call_unref(pending);

    HandleReturnMessage(reply);
    // while (true)
    // {
    //     const dbus_bool_t b = dbus_connection_read_write_dispatch(conn, 3000);
    //     if (!b)
    //     {
    //         std::cerr << "连接断开" << std::endl;
    //         return 1;
    //     }
    // }
    return 0;
}