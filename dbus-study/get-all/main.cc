#include <iostream>
#include <dbus/dbus.h>

void PrintVariantValue(const char* key, DBusMessageIter* valIter)
{
    const int valueType = dbus_message_iter_get_arg_type(valIter);

    if (valueType == DBUS_TYPE_STRING)
    {
        const char* strVal = nullptr;
        dbus_message_iter_get_basic(valIter, &strVal);
        std::cout << key << " " << strVal << std::endl;
        return;
    }

    if (valueType == DBUS_TYPE_INT32)
    {
        dbus_int32_t intVal = 0;
        dbus_message_iter_get_basic(valIter, &intVal);
        std::cout << key << " " << intVal << std::endl;
        return;
    }

    std::cout << key << " [unsupported variant type: "
              << static_cast<char>(valueType) << "]" << std::endl;
}

// a{sv}
void HandlerReply(DBusMessage* reply)
{
    // 1. 初始化迭代器
    DBusMessageIter iter;
    const dbus_bool_t b = dbus_message_iter_init(reply, &iter);
    if (!b)
    {
        std::cerr << "消息体中没有消息" << std::endl;
        return;
    }

    // 2. 判断类型
    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY)
    {
        std::cerr << "这个消息不是数组类型" << std::endl;
        return;
    }

    // 3. 进入数组
    DBusMessageIter arrayIter;
    dbus_message_iter_recurse(&iter, &arrayIter);
    while (dbus_message_iter_get_arg_type(&arrayIter) != DBUS_TYPE_INVALID)
    {
        // 检查类型
        if (dbus_message_iter_get_arg_type(&arrayIter) != DBUS_TYPE_DICT_ENTRY)
        {
            std::cerr << "这个消息不是字典类型" << std::endl;
            dbus_message_iter_next(&arrayIter);
            continue;
        }
        // 4. 进入 map
        DBusMessageIter dictIter;
        dbus_message_iter_recurse(&arrayIter, &dictIter);
        
        // 5. 判断字典中第一个元素的类型
        if (dbus_message_iter_get_arg_type(&dictIter) != DBUS_TYPE_STRING)
        {
            std::cerr << "这个字典中的第一个参数不是 string 类型" << std::endl;
            dbus_message_iter_next(&arrayIter);
            continue;
        }

        // 6. 获取字典中的第一个元素，移动字典迭代器
        const char* key = nullptr;
        dbus_message_iter_get_basic(&dictIter, &key);
        dbus_message_iter_next(&dictIter);

        // 7. 判断字典中第二个元素的类型 是否是 variant
        if (dbus_message_iter_get_arg_type(&dictIter) != DBUS_TYPE_VARIANT)
        {
            std::cerr << "这个字典中的第一个参数不是 variant 类型" << std::endl;
            dbus_message_iter_next(&arrayIter);
            continue;
        }

        // 8. 进入到 variant
        DBusMessageIter valIter;
        dbus_message_iter_recurse(&dictIter, &valIter);

        // 9. 按 variant 里的真实类型输出
        PrintVariantValue(key, &valIter);

        // 10. 移动到下一个数组元素
        dbus_message_iter_next(&arrayIter);
    }
}

int main()
{
    // 1. 建立 dbus 连接
    DBusError error;
    dbus_error_init(&error);
    DBusConnection* conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
    if (dbus_error_is_set(&error))
    {
        std::cerr << "建立连接出错！" << std::endl;
        return 1;
    }
    std::cout << "连接建立成功！" << std::endl;

    // 2. 本地创建 method call
    const char* bus_name = "org.freedesktop.network1";
    const char* obj_path = "/org/freedesktop/network1/link/_31";
    const char* interface = "org.freedesktop.DBus.Properties";
    const char* method = "GetAll";

    DBusMessage* local = dbus_message_new_method_call(bus_name, obj_path, interface, method);
    if (local == nullptr)
    {
        std::cerr << "本地创建 method call 错误\n";
        return 1;
    }
    std::cout << "本地创建 method call 成功\n";

    // 在message中写入数据（GetAll 的参数）
    DBusMessageIter args;
    dbus_message_iter_init_append(local, &args);

    const char* interface_args = "org.freedesktop.network1.Link";
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &interface_args);

    // 3. 将创建的 method call 发送到 dbus 的守护进程
    DBusPendingCall* pending = nullptr;
    const dbus_bool_t b = dbus_connection_send_with_reply(conn, local, &pending, 3000);
    if (!b)
    {
        dbus_message_unref(local);
        std::cerr << "将 method call 发送到 dbus 的守护进程错误" << std::endl;
        return 1;
    }
    std::cout << "将 method call 发送到 dbus 的守护进程 成功" << std::endl;
    // 消息交给 libdbus 了，引用计数 -1
    dbus_message_unref(local);

    // 4. 开始等待
    dbus_pending_call_block(pending);

    // 5. pending 变成 completed 状态，拿出 message
    DBusMessage* reply = dbus_pending_call_steal_reply(pending);
    if (!reply)
    {
        std::cerr << "回复中没有消息" << std::endl;
        return 1;
    }
    std::cout << "收到消息，开始解析" << std::endl;

    // 6. 处理消息
    HandlerReply(reply);

    // 7. 连接引用计数 -1
    dbus_connection_unref(conn);
    return 0;   
}
