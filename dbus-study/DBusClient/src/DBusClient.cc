#include "DBusClient.h"

DBusClient::DBusClient()
    :_conn(nullptr)
{}

DBusClient::~DBusClient()
{
    if (_conn != nullptr)
    {
        dbus_connection_unref(_conn);
        _conn = nullptr;
    }
}

bool DBusClient::ConnectSystemBus()
{
    DBusError error;
    dbus_error_init(&error);

    _conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);

    if (dbus_error_is_set(&error)) 
    {
        std::cerr << "连接 System Bus 失败：" << error.message << '\n';
        dbus_error_free(&error);
        _conn = nullptr;
        return false;
    }

    if (_conn == nullptr) 
    {
        std::cerr << "连接 System Bus 失败：" << "connection 为空\n";
        return false;
    }
    std::cout << "成功连接 System Bus\n";
    return true;
}

bool DBusClient::ListLinks(std::vector<LinkInfo>& links)
{
    // 1. 创建本地 method call
    const char* bus_name = "org.freedesktop.network1";
    const char* path = "/org/freedesktop/network1";
    const char* interface = "org.freedesktop.network1.Manager";
    const char* method = "ListLinks";

    DBusMessage* message = dbus_message_new_method_call(bus_name, path, interface, method);
    if (message == nullptr)
    {
        std::cerr << "本地创建 method call 出错" << std::endl;
        return false;
    }
    std::cout << "本地创建 method call 成功" << std::endl;
    // 2. 将 method call 发送到 dbus 守护进程
    DBusPendingCall* pending = nullptr;
    const dbus_bool_t b = dbus_connection_send_with_reply(_conn, message, &pending, 3000);
    if (!b)
    {
        std::cerr << "将 method call 发送到 dbus 守护进程 出错" << std::endl;
        return false;
    }
    std::cout << "将 method call 发送到 dbus 守护进程 成功" << std::endl; 
    // 消息交给了 dbus，自己引用计数 -1
    dbus_message_unref(message);

    // 3. 等待 reply -> pending 中有数据到来
    dbus_pending_call_block(pending);

    // 4. 从 pending 中拿出 reply
    std::cout << "有消息到来" << std::endl;
    DBusMessage* reply = dbus_pending_call_steal_reply(pending);
    if (reply == nullptr)
    {
        std::cerr << "消息是空的" << std::endl;
        return false;
    }
    else if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR)
    {
        const char* error_name = dbus_message_get_error_name(message);
        std::cerr   << "消息类型出错，错误类型是：" 
                    << ((error_name == nullptr) ? "(unknown)" : error_name) 
                    << std::endl;
        return false;
    }
    else if (dbus_message_get_type(reply) != DBUS_MESSAGE_TYPE_METHOD_RETURN)
    {
        std::cerr << "不是 method call 的 回复消息" << std::endl;
        return false;
    }
    std::cout << "消息类型正确，准备处理" << std::endl;
    // pending 用完，引用计数 -1
    dbus_pending_call_unref(pending);

    // 处理 reply -> a(iso)
    DBusMessageIter rootIter;
    if (!dbus_message_iter_init(reply, &rootIter))
    {
        std::cerr << "回复中没有消息" << std::endl;
        return false;
    }
    // 1. 检查类型
    if (dbus_message_iter_get_arg_type(&rootIter) != DBUS_TYPE_ARRAY)
    {
        std::cerr << "消息不是 array 类型" << std::endl;
        return false;
    }

    // 2. 进入数组
    DBusMessageIter arrayIter;
    dbus_message_iter_recurse(&rootIter, &arrayIter);
    while (dbus_message_iter_get_arg_type(&arrayIter) != DBUS_TYPE_INVALID)
    {
        // 3. 检查每个元素的类型是否是 struct
        if (dbus_message_iter_get_arg_type(&arrayIter) != DBUS_TYPE_STRUCT)
        {
            std::cerr << "消息不是 struct 类型" << std::endl;
            dbus_message_iter_next(&arrayIter);
            continue;
        }
        // 4. 进入结构体
        DBusMessageIter structIter;
        dbus_message_iter_recurse(&arrayIter, &structIter);

        // 5. 依次遍历结构体中的元素
        // 第一个参数
        if(dbus_message_iter_get_arg_type(&structIter) != DBUS_TYPE_INT32)
        {
            std::cerr << "struct 中的第 1 个元素不是 int 类型" << std::endl;
            dbus_message_iter_next(&arrayIter);
            continue;
        }
        int index = 0;
        dbus_message_iter_get_basic(&structIter, &index);
        dbus_message_iter_next(&structIter);

        // 第二个参数
        if(dbus_message_iter_get_arg_type(&structIter) != DBUS_TYPE_STRING)
        {
            std::cerr << "struct 中的第 2 个元素不是 string 类型" << std::endl;
            dbus_message_iter_next(&arrayIter);
            continue;
        }
        const char* name;
        dbus_message_iter_get_basic(&structIter, &name);
        dbus_message_iter_next(&structIter);

        // 第三个参数
        if(dbus_message_iter_get_arg_type(&structIter) != DBUS_TYPE_OBJECT_PATH)
        {
            std::cerr << "struct 中的第 3 个元素不是 object path 类型" << std::endl;
            dbus_message_iter_next(&arrayIter);
            continue;
        }
        const char* obj_path;
        dbus_message_iter_get_basic(&structIter, &obj_path);
        dbus_message_iter_next(&structIter);

        // 6. 在 links 中插入
        links.push_back({index, name, obj_path});

        // 7. 移动到下一个数组元素
        dbus_message_iter_next(&arrayIter);
    }

    return true;
}

bool DBusClient::GetLinkProperties(const std::string& link_path, LinkProperties& properties)
{
    // 1. 本地创建 method call
    const char* bus_name = "org.freedesktop.network1";
    const char* interface = "org.freedesktop.DBus.Properties";
    const char* method = "GetAll";
    DBusMessage* message = dbus_message_new_method_call(bus_name, link_path.c_str(), interface, method);
    if (message == nullptr)
    {
        std::cerr << "本地创建 method call 失败" << std::endl;
        return false;
    }

    std::cout << "本地创建 method call 成功" << std::endl;

    // 2. 在 method call 中加入 interface 参数
    DBusMessageIter in_iter;
    dbus_message_iter_init_append(message, &in_iter);
    const char* args_interface = "org.freedesktop.network1.Link";
    dbus_message_iter_append_basic(&in_iter, DBUS_TYPE_STRING, &args_interface);

    // 3. 发送到 dbus 守护进程
    DBusPendingCall* pending = nullptr;
    dbus_bool_t b = dbus_connection_send_with_reply(_conn, message, &pending, 3000);
    if (!b)
    {
        std::cerr << "发送 method call 消息失败\n";
        return false;
    }
    std::cout << "发送 method call 消息成功\n";
    dbus_message_unref(message);

    // 4. 等待 pending 有数据
    dbus_pending_call_block(pending);
    
    // 5. 获取消息
    DBusMessage* reply = dbus_pending_call_steal_reply(pending);
    if (reply == nullptr)
    {
        std::cerr << "没有数据到来\n";
        return false;
    }
    int type = dbus_message_get_type(reply);
    if (type == DBUS_MESSAGE_TYPE_ERROR)
    {
        const char* errorName = dbus_message_get_error_name(reply);

        std::cerr
            << "GetAll 返回 D-Bus Error："
            << (errorName != nullptr
                    ? errorName
                    : "(unknown)")
            << '\n';
        dbus_message_unref(reply);
    }
    std::cout << "有消息到来，准备处理" << std::endl;
    dbus_pending_call_unref(pending);

    // 处理消息 a{sv}
    // 1. 初始化迭代器
    DBusMessageIter iter;
    if (!dbus_message_iter_init(reply, &iter))
    {
        std::cerr << "消息体中没有消息" << std::endl;
        return false;
    }

    // 2. 判断类型
    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY)
    {
        std::cerr << "这个消息不是数组类型" << std::endl;
        return false;
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
        PrintVariantValue(key, &valIter, properties);

        // 10. 移动到下一个数组元素
        dbus_message_iter_next(&arrayIter);
    }
    return true;
}


// private
void DBusClient::PrintVariantValue(const char* key, DBusMessageIter* valIter, LinkProperties& property)
{
    const int valueType = dbus_message_iter_get_arg_type(valIter);
    
    if (valueType == DBUS_TYPE_STRING)
    {
        const char* strVal = nullptr;
        dbus_message_iter_get_basic(valIter, &strVal);
        if (strcmp(key, "OperationalState") == 0)
        {
            property._OperationalState = strVal;
        }
        else if (strcmp(key, "CarrierState") == 0)
        {
            property._CarrierState = strVal;
        }
        else if (strcmp(key, "AddressState") == 0)
        {
            property._AddressState = strVal;
        }
        else if (strcmp(key, "IPv4AddressState") == 0)
        {
            property._IPv4AddressState = strVal;
        }
        else if (strcmp(key, "IPv6AddressState") == 0)
        {
            property._IPv6AddressState = strVal;
        }
        else if (strcmp(key, "OnlineState") == 0)
        {
            property._OnlineState = strVal;
        }
        else if (strcmp(key, "AdministrativeState") == 0)
        {
            property._AdministrativeState = strVal;
        }
        return;
    }

    // if (valueType == DBUS_TYPE_INT32)
    // {
    //     dbus_int32_t intVal = 0;
    //     dbus_message_iter_get_basic(valIter, &intVal);
    //     std::cout << key << " " << intVal << std::endl;
    //     return;
    // }

    // if (valueType == DBUS_TYPE_UINT64)
    // {
    //     dbus_int64_t intVal = 0;
    //     dbus_message_iter_get_basic(valIter, &intVal);
    //     std::cout << key << " " << intVal << std::endl;
    //     return;
    // }

    std::cout << key << " [unsupported variant type: "
              << static_cast<char>(valueType) << "]" << std::endl;
}

// 不能把 类的成员函数注册给 filter，因为有 this 指针，通过 user_data 强转即可获取
DBusHandlerResult DBusClient::MessageFilter(DBusConnection* conn, DBusMessage* message, void* user_data)
{
    if (message == nullptr)
    {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    DBusClient* client = static_cast<DBusClient*>(user_data);
    return client->HandlerMessage(message);
}

DBusHandlerResult DBusClient::HandlerMessage(DBusMessage* message)
{
    if (message == nullptr)
    {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    const char* interface = "org.freedesktop.DBus.Properties";
    const char* signal_name = "PropertiesChanged";

    if (!dbus_message_is_signal(message, interface, signal_name))
    {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    const char* path = dbus_message_get_path(message);

    std::cout << "收到 PropertiesChanged 信号，" << "obj_path: " << path << '\n';
    return DBUS_HANDLER_RESULT_HANDLED;
}