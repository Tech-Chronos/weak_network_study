#include <cerrno>
#include <cstring>
#include <iostream>
#include <linux/if_link.h>
#include <linux/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 8192
#define NAME_BUFFER_SIZE 4096

/*
 * 从 RTM_NEWLINK / RTM_DELLINK 消息中解析网卡名称。
 *
 * Netlink 消息结构：
 * nlmsghdr
 *   └── ifinfomsg
 *         └── 一系列 rtattr 属性
 *
 * 网卡名称位于 IFLA_IFNAME 属性中。
 */
void get_interface_name(struct nlmsghdr* h, char* name_buffer, int len)
{
    // 根据头部字段获取网络接口相关信息
    struct ifinfomsg* ifi = (struct ifinfomsg*)NLMSG_DATA(h);
    // 通过网络接口获取属性的字节数
    int attr_len = IFLA_PAYLOAD(h);
    // 获取第一个属性的 rtaddr
    for (struct rtattr* attr = (struct rtattr*)IFLA_RTA(ifi); RTA_OK(attr, attr_len); attr = RTA_NEXT(attr, attr_len))
    {
        if (attr->rta_type == IFLA_IFNAME)
        {
            snprintf(name_buffer, len, "%s", (const char*)RTA_DATA(attr));
            return;
        }
    }

    snprintf(name_buffer, len, "ifi_index = %d", ifi->ifi_index);
}

/*
    处理一条网卡的相关 Netlink 信息
*/
void handle_link_messages(struct nlmsghdr* h)
{
    // 接口相关信息
    struct ifinfomsg* ifi = (struct ifinfomsg*)NLMSG_DATA(h);

    char name_buffer[NAME_BUFFER_SIZE] = { 0 }; 
    get_interface_name(h, name_buffer, sizeof(name_buffer));
    bool admin_up = ((ifi->ifi_flags & IFF_UP) != 0);

    bool lower_up = ((ifi->ifi_flags & IFF_LOWER_UP) != 0);

    printf(
        "[LINK] interface=%s index=%d admin=%s lower=%s\n",
        name_buffer,
        ifi->ifi_index,
        admin_up ? "UP" : "DOWN",
        lower_up ? "UP" : "DOWN");
}

/*
 * 遍历 recv() 返回的缓冲区。
 *
 * 一次 recv() 可能包含多条 Netlink 消息，
 * 所以不能只处理第一条。
*/
void process_link_messages(char* buffer, int len)
{
    for (struct nlmsghdr* h = (struct nlmsghdr*)buffer; NLMSG_OK(h, len); h = NLMSG_NEXT(h, len))
    {
        switch(h->nlmsg_type)
        {
            case NLMSG_DONE:
                return;
            case NLMSG_ERROR:
            {
                struct nlmsgerr* err = (struct nlmsgerr*)NLMSG_DATA(h);
                if (err->error == 0)
                {
                    std::cout << "[ACK] Netlink Request Succeeded!" << std::endl;
                }
                else
                {
                    std::cerr << "Netlink message error, " << strerror(-err->error) << std::endl;
                }
                break;
            }
            case RTM_NEWLINK:
            case RTM_DELLINK:
                handle_link_messages(h);
                break;
            default:
                break;
        }
    }
}

int main()
{
    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd < 0)
    {
        std::cerr << "socket error: " << strerror(errno) << std::endl;
        exit(-1);
    }

    struct sockaddr_nl local;
    bzero(&local, sizeof(local));
    local.nl_family = AF_NETLINK;
    local.nl_groups = RTMGRP_LINK;
    local.nl_pid = getpid();

    if (bind(fd, (struct sockaddr*)&local, sizeof(local)) < 0)
    {
        std::cerr << "bind error: " << strerror(errno) << std::endl;
        close(fd);
        exit(-1);
    }

    std::cout << "Listening for interface changes ..." << std::endl;
    std::cout << "Press Ctrl + c to interupt ..." << std::endl;

    char buffer[BUFFER_SIZE] = { 0 };

    while (true)
    {
        int len = recv(fd, buffer, sizeof(buffer), 0);
        if (len < 0)
        {
            if (errno == EINTR || errno == EWOULDBLOCK)
            {
                continue;
            }
            std::cerr << "recv error: " << strerror(errno) << std::endl;
            close(fd);
            break;
        }
        else if (len == 0)
        {
            std::cout << "kernel close socket ..." << std::endl;
            close(fd);
            break;
        }

        process_link_messages(buffer, len);
    }

    close(fd);
    return 0;
}
