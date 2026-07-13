#include <iostream>
#include <string>
#include <dbus/dbus.h>
#include <cstring>
#include <csignal>

DBusHandlerResult messageFilter(DBusConnection* conn, DBusMessage* message, void* user_data)
{
    (void*)conn;
    (void*) user_data;

    std::string inter = "org.freedesktop.DBus.Properties";
    std::string member = "PropertiesChanged";
    const bool sig = dbus_message_is_signal(message, inter.c_str(), member.c_str());

    if (!sig)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    
    std::cout << "receive signal PropertiesChanged succeed!" << std::endl;
    return DBUS_HANDLER_RESULT_HANDLED;
}

int main()
{
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
    // 接口是org.freedesktop.DBUS.Properties发送的
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
    while(true)
    {
        const bool b = dbus_connection_read_write_dispatch(conn, 1000);
        if (!b)
        {
            std::cerr << "D-Bus 连接已经断开\n";
            break;
        }
        
    }

    dbus_connection_remove_filter(conn, messageFilter, nullptr);
    dbus_bus_remove_match(conn, rule, nullptr);
    // 用完链接后进行释放
    dbus_connection_unref(conn);
    return 0;
}
