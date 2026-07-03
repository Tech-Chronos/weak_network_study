#include <iostream>
#include <cstring>

#include <unistd.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <linux/if_link.h>
#include <sys/socket.h>

#define BUFFER_SIZE 8192
#define NAME_BUFFER_SIZE 1024
#define MAC_BUFFER_SIZE 1024
#define MTU_BUFFER_SIZE 1024

typedef enum 
{
    SOCKET_ERROR,
    BIND_ERROR,
    RECV_ERROR,
} ERROR_TYPE; 

void get_interface_name(struct nlmsghdr* h, char* name_buffer, int name_buffer_size)
{
    struct ifinfomsg* ifi = (struct ifinfomsg*)NLMSG_DATA(h);

    int attr_len = IFLA_PAYLOAD(h);
    // 一个 ifinfomsg 中可能有多个 attributes
    for (struct rtattr* rta = (struct rtattr*)IFLA_RTA(ifi); RTA_OK(rta, attr_len); rta = RTA_NEXT(rta, attr_len))
    {
        if (rta->rta_type == IFLA_IFNAME)
        {
            snprintf(name_buffer, name_buffer_size, "%s", (const char*)RTA_DATA(rta));
            return;
        }
    }
    snprintf(name_buffer, name_buffer_size, "ifi_index = %d", ifi->ifi_index);
}

void handle_link_messages(struct nlmsghdr* h)
{
    struct ifinfomsg* ifi = (struct ifinfomsg*)NLMSG_DATA(h);

    char name_buffer[NAME_BUFFER_SIZE] = { 0 };
    get_interface_name(h, name_buffer, sizeof(name_buffer));

    bool admin_up = (ifi->ifi_flags & IFF_UP) != 0;
    bool lower_up = (ifi->ifi_flags & IFF_LOWER_UP) != 0;

    printf(
            "[LINK] NAME: %s, UP: %s, LOWER_UP: %s\n",
            name_buffer, 
            (admin_up == true) ? "UP" : "DOWN",
            (lower_up == true) ? "LOWER_UP" : "LOWER_DOWN"
        );
}

void handle_delete_link(struct nlmsghdr* h)
{
    struct ifinfomsg* ifi = (struct ifinfomsg*)NLMSG_DATA(h);

    char name_buffer[NAME_BUFFER_SIZE] = { 0 };
    get_interface_name(h, name_buffer, sizeof(name_buffer));

    printf("[DELETE] NAME: %s\n", name_buffer);
}

void get_link_msg(struct nlmsghdr* h)
{
    struct ifinfomsg* ifi = (struct ifinfomsg*)NLMSG_DATA(h);

    char name_buffer[NAME_BUFFER_SIZE] = { 0 };
    char mac_buffer[MAC_BUFFER_SIZE] = { 0 };
    char mtu_buffer[MTU_BUFFER_SIZE] = { 0 };

    int attr_len = IFLA_PAYLOAD(h);
    // 一个 ifinfomsg 中可能有多个 attributes
    for (struct rtattr* rta = (struct rtattr*)IFLA_RTA(ifi); RTA_OK(rta, attr_len); rta = RTA_NEXT(rta, attr_len))
    {
        if (rta->rta_type == IFLA_IFNAME)
        {
            snprintf(name_buffer, NAME_BUFFER_SIZE, "%s", (const char*)RTA_DATA(rta));
        }
        else if (rta->rta_type == IFLA_MTU)
        {
            snprintf(mtu_buffer, MTU_BUFFER_SIZE, "%s", (const char*)RTA_DATA(rta));
        }
        else if (rta->rta_type == IFLA_ADDRESS)
        {
            snprintf(mac_buffer, MAC_BUFFER_SIZE, "%s", (const char*)RTA_DATA(rta));
        }
    }

    bool admin_up = (ifi->ifi_flags & IFF_UP) != 0;
    bool lower_up = (ifi->ifi_flags & IFF_LOWER_UP) != 0;

    printf("%d: %s: <%s %s %s> mtu %s mac %s\n",
            ifi->ifi_index,
            name_buffer,
            "MULTICAST",
            (admin_up == true) ? "UP" : "DOWN",
            (lower_up == true) ? "LOWER_UP" : "LOWER_DOWN",
            mtu_buffer,
            mac_buffer
    );
}

void process_net_messages(char* buffer, int len)
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
                    std::cout << "[ACK] kernel reply succeeded!" << std::endl;
                }
                else 
                {
                    std::cerr << "[ERROR] net link message error!" << strerror(-err->error) << std::endl;
                }
                break;
            }
            case RTM_NEWLINK:
                //handle_link_messages(h);
                get_link_msg(h);
                break;
            case RTM_DELLINK:
                handle_delete_link(h);
                break;
            case RTM_GETLINK:
                get_link_msg(h);
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
        std::cerr << "socket error!" << strerror(errno) << std::endl;
        exit(SOCKET_ERROR);
    }

    struct sockaddr_nl local_addr;
    memset(&local_addr, 0, sizeof(local_addr));

    local_addr.nl_family = AF_NETLINK;
    local_addr.nl_groups = RTMGRP_LINK;
    local_addr.nl_pid = getpid();
    if (bind(fd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0)
    {
        std::cerr << "bind error!" << strerror(errno) << std::endl;
        exit(BIND_ERROR);
    }

    std::cout << "Listening for interface changes ..." << std::endl;
    std::cout << "Press Ctrl + c to interupt ..." << std::endl;

    char recv_buffer[BUFFER_SIZE] = { 0 };
    while (true)
    {
        int len = recv(fd, recv_buffer, sizeof(recv_buffer), 0);
        if (len < 0)
        {
            if (errno == EINTR || errno == EWOULDBLOCK)
            {
                continue;
            }
            else 
            {
                std::cerr << "recv error!" << strerror(errno) << std::endl;
                break;
            }
        }
        else if (len == 0)
        {
            std::cout << "kernel close net fd!" << std::endl;
            close(fd);
            break;
        }
        /// std::cout << "prepare process_net_messages..." << std::endl;
        process_net_messages(recv_buffer, len);
    }

    return 0;
}