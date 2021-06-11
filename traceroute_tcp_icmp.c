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
#include <stdbool.h>
#include <getopt.h>

#define PORT_NUMBER 12564
#define TIMEOUT 10 // seconds
#define MAX_TTL 30
#define RECV_BUF_LEN 10000

char *dest;
bool check_dst = false;
int timeout_ = TIMEOUT;
int max_ttl = MAX_TTL;
int start_ttl = 1;
// int packet_size 
int port = PORT_NUMBER;

void usage(char *app);
bool parse_argv(int argc, char *argv[]);
const static struct option long_options[] = {
	{"help", no_argument, 0, 0x1},
    {"destination-host", required_argument, 0, 'd'},
    {"timeout", required_argument, 0, 't'},
	{"max-ttl", required_argument, 0, 'm'},
    {"start-ttl", required_argument, 0, 's'},
    {"port-number", required_argument, 0, 'p'},
	{0, 0, 0, 0}};

int main(int argc, char *argv[])
{
	if (!parse_argv(argc, argv))
	{
		usage(argv[0]);
		return 1;
	}

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
    dest_addr.sin_port = htons(port);

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

    // timeout
    struct timeval timeout;
    timeout.tv_sec = timeout_;

    int setTimeoutOptTcp = setsockopt(sendSocket, SOL_SOCKET, SO_SNDTIMEO, (struct timeval *)&timeout, sizeof(struct timeval));
    if (setTimeoutOptTcp < 0)
    {
        perror("failed to set socket timeout (tcp)");
        exit(-1);
    }

    int setTimeoutOptIcmp = setsockopt(recvSocket, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&timeout, sizeof(struct timeval));
    if (setTimeoutOptIcmp < 0)
    {
        perror("failed to set socket timeout (icmp)");
        exit(-1);
    }

    // receive buffer
    char recvBuffer[RECV_BUF_LEN];
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(struct sockaddr_in);
    long numBytesReceived;

    printf("******************************************************\n");
    // printf("0: %s <gateway>\n", inet_ntoa(src_addr->sin_addr));
    int i = 1;
    while (i < max_ttl)
    {
        int icmpErrorReceived = 0;

        // set TTL in IP header
        setsockopt(sendSocket, IPPROTO_IP, IP_TTL, &i, sizeof(i));

        // send SYN packet (start 3-way handshake)
        errno = 0;
        connect(sendSocket, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr));

        // TTL expired
        if (errno == EHOSTUNREACH)
        {
            while (!icmpErrorReceived)
            {
                numBytesReceived = recvfrom(recvSocket, recvBuffer, RECV_BUF_LEN, 0, (struct sockaddr *)&cli_addr, &cli_len);

                // extract IP header
                struct ip *ip_hdr = (struct ip *)recvBuffer;

                // extract ICMP header
                int ipHeaderLength = 4 * ip_hdr->ip_hl;
                struct icmp *icmp_hdr = (struct icmp *)((char *)ip_hdr + ipHeaderLength);

                int icmpMessageType = icmp_hdr->icmp_type;
                int icmpMessageCode = icmp_hdr->icmp_code;

                // TTL exceeded
                if (icmpMessageType == ICMP_TIME_EXCEEDED && icmpMessageCode == ICMP_NET_UNREACH)
                {
                    // check if ICMP messages are related to TCP SYN packets
                    struct ip *inner_ip_hdr = (struct ip *)((char *)icmp_hdr + ICMP_MINLEN);
                    if (inner_ip_hdr->ip_p == IPPROTO_TCP)
                    {
                        icmpErrorReceived = 1;
                    }
                }

                // port unreachable
                else if (icmpMessageType == ICMP_DEST_UNREACH && icmpMessageCode == ICMP_PORT_UNREACH)
                {
                    printf("%d: %s [complete]\n", i, inet_ntoa(dest_addr.sin_addr));
                    printf("******************************************************\n");
                    exit(0);
                }
            }
            if (i >= start_ttl)
                printf("%d: %s\n", i, inet_ntoa(cli_addr.sin_addr));

            // timeout
        }
        else if (
            errno == ETIMEDOUT      // socket timeout
            || errno == EINPROGRESS // operation in progress
            || errno == EALREADY    // consecutive timeouts
        )
        {
            printf("%d: * * * * * [timeout]\n", i);
        }

        // destination reached
        else if (errno == ECONNRESET || errno == ECONNREFUSED)
        {
            printf("%d: %s [complete]\n", i, inet_ntoa(dest_addr.sin_addr));
            printf("******************************************************\n");
            exit(0);
        }
        else
        {
            printf("Unknown error: %d sending SYN packet\n", errno);
            exit(-1);
        }

        i++;
    }
    printf("Unable to reach host within TTL of %d\n", max_ttl);
    printf("******************************************************\n");
    return -1;
}

bool parse_argv(int argc, char *argv[])
{
	if (argc == 1)
		return 0;

	int c;
	int opt_index = 0;

	while (c != -1)
	{
		c = getopt_long(argc, argv, "s:t:m:p:d:", long_options, &opt_index);

		if (c == -1)
			break;

		switch (c)
		{
            case 0x1:
                return false;
                break;

            // destination host
            case 'd':
                dest = optarg;
                printf("destination : %s\n", dest);
                check_dst = true;
                break;

            // max ttl
            case 'm':
                max_ttl = atoi(optarg);
                printf("max ttl : %d\n", max_ttl);
                break;

            // timeout
            case 't':
                timeout_ = atoi(optarg);
                printf("timeout : %d\n", timeout_);
                break;

            // start ttl
            case 's' :
                start_ttl = atoi(optarg);
                printf("start ttl : %d\n", start_ttl);
                break;

            // port number
            case 'p' :
                port = atoi(optarg);
                printf("port: %d\n", port);
                break;    

            default:
                printf("\n");
                return false;
                break;
		}
	}
	return check_dst;
}

void usage(char *app)
{
	printf("Usage: sudo %s -d <host> [options]\n", app);
    printf("  Options:\n");
	printf("    -m|--max-ttl <num>|Max ttl (default: %d))\n", MAX_TTL);
	printf("    -t|--timeout <num>|Timeout is sec (default: %d)\n", TIMEOUT);
    printf("    -s|--start-ttl <num>|Start ttl (default: 1)\n");
    printf("    -p|--port-number <num>|Port number (default: %d)\n", PORT_NUMBER);
}