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

#define DEFAULT_SEND_TIMEOUT 2
#define DEFAULT_RECV_TIMEOUT 2
#define MAX_IP_STR_LEN     17

int16_t recv_timeout = DEFAULT_RECV_TIMEOUT;
int16_t send_timeout = DEFAULT_SEND_TIMEOUT;

void remove_cr(char*);
int socket_creation();
bool socket_connect(int, char*, uint16_t, int*);

int main(int argc , char **argv)
{
	char hostname[MAX_IP_STR_LEN];

	//Get the hostname to scan
	printf("Enter IP : ");
	fgets(hostname, MAX_IP_STR_LEN, stdin);
	remove_cr(hostname);

	int out_errno;

	for (int i = 1; i < 100; i++)
	{
		int fd = socket_creation();
		if (socket_connect(fd, hostname, (uint16_t)i, &out_errno)) {
			printf("%s:%d is open\n",hostname, i);
		}
		else 
			printf("%s:%d is close\n",hostname, i);

		if (fd > 0)		
			close(fd);	
	}

	return(0);
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

	// printf("Connecting to %s:%d... ", host, port);
	if (connect(fileDescriptor, (struct sockaddr *)&(server_addr), sizeof(struct sockaddr_in)) < 0)
	{
		*out_errno = errno;
		perror("connection failed.");
		return false;
	}
	// printf("Connection established %s:%d\n", host, port);

	return true;
}

void remove_cr(char* str){
    for(int i = 0; i < strlen(str); i++) {
        if (str[i] == '\n') {
            str[i] = '\0';
            return;
        }
    }
}