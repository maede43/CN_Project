#define _GNU_SOURCE
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <search.h>
#include <linux/limits.h>

// String length for IP and IP/CIDR
#define IPSTR_ADDR_LEN 16
#define IPCIDRSTR_ADDR_LEN 20
#define MACSTR_ADDR_LEN 18
#define LINE_LEN 200

#define IP_ADDR_LEN 4

// Configuration flags
#define PRINT_REQ 0x02
#define VERBOSE 0x04

void usage();
// Incremente a 32 bit integer in network byte order
uint32_t inc_netorder(uint32_t value);
// Convert MAC/IP in array to string
void macarray_to_str(uint8_t array[ETHER_ADDR_LEN], char dest[MACSTR_ADDR_LEN]);
void iparray_to_str(uint8_t array[IP_ADDR_LEN], char dest[IPSTR_ADDR_LEN]);
// Split a string IP/CIDR into network address and IP count
int split_cidr_range(const char *target, struct in_addr *ip_range, uint32_t *ip_count);
// Find MAC addresses for <target> range using <interface_index>, <mac>, and <ip>
int getMACs(int fd, int interface_index, char mac[ETHER_ADDR_LEN], char *ip, char *target, int flags, struct hsearch_data *htab);

int main(int argc, char **argv)
{
    int c, fd = 0, flags = 0, matches = 0;
    struct timeval tv;
    char target[IPCIDRSTR_ADDR_LEN] = "";
    char interface_name[IFNAMSIZ] = "";
    int interface_index;
    unsigned char interface_mac[ETHER_ADDR_LEN];
    char interface_ip[IPSTR_ADDR_LEN];
    struct ifreq ifr;
    struct hsearch_data mac_list_hash;
    char *mac_list = NULL;

    // Default socket timeout .950 seconds
    tv.tv_sec = 0;
    tv.tv_usec = 950000;

    // Read arguments
    opterr = 0;

    flags |= VERBOSE;
    flags |= PRINT_REQ;
    while ((c = getopt(argc, argv, "hr:t:")) != -1)
        switch (c)
        {
        // help
        case 'h':
            usage();
            return 0;
            break;
        // ip/cidr
        case 'r':
            strncpy(target, optarg, IPCIDRSTR_ADDR_LEN);
            break;
        // timeout
        case 't':
        {
            uint32_t n = atol(optarg);
            tv.tv_sec = n / 1000;
            tv.tv_usec = (n % 1000) * 1000;
        }
        break;
        case '?':
            if (optopt == 't')
                fprintf(stderr, "Option -t requires a timeout in milliseconds.\n");
            return -1;
        }

    // Check range
    if (strlen(target) && split_cidr_range(target, NULL, NULL))
    {
        fprintf(stderr, "You must enter IP/CIDR range. For example: 10.0.0.1/24\n");
        return -1;
    }

    /// START ///
    if (optind < argc)
    {
        strncpy(interface_name, argv[optind], IFNAMSIZ - 1);
        interface_name[IFNAMSIZ - 1] = 0;
    }

    if (interface_name[0] == 0)
    {
        fprintf(stderr, "You must specify an interface\n");
        return -1;
    }

    fd = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_ARP));
    if (fd < 0)
    {
        perror("creting socket");
        return -1;
    }
    memcpy(ifr.ifr_name, interface_name, strlen(interface_name));
    ifr.ifr_name[strlen(interface_name)] = 0;

    // set timeout
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        perror("setting socket timeout");
        return -1;
    }

    if (ioctl(fd, SIOCGIFINDEX, &ifr) == -1)
    {
        perror("getting interface index");
        return -1;
    }
    interface_index = ifr.ifr_ifindex;

    if (ioctl(fd, SIOCGIFHWADDR, &ifr) == -1)
    {
        perror("getting interface MAC address");
        return -1;
    }
    memcpy(interface_mac, ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);

    if (ioctl(fd, SIOCGIFADDR, &ifr) == -1)
    {
        perror("getting interface IP address");
        return -1;
    }
    struct sockaddr_in *ipaddr = (struct sockaddr_in *)&ifr.ifr_addr;
    memcpy(interface_ip, inet_ntoa(ipaddr->sin_addr), IPSTR_ADDR_LEN);

    if (!strlen(target))
    {
        // User didn't give a target range. User our IP/24
        sprintf(target, "%s/24", interface_ip);
    }

    if (flags & VERBOSE)
    {
        printf("Interface %s\n", interface_name);
        printf("Local IP %s\n", interface_ip);
        printf("Local MAC %02x:%02x:%02x:%02x:%02x:%02x\n", interface_mac[0], interface_mac[1], interface_mac[2], interface_mac[3], interface_mac[4], interface_mac[5]);
        printf("Scan range %s\n", target);
    }

    matches = getMACs(fd, interface_index, interface_mac, interface_ip, target, flags, &mac_list_hash);

    close(fd);

    return (matches > 0); // returns 1 if at least one MAC was printed
}

int getMACs(int fd, int interface_index, char mac[ETHER_ADDR_LEN], char *ip, char *target, int flags, struct hsearch_data *htab)
{
    const unsigned char ether_broadcast_addr[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    struct sockaddr_ll addr = {0}, r_addr = {0};
    struct ether_arp req, *rep;
    struct in_addr source_ip_addr = {0};
    struct in_addr target_ip_addr = {0};
    struct in_addr ip_range = {0};
    struct iovec iov[1];
    struct msghdr message;
    struct msghdr reply;
    ssize_t reply_len;
    char buffer[512];
    struct iovec r_iov[1];
    int p, show = 0, matches = 0;
    ENTRY e, *ep;
    uint32_t ip_count, i, found;
    char macstr[MACSTR_ADDR_LEN];
    char macstr_dest[MACSTR_ADDR_LEN];
    char ipstr[IPSTR_ADDR_LEN];
    char ipstr_dest[IPSTR_ADDR_LEN];

    // Init
    bzero(&message, sizeof(message));
    bzero(&reply, sizeof(reply));

    // Prepare range
    split_cidr_range(target, &ip_range, &ip_count);

    // Construct target address
    addr.sll_family = AF_PACKET;
    addr.sll_ifindex = interface_index;
    addr.sll_halen = ETHER_ADDR_LEN;
    addr.sll_protocol = htons(ETH_P_ARP);
    memcpy(addr.sll_addr, ether_broadcast_addr, ETHER_ADDR_LEN);

    // Construct ARP request
    req.arp_hrd = htons(ARPHRD_ETHER);
    req.arp_pro = htons(ETH_P_IP);
    req.arp_hln = ETHER_ADDR_LEN;
    req.arp_pln = sizeof(in_addr_t);
    req.arp_op = htons(ARPOP_REQUEST);
    memset(&req.arp_tha, 0, sizeof(req.arp_tha));

    if (!inet_aton(ip, &source_ip_addr))
    {
        fprintf(stderr, "%s is not a valid IP address", ip);
        return -2;
    }
    memcpy(&req.arp_spa, &source_ip_addr.s_addr, sizeof(req.arp_spa));
    memcpy(req.arp_sha, mac, ETHER_ADDR_LEN);

    for (i = 0; i < ip_count; ++i)
    {

        memcpy(&req.arp_tpa, &ip_range.s_addr, sizeof(req.arp_tpa));
        if (flags & PRINT_REQ)
            printf("Sending ARP request for %s\n", inet_ntoa(ip_range));

        // Send the packet
        iov[0].iov_base = &req;
        iov[0].iov_len = sizeof(req);

        message.msg_name = &addr;
        message.msg_namelen = sizeof(addr);
        message.msg_iov = iov;
        message.msg_iovlen = 1;
        message.msg_control = 0;
        message.msg_controllen = 0;

        if (sendmsg(fd, &message, 0) == -1)
        {
            perror("sending ARP request");
            return -3;
        }

        r_iov[0].iov_base = buffer;
        r_iov[0].iov_len = sizeof(req);
        reply.msg_name = &r_addr;
        reply.msg_namelen = sizeof(r_addr);
        reply.msg_iov = r_iov;
        reply.msg_iovlen = 1;
        reply.msg_control = 0;
        reply.msg_controllen = 0;

        found = 0;
        do
        {
            // Wait for reply
            if ((reply_len = recvmsg(fd, &reply, 0)) < 0)
            {
                if (errno != EAGAIN)
                {
                    perror("receiving ARP request");
                    return -4;
                }
                else
                {
                    break;
                }
            }

            // Check it's an ARP reply and it's for us
            rep = (struct ether_arp *)buffer;
            macarray_to_str(rep->arp_sha, macstr);
            iparray_to_str(rep->arp_spa, ipstr);

            // Check if it's for us
            found = (*(uint32_t *)rep->arp_spa == *(uint32_t *)req.arp_tpa);

            if (ntohs(rep->arp_op) == ARPOP_REPLY && (found) && (show))
            {
                printf("%s\t", macstr);
                printf("%s", ipstr);
                ++matches;

                if (flags & VERBOSE)
                {
                    // Print ARP destination (usually our IP)
                    macarray_to_str(rep->arp_tha, macstr_dest);
                    iparray_to_str(rep->arp_tpa, ipstr_dest);
                    printf("\t->\t");
                    printf("%s\t", macstr_dest);
                    printf("%s", ipstr_dest);
                }
                printf("\n");
            }

        } while (!found);

        ip_range.s_addr = inc_netorder(ip_range.s_addr); // Next
    }
    return matches;
}

int split_cidr_range(const char *target, struct in_addr *ip_range, uint32_t *ip_count)
{
    char tmp[IPCIDRSTR_ADDR_LEN];
    int count;
    struct in_addr range;
    char *cidr;
    uint32_t mask = 0xFFFFFFFF;

    strncpy(tmp, target, IPCIDRSTR_ADDR_LEN);

    cidr = strchr(tmp, '/');
    if (cidr == NULL)
        return 1; // Error

    ++cidr;
    if (cidr == NULL)
        return 1; // Error

    count = atoi(cidr);
    if (count < 1 || count > 32)
        return 1; // Error

    --cidr;
    *cidr = 0; // Cut tmp string

    mask = mask << 32 - count;

    if (inet_aton(tmp, &range) == 0)
        return 2; // Error

    // Apply mask
    range.s_addr &= htonl(mask);

    // OK
    if (ip_range)
        memcpy(ip_range, &range, sizeof(range)); // Return value if pointer was given

    if (ip_count)
        *ip_count = ~mask + 1; // Return value if pointer was given

    return 0;
}

uint32_t inc_netorder(uint32_t value)
{
    return ntohl(htonl(value) + 1);
}

void macarray_to_str(uint8_t array[ETHER_ADDR_LEN], char dest[MACSTR_ADDR_LEN])
{
    sprintf(dest, "%02x:%02x:%02x:%02x:%02x:%02x", array[0], array[1], array[2], array[3], array[4], array[5]);
}
void iparray_to_str(uint8_t array[IP_ADDR_LEN], char dest[IPSTR_ADDR_LEN])
{
    sprintf(dest, "%u.%u.%u.%u", array[0], array[1], array[2], array[3]);
}
void usage()
{
    printf("Usage: getmacs [-t time] [-r IP/CIDR] interface\n\n");
    printf("  -r IP/CIDR      Scan this IP range. If not given <localIP>/24 is used\n");
    printf("  -t time         Set wait-for-reply timeout to <time> milliseconds. Default is 950 ms\n");
    printf("  -h              Print this help\n");
}