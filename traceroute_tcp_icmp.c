#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>

#define PORT_NUMBER 12500

int main(int argc, char *argv[])
{
    char *dest = argv[1];

    // Get current IP
    struct sockaddr_in *src_addr;
    struct ifaddrs *id, *id_tmp;
    int getAddress = getifaddrs(&id);
    if (getAddress == -1)
    {
        perror("Unable to retrieve IP address of interface");
        exit(-1);
    }

    id_tmp = id;
    while (id_tmp)
    {
        if ((id_tmp->ifa_addr) && (id_tmp->ifa_addr->sa_family == AF_INET))
        {
            src_addr = (struct sockaddr_in *)id_tmp->ifa_addr;
        }
        id_tmp = id_tmp->ifa_next;
    }
    printf("GATEWAY<%s>\n", inet_ntoa(src_addr->sin_addr));

    // Resolve destination IP
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT_NUMBER);

    int is_ip_addr = inet_pton(AF_INET, dest, &(dest_addr.sin_addr));
    if (!is_ip_addr)
    {
        struct hostent *host;
        host = gethostbyname(dest);
        if (host == NULL)
        {
            perror("failed DNS resolution!");
            exit(-1);
        }

        dest_addr.sin_addr = *((struct in_addr *)host->h_addr);
        // printf("DESTINATION<%s> : ip<%s>\n", trace_dest, inet_ntoa(dest_addr.sin_addr));
    }

    // create socket to send tcp messages
    int sendSocket = socket(PF_INET, SOCK_STREAM, 0);
    if (sendSocket < 0)
    {
        perror("failed to create tcp socket");
        exit(-1);
    }

    // create socket to receive icmp messages
    int recvSocket = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (recvSocket < 0)
    {
        perror("failed to create icmp socket");
        exit(-1);
    }

    return 0;
}