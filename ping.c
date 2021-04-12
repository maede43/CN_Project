#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip_icmp.h>
#include <time.h>

#define DEFAULT_TTL 64;
// Gives the timeout delay for receiving packets
// in seconds
#define RECV_TIMEOUT 1
// Automatic port number
#define PORT_NO 0

// time to live value
int ttl_val = DEFAULT_TTL;

int socket_creation();
struct in_addr **dns_lookup(char*, struct hostent*);

int main(int argc, char *argv[])
{
    struct hostent host_entity;
    if (argc != 2)
    {
        printf("\n%s <host>\n", argv[0]);
        return 0;
    }

    struct in_addr **ip_list = dns_lookup(argv[1], &host_entity);
    if (ip_list == NULL)
    {
        printf("\nResolving failed.\n");
        return 0;
    }

    printf("All addresses: ");
    for (int i = 0; ip_list[i] != NULL; i++)
    {
        printf("%s ", inet_ntoa(*ip_list[i]));
        printf("\n");
    }

    // struct sockaddr_in server_addr;
    // server_addr.sin_family = host_entity.h_addrtype;
    // server_addr.sin_port = htons(PORT_NO);
    // server_addr.sin_addr.s_addr = *(long *)host_entity.h_addr;

    // char *ip = (char *)malloc(NI_MAXHOST * sizeof(char));
    // //filling up address structure
    // strcpy(ip, inet_ntoa(*(struct in_addr *)host_entity.h_addr));

    return 0;
}

// get all Resolutions - DNS lookup
// output => list of all ip address , host_entity
struct in_addr **dns_lookup(char *addr_host, struct hostent *host_entity)
{
    struct in_addr **addr_list;

    if ((host_entity = gethostbyname(addr_host)) == NULL)
    {
        //The routine herror() can be used to print an error message describing the failure : HOST_NOT_FOUND - TRY_AGAIN - NO_RECOVERY - NO_DATA
        // do some error checking
        herror("gethostbyname() failed"); // herror(), NOT perror()
        return NULL;
    }

    addr_list = (struct in_addr **)host_entity->h_addr_list;
    return addr_list;
}

// return file descriptor of created socket
int socket_creation()
{
    int fd; // file descriptor

    fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (fd < 0)
    {
        perror("socket() failed.");
        return fd;
    }

    struct timeval tv_out;
    tv_out.tv_sec = RECV_TIMEOUT;
    tv_out.tv_usec = 0;

    // set socket options at ip to TTL
    if (setsockopt(fd, SOL_IP, IP_TTL, &ttl_val, sizeof(ttl_val)) != 0)
    {
        printf("\nSetting socket options to TTL failed!\n");
        if (fd > -1)
        {
            close(fd);
            fd = -1;
        }
    }

    // setting timeout of recv setting
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv_out, sizeof(tv_out)) < 0)
    {
        perror("\nSetting socket options to Timeout failed!\n");
        if (fd > -1)
        {
            close(fd);
            fd = -1;
        }
    }
    return fd;
}
