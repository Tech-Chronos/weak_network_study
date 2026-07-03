#include <iostream>
#include <dbus/dbus.h>
#include <cstring>
#include <csignal>

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
    }
    if (conn == nullptr)
    {
        std::cerr << "链接 system bus 失败, conn 是 空指针" << std::endl;
    }

    std::cout << "链接 system bus 成功" << std::endl;

    // 用完链接后进行释放
    dbus_connection_unref(conn);
    return 0;
}
