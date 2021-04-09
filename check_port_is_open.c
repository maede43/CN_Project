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

#define DEFAULT_SEND_TIMEOUT 3
#define DEFAULT_RECV_TIMEOUT 3
#define MAX_IP_STR_LEN 17

int16_t recv_timeout = DEFAULT_RECV_TIMEOUT;
int16_t send_timeout = DEFAULT_SEND_TIMEOUT;

void remove_cr(char *);
int socket_creation();
bool socket_connect(int, char *, uint16_t, int *);

int main(int argc, char **argv)
{
	char hostname[MAX_IP_STR_LEN];

	//Get the hostname to scan
	printf("Enter IP : ");
	fgets(hostname, MAX_IP_STR_LEN, stdin);
	remove_cr(hostname);

	int socket;
	int out_errno;

	for (int i = 1; i < 100; i++)
	{
		socket = socket_creation();

		if (socket < 0)
		{
			perror("socket_creation() failed");
		}

		// Connect OK -> port is open
		if (socket_connect(socket, hostname, (uint16_t)i, &out_errno))
		{
			printf("%s:%d is open\n", hostname, i);
		}
		// port may not be open
		else
		{
			switch (out_errno)
			{
			// The port may not be DROPPED by the firewall :
			case ECONNREFUSED: /* Connection refused. */
				printf("%s:%d may_not_be_firewalled. (ECONNREFUSED)\n", hostname, i);
				break;

			// The port may be DROPPED by the firewall :
			case EINPROGRESS:
				printf("%s:%d firewall_det. (EINPROGRESS)\n", hostname, i);
				break;
			case ETIMEDOUT: /* Connection timedout. */
				printf("%s:%d firewall_det. (ETIMEDOUT)\n", hostname, i);
				break;

			// Error client :
			case ENETUNREACH: /* Network unreachable. */
				printf("%s:%d Network unreachable. (ENETUNREACH)\n", hostname, i);
				break;
			case EINTR: /* Interrupted. */
				printf("%s:%d Interrupted. (EINTR)\n", hostname, i);
				break;
			case EFAULT: /* Fault. */
				printf("%s:%d Fault. (EFAULT)\n", hostname, i);
				break;
			case EBADF: /* Invalid sockfd. */
				printf("%s:%d Invalid sockfd. (EBADF)\n", hostname, i);
				break;
			case ENOTSOCK: /* sockfd is not a socket file descriptor. */
				printf("%s:%d sockfd is not a socket file descriptor. (ENOTSOCK)\n", hostname, i);
				break;
			case EPROTOTYPE: /* Socket does not support the protocol. */
				printf("%s:%d Socket does not support the protocol. (EPROTOTYPE)\n", hostname, i);
				break;
			default:
				printf("%s:%d Unknown error. (unknown_error)\n", hostname, i);
				break;
			}
		}
		if (socket > -1)
		{
			close(socket);
		}
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

void remove_cr(char *str)
{
	for (int i = 0; i < strlen(str); i++)
	{
		if (str[i] == '\n')
		{
			str[i] = '\0';
			return;
		}
	}
}