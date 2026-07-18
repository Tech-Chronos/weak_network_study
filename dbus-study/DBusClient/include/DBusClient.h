#ifndef __DBUS_CLIENT__H
#define __DBUS_CLIENT__H
#include <iostream>
#include <vector>
#include <string>

#include <cstring>
#include <dbus/dbus.h>

struct LinkInfo
{
    LinkInfo(int index = 0, std::string name = "", std::string obj_path = "")
        : _index(index)
        , _name(name)
        , _obj_path(obj_path)
    {}

    int _index;
    std::string _name;
    std::string _obj_path;
};

struct LinkProperties
{
    std::string _OperationalState;
    std::string _CarrierState;
    std::string _AddressState;
    std::string _IPv4AddressState;
    std::string _IPv6AddressState;
    std::string _OnlineState;
    std::string _AdministrativeState;
};

class DBusClient
{
private:
    void PrintVariantValue(const char* key, DBusMessageIter* valIter, LinkProperties& property);
    static DBusHandlerResult MessageFilter(DBusConnection*, DBusMessage*, void*);
    DBusHandlerResult HandlerMessage(DBusMessage*);
public:
    DBusClient();   
    ~DBusClient();

    // 连接系统总线
    bool ConnectSystemBus();
    bool ListLinks(std::vector<LinkInfo>& links);

    // 本地创建 method call，将 method call 发送到 dbus 守护进程
    bool GetLinkProperties(const std::string& link_path, LinkProperties& properties); 
private:
    DBusConnection* _conn;
};

#endif