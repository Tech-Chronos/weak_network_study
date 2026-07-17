#include <iostream>
#include <string>
#include <dbus/dbus.h>
#include <cstring>
#include <csignal>

volatile sig_atomic_t running = 1;

void SignalHandler(int signal)
{
    running = 0;
}

std::string messagetypeToString(int type)
{
    switch(type)
    {
        case DBUS_MESSAGE_TYPE_METHOD_CALL:
            return "method_call";
        case DBUS_MESSAGE_TYPE_METHOD_RETURN:
            return "method_return";
        case DBUS_MESSAGE_TYPE_ERROR:
            return "error";
        case DBUS_MESSAGE_TYPE_SIGNAL:
            return "signal";
    }
    return "unknown";
}

DBusHandlerResult messageFilter(DBusConnection* conn, DBusMessage* message, void* user_data)
{
    (void*)conn;
    (void*) user_data;

    std::string need_inter = "org.freedesktop.DBus.Properties";
    std::string need_member = "PropertiesChanged";
    const bool sig = dbus_message_is_signal(message, need_inter.c_str(), need_member.c_str());

    if (!sig)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    
    std::string type = messagetypeToString(dbus_message_get_type(message));
    std::string sender = dbus_message_get_sender(message);
    std::string path = dbus_message_get_path(message);
    std::string interface = dbus_message_get_interface(message);
    std::string member = dbus_message_get_member(message);
    std::string signature = dbus_message_get_signature(message);

    /*
     * 打印消息头信息。
     */
    std::cout
        << "\n收到一条 PropertiesChanged 信号\n"
        << "消息类型："
        << type
        << '\n'
        << "发送者："
        << sender
        << '\n'
        << "对象路径："
        << path 
        << '\n'
        << "接口："
        << interface
        << '\n'
        << "成员："
        <<  member
        << '\n'
        << "参数签名："
        << signature
        << '\n';

    /*
     * 打印消息体信息。
     */
    // 1. 初始化迭代器
    DBusMessageIter iter;
    if (!dbus_message_iter_init(message, &iter))
    {
        std::cerr << "消息体中没有消息" << std::endl;
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    // 2. 判断迭代器指向的第一个消息的类型，一般情况下是变化的接口，是字符串类型
    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
    {
        std::cerr << "第一个参数不是字符串类型" << std::endl;
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    // 3. 获取消息的内容
    const char* changedInterface = nullptr;
    dbus_message_iter_get_basic(&iter, &changedInterface);

    std::cout   \
        << "\n变化的接口是：" 
        << ((changedInterface == nullptr) ? "(nullptr)" : changedInterface) 
        << std::endl;

    // 继续向后遍历，第二个参数应该是 a{sv} -> map{string, variant}
    if (!dbus_message_iter_next(&iter))
    {
        std::cerr << "PropertiesChanged 缺少第二个参数" << std::endl;
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY)
    {
        std::cerr << "PropertiesChanged 第二个参数不是数组类型！" << std::endl; 
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    // 重新定义一个数组迭代器，让 arrayIter 进入数组， iter 指向整个数组 
    DBusMessageIter arrayIter;
    dbus_message_iter_recurse(&iter, &arrayIter);
    while (dbus_message_iter_get_arg_type(&arrayIter) != DBUS_TYPE_INVALID)
    {
        // 每个元素应该是 {sv}，如果不是字典类型，直接跳到下一个
        if (dbus_message_iter_get_arg_type(&arrayIter) != DBUS_TYPE_DICT_ENTRY)
        {
            dbus_message_iter_next(&arrayIter);
            continue;
        }
        // 定义字典迭代器，进入到字典中
        DBusMessageIter dictIter;
        dbus_message_iter_recurse(&arrayIter, &dictIter);

        // 字典中的第一个应该是 string 类型，不是直接跳到下一个
        if (dbus_message_iter_get_arg_type(&dictIter) != DBUS_TYPE_STRING)
        {
            dbus_message_iter_next(&arrayIter);
            continue;
        }

        // 到这里说明字典中的第一个是string类型，解析变化的属性名称
        const char* propertyName = nullptr;
        dbus_message_iter_get_basic(&dictIter, &propertyName);

        // 字典中的第二个是 variant 类型，移动 dictIter 迭代器
        if(!dbus_message_iter_next(&dictIter)) // 指向 variant
        {
            std::cerr << propertyName << " 缺少属性值" << std::endl;
            dbus_message_iter_next(&arrayIter);
            continue;
        }

        // 检查 value 类型是否是 variant
        if (dbus_message_iter_get_arg_type(&dictIter) != DBUS_TYPE_VARIANT)
        {
            std::cerr << propertyName << " 的属性值类型不是 variant" << std::endl;
            dbus_message_iter_next(&arrayIter);
            continue;
        }

        // variant 是个包装，也需要 recurse 进去
        DBusMessageIter valueIter;
        dbus_message_iter_recurse(&dictIter, &valueIter);
        
        const char* valStr = nullptr;
        const int* valInt = nullptr;
        if (dbus_message_iter_get_arg_type(&valueIter) == DBUS_TYPE_STRING)
        {
            dbus_message_iter_get_basic(&valueIter, &valStr);
        }
        else if (dbus_message_iter_get_arg_type(&valueIter) == DBUS_TYPE_UINT32)
        {
            dbus_message_iter_get_basic(&valueIter, &valInt);
        }

        std::cout 
            << "变化的属性名称：" 
            << ((propertyName != nullptr) ? propertyName : "(nullptr)") << ": "
            << ((valStr != nullptr) ? valStr : "")
            << ((valInt != nullptr) ? *valInt : NULL)
            << std::endl;

        dbus_message_iter_next(&arrayIter);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

int main()
{
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    // 1. 先准备一个接受错误消息的对象
    DBusError error;

    // 2. 初始化错误对象
    dbus_error_init(&error);

    // 3. 链接 system bus
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

    std::cout << "链接 system bus 成功" << std::endl;

    // 在 dbus daemon 中添加匹配规则
    // 添加注册规则，不能有空格 dbus_bus_add_match()
    // 当org.freedesktop.network1这个服务中有
    // 接口是org.freedesktop.Dbus.Properties发送的
    // PropertiesChanged信号，我就会收到消息
    const char* rule = 
        "type='signal',"
        "sender='org.freedesktop.network1',"
        "interface='org.freedesktop.DBus.Properties',"
        "member='PropertiesChanged'";

    dbus_bus_add_match(conn, rule, &error);

    if (dbus_error_is_set(&error))
    {
        std::cerr << "注册匹配规则失败！" << error.message << std::endl;

        dbus_error_free(&error);
        dbus_connection_unref(conn);

        return -1;
    }

    std::cout << "注册匹配规则成功！" << std::endl;

    if(!dbus_connection_add_filter(conn, messageFilter, nullptr, nullptr))
    {
        std::cerr << "安装消息过滤器失败\n";

        dbus_bus_remove_match(conn,rule,nullptr);
        dbus_connection_unref(conn);
        return 1;
    }

    std::cout << "成功安装消息过滤器\n";

    std::cout << "开始等待 D-Bus 消息……\n" << "按 Ctrl+C 结束程序\n";

    while(running)
    {
        const bool b = dbus_connection_read_write_dispatch(conn, 1000);
        // 如果返回的是 false，说明这个连接已经断开了
        if (!b)
        {
            std::cerr << "D-Bus 连接已经断开\n";
            break;
        }
        
    }

    // 回收资源
    dbus_connection_remove_filter(conn, messageFilter, nullptr);
    dbus_bus_remove_match(conn, rule, nullptr);
    dbus_connection_unref(conn);
}
