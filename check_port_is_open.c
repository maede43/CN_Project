#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <inttypes.h>

#define DEFAULT_TIMEOUT 3
#define MAX_NUM_THREAD 100 // Limited by system resources
#define MAX_IP_STR_LEN 17
#define DEFAULT_THREAD_NUM 8
#define DEFAULT_START 1	 // port number
#define DEFAULT_END 1023 // port number

int16_t recv_timeout = DEFAULT_TIMEOUT;
int16_t send_timeout = DEFAULT_TIMEOUT;
int16_t start = DEFAULT_START;
int16_t end = DEFAULT_END;

int16_t num_thread;
int config = -1; // -1: invalid, 0: all ports, 1: well-known ports, 2: specified port, 3: port range
char *hostname = NULL;

const static struct option long_options[] = {
	{"help", no_argument, 0, 0x1},
	{"host", required_argument, 0, 'h'},
	{"all-ports", no_argument, 0, 'a'},
	{"well-known-ports", no_argument, 0, 'w'},
	{"specified-port", required_argument, 0, 's'},
	{"threads", required_argument, 0, 'r'},
	{"timeout", required_argument, 0, 't'},
	{"port-range", no_argument, 0, 'p'},
	{0, 0, 0, 0}};

void usage();
bool parse_argv(int, char **);
// void remove_cr(char *);
int socket_creation();
bool socket_connect(int, char *, uint16_t, int *);

int main(int argc, char **argv)
{
	// //Get the hostname to scan
	// printf("Enter IP : ");
	// fgets(hostname, MAX_IP_STR_LEN, stdin);
	// remove_cr(hostname);

	// int socket;
	// int out_errno;

	// for (int i = 1; i < 100; i++)
	// {
	// 	socket = socket_creation();

	// 	if (socket < 0)
	// 	{
	// 		perror("socket_creation() failed");
	// 	}

	// 	// Connect OK -> port is open
	// 	if (socket_connect(socket, hostname, (uint16_t)i, &out_errno))
	// 	{
	// 		printf("%s:%d is open\n", hostname, i);
	// 	}
	// 	// port may not be open
	// 	else
	// 	{
	// 		switch (out_errno)
	// 		{
	// 		// The port may not be DROPPED by the firewall :
	// 		case ECONNREFUSED: /* Connection refused. */
	// 			printf("%s:%d may_not_be_firewalled. (ECONNREFUSED)\n", hostname, i);
	// 			break;

	// 		// The port may be DROPPED by the firewall :
	// 		case EINPROGRESS:
	// 			printf("%s:%d firewall_det. (EINPROGRESS)\n", hostname, i);
	// 			break;
	// 		case ETIMEDOUT: /* Connection timedout. */
	// 			printf("%s:%d firewall_det. (ETIMEDOUT)\n", hostname, i);
	// 			break;

	// 		// Error client :
	// 		case ENETUNREACH: /* Network unreachable. */
	// 			printf("%s:%d Network unreachable. (ENETUNREACH)\n", hostname, i);
	// 			break;
	// 		case EINTR: /* Interrupted. */
	// 			printf("%s:%d Interrupted. (EINTR)\n", hostname, i);
	// 			break;
	// 		case EFAULT: /* Fault. */
	// 			printf("%s:%d Fault. (EFAULT)\n", hostname, i);
	// 			break;
	// 		case EBADF: /* Invalid sockfd. */
	// 			printf("%s:%d Invalid sockfd. (EBADF)\n", hostname, i);
	// 			break;
	// 		case ENOTSOCK: /* sockfd is not a socket file descriptor. */
	// 			printf("%s:%d sockfd is not a socket file descriptor. (ENOTSOCK)\n", hostname, i);
	// 			break;
	// 		case EPROTOTYPE: /* Socket does not support the protocol. */
	// 			printf("%s:%d Socket does not support the protocol. (EPROTOTYPE)\n", hostname, i);
	// 			break;
	// 		default:
	// 			printf("%s:%d Unknown error. (unknown_error)\n", hostname, i);
	// 			break;
	// 		}
	// 	}
	// 	if (socket > -1)
	// 	{
	// 		close(socket);
	// 	}
	// }

	if (!parse_argv(argc, argv))
	{
		usage();
		return 1;
	}

	return (0);
}

// return fileDescriptor of created socket
int socket_creation()
{
	int fd; // fileDescriptor
	struct timeval timeout;

	// SOCK_STREAM related to TCP connection
	// 0 -> ANY
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
	{
		perror("socket() failed.");
		return fd;
	}

	timeout.tv_sec = recv_timeout;
	timeout.tv_usec = 0;

	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
	{
		perror("setsockopt() failed.");
		if (fd > -1)
		{
			close(fd);
			fd = -1;
		}
	}

	timeout.tv_sec = send_timeout;
	timeout.tv_usec = 0;

	if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
	{
		perror("setsockopt() failed.");
		if (fd > -1)
		{
			close(fd);
			fd = -1;
		}
	}
	return fd;
}

// connect socket(fileDescriptor) to target_host:target_port
// out_errno is kinda output
bool socket_connect(int fileDescriptor, char *host, uint16_t port, int *out_errno)
{
	// fileDescriptor -> socket
	struct sockaddr_in server_addr;

	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = inet_addr(host);

	if (connect(fileDescriptor, (struct sockaddr *)&(server_addr), sizeof(struct sockaddr_in)) < 0)
	{
		*out_errno = errno;
		// perror("connection failed.");
		return false;
	}

	return true;
}

// void remove_cr(char *str)
// {
// 	for (int i = 0; i < strlen(str); i++)
// 	{
// 		if (str[i] == '\n')
// 		{
// 			str[i] = '\0';
// 			return;
// 		}
// 	}
// }

bool parse_argv(int argc, char *argv[])
{
	if (argc == 1)
		return 0;

	int c;
	int opt_index = 0;

	while (1)
	{
		c = getopt_long(argc, argv, "h:s:r:t:paw", long_options, &opt_index);

		if (c == -1)
			break;

		switch (c)
		{
		case 0x1:
			return false;
			break;

		case 'h':
			hostname = optarg;
			break;

		// all ports
		case 'a':
			config = 0;
			break;

		// well-known ports
		case 'w':
			config = 1;
			break;

		// specified port
		case 's':
			config = 2;
			char *port = optarg;
			break;

		case 'r':
			num_thread = (uint16_t)atoi(optarg);
			break;

		case 't':
			recv_timeout = atoi(optarg);
			send_timeout = atoi(optarg);
			break;

		case 'p':
			printf("Enter start port number : ");
			scanf("%" SCNd16, &start);
			printf("Enter end port number : ");
			scanf("%" SCNd16, &end);
			if (start > end)
			{
				printf("invalid range");
				return false;
			}
			config = 3;
			break;

		default:
			printf("\n");
			return false;
			break;
		}
	}

	if (hostname == NULL)
	{
		printf("Error: Target host cannot be empty!\n");
		return false;
	}

	if (config == -1)
	{
		printf("Error: Choose configuration([-a] or [-w] or [-s <port name>] or [-p])\n");
		return false;
	}

	if (num_thread < 1)
	{
		num_thread = DEFAULT_THREAD_NUM;
	}
	else if (num_thread > MAX_NUM_THREAD)
	{
		num_thread = MAX_NUM_THREAD;
	}

	return true;
}

void usage()
{
	printf("    -h|--host <host>|Target host(IPv4)\n");
	printf("    -a|--all-ports\n");
	printf("    -w|--well-known-ports\n");
	printf("    -s|--specified_port <port name> like [-s 'http']\n");
	printf("    -r|--threads <num>|Number of threads (default: %d)\n", DEFAULT_THREAD_NUM);
	printf("    -t|--timeout <num>|Timeout is sec (default: %d)\n", DEFAULT_TIMEOUT);
	printf("    -p|--port-range|Range of port numbers\n");
}