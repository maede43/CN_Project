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
#include <stdbool.h>
#include <signal.h>
#include <float.h>

#define DEFAULT_TTL 64;
#define RECV_TIMEOUT 1
// Automatic port number
#define PORT_NO 0
#define DEFAULT_PACKET_SIZE 64
#define PING_SLEEP_RATE 1000000

// time to live value
int ttl_val = DEFAULT_TTL;
// ping packet size
int packet_size = DEFAULT_PACKET_SIZE;
bool continue_pinging = true;

// ping packet structure
struct ping_pkt
{
    struct icmphdr header;
    char *msg;
};

int socket_creation();
struct in_addr **dns_lookup(char *, struct hostent *);
unsigned short checksum(void *, int);
void send_ping(int, struct sockaddr_in *, char *);
// Interrupt handler
void intHandler(int dummy);

int main(int argc, char *argv[])
{
    //catching interrupt
    signal(SIGINT, intHandler);

    struct hostent host_entity;
    if (argc != 2)
    {
        printf("usage : sudo %s <host>\n", argv[0]);
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

    // works for first ip addr
    struct sockaddr_in server_addr;
    server_addr.sin_family = host_entity.h_addrtype;
    server_addr.sin_port = htons(PORT_NO);
    server_addr.sin_addr.s_addr = *(long *)ip_list[0];

    char *ip_addr = (char *)malloc(NI_MAXHOST * sizeof(char));
    //filling up address structure
    strcpy(ip_addr, inet_ntoa(*(struct in_addr *)ip_list[0]));

    int sockfd = socket_creation();
    if (sockfd < 0)
    {
        printf("\nSocket() failed.\n");
        return 0;
    }

    //send pings continuously
    send_ping(sockfd, &server_addr, ip_addr);

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
        printf("Setting socket options to TTL failed!\n");
        if (fd > -1)
        {
            close(fd);
            fd = -1;
        }
    }

    // setting timeout of recv setting
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv_out, sizeof(tv_out)) < 0)
    {
        perror("Setting socket options to Timeout failed!\n");
        if (fd > -1)
        {
            close(fd);
            fd = -1;
        }
    }
    return fd;
}

// ping request
void send_ping(int ping_sockfd, struct sockaddr_in *ping_addr, char *ping_ip)
{
    long double min_rtt = LDBL_MAX, max_rtt = 0;
    int msg_count = 0, i, addr_len, msg_received_count = 0;
    bool packet_sent = true;

    struct ping_pkt packet;
    struct sockaddr_in recv_addr;
    struct timespec time_start, time_end;
    long double rtt = 0;

    // send icmp packet in an infinite loop
    while (continue_pinging)
    {
        bzero(&packet, sizeof(packet));
        packet_sent = true;

        packet.header.type = ICMP_ECHO;
        packet.header.un.echo.id = getpid(); // id of current process
        packet.msg = (char *)malloc((packet_size - sizeof(struct icmphdr)) * sizeof(char));

        // filling packet by '1234...'
        for (i = 0; i < sizeof(packet.msg) - 1; i++)
            packet.msg[i] = i + '0';

        printf("msg: %s\n", packet.msg);

        packet.msg[i] = 0;
        packet.header.un.echo.sequence = msg_count++;
        packet.header.checksum = checksum(&packet, sizeof(packet));

        usleep(PING_SLEEP_RATE);

        //send packet
        clock_gettime(CLOCK_MONOTONIC, &time_start);
        //sendto => Send packet on ping_socket to peer at ping_addr
        //returns the number sent, or -1 for errors.
        if (sendto(ping_sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)ping_addr, sizeof(*ping_addr)) <= 0)
        {
            packet_sent = false;
            printf("sendto() failed.\n");
        }

        addr_len = sizeof(recv_addr);
        //receive packet
        if (recvfrom(ping_sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&recv_addr, &addr_len) <= 0 &&
            msg_count > 1)
        {
            printf("recvfrom() failed!\n");
        }
        else
        {
            // calculate RTT
            clock_gettime(CLOCK_MONOTONIC, &time_end);
            double timeElapsed = ((double)(time_end.tv_nsec - time_start.tv_nsec)) / 1000000.0;
            rtt = (time_end.tv_sec - time_start.tv_sec) * 1000.0 + timeElapsed;

            // if packet was not sent, don't receive
            if (packet_sent)
            {
                if ((packet.header.type == 69 && packet.header.code == 0))
                {
                    printf("Reply from IP<%s> in %Lfms seq=%d.\n", ping_ip, rtt, msg_count);
                    msg_received_count++;
                    if (rtt < min_rtt)
                        min_rtt = rtt;
                    if (rtt > max_rtt)
                        max_rtt = rtt;
                }
                else
                {
                    printf("Error..ICMP type %d, code %d\n", packet.header.type, packet.header.code);
                }
            }
        }
    }
    if (msg_received_count > 0)
    {
        float loss = ((msg_count - msg_received_count) / (float)msg_count) * 100.0;
        printf("\n------------statistics------------\n");
        printf("for IP<%s> <%d> packet(s) sent and <%d> packet(s) received, loss = %f%%.\n", ping_ip, msg_count, msg_received_count, loss);
        printf("MINIMUM RTT=<%Lf>ms MAXIMUM RTT=<%Lf>ms.\n", min_rtt, max_rtt);
    }
}

// Calculate checksum bit
unsigned short checksum(void *b, int len)
{
    unsigned short *buf = b;
    unsigned int sum = 0;
    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;

    if (len == 1)
        sum += *(unsigned char *)buf;

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    // One's Complement
    return ~sum;
}

// Interrupt handler
void intHandler(int dummy)
{
    continue_pinging = false;
}