#include "DBusClient.h"

int main()
{
    DBusClient client;
    client.ConnectSystemBus();
    std::vector<LinkInfo> links;

    if (!client.ListLinks(links)) 
    {
        return 1;
    }

    std::cout << "共发现 " << links.size() << " 张网卡\n";

    for (const LinkInfo& link : links) 
    {
        std::cout << "\n网卡索引：" << link._index << '\n' << "网卡名称：" << link._name << '\n' 
                  << "对象路径：" << link._obj_path << '\n';
    }

    for (int i = 0; i < links.size(); ++i)
    {
        std::cout << "\n获取第 " << i + 1 << " 个网卡的属性\n";
        LinkProperties p;
        client.GetLinkProperties(links[i]._obj_path, p);

        std::cout << "_OperationalState: " << p._OperationalState 
                << "\n_CarrierState: " << p._CarrierState
                << "\n_AddressState: " << p._AddressState
                << "\n_IPv4AddressState: " << p._IPv4AddressState
                << "\n_IPv6AddressState: " << p._IPv6AddressState
                << "\n_OnlineState: " << p._OnlineState
                << "\n_AdministrativeState: " << p._AdministrativeState << std::endl;
    }
    
    return 0;
}