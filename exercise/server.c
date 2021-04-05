#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/types.h>
// #include<sys/socket.h>
// #include<netinet/in.h>
// #include<arpa/inet.h>

/*
* port from user
* start server (socket creation & binding)
* loop =>
*   start listening & echoing
*/

// const int MAX_IP_STR_LEN = 17;
const int MAX_PORT_STR_LEN = 7;
const int MAX_MSG_OUT_LEN = 128;
const int MAX_PENDING = 5;

void remove_cr(char* str){
    for(int i = 0; i < strlen(str); i++) {
        if (str[i] == '\n') {
            str[i] = '\0';
            return;
        }
    }
}

void serve_client(int client_socket) {
    char buffer[MAX_MSG_OUT_LEN];
    ssize_t bytes_rcv = recv(client_socket, buffer, MAX_MSG_OUT_LEN, 0);

    if (bytes_rcv < 0) {
        perror("recv failed");
        free(port_str);
        exit(EXIT_FAILURE);
    }

    while (bytes_rcv > 0)
    {
        ssize_t bytes_send = send(client_socket, buffer, bytes_rcv, 0);
        if (bytes_send < 0) {
            perror("send failed");
            free(port_str);
            exit(EXIT_FAILURE);
        }
        else if (bytes_rcv != bytes_send) {
            fputs("unexpected number of bytes were received.", stderr);
            free(port_str);
            exit(EXIT_FAILURE);
        }

        bytes_rcv = recv(client_socket, buffer, MAX_MSG_OUT_LEN, 0);
        if (bytes_rcv < 0) {
            perror("recv failed");
            free(port_str);
            exit(EXIT_FAILURE);
        }
    }
    close(client_socket);
    
}

int main(){
    char* port_str = malloc(sizeof(char) * MAX_PORT_STR_LEN);
    if (port_str ==  NULL)
    {
        fputs("memory alocation failed. (port)", stderr);
        exit(EXIT_FAILURE);
    }
    memset(port_str, '\0', MAX_PORT_STR_LEN);

    puts("Server Port: [uint 16] ");
    fgets(port_str, MAX_PORT_STR_LEN, stdin);

    remove_cr(port_str);
    puts(port_str);

    in_port_t server_port = atoi(port_str);

    int server_socket = socket(AF_INET, SOCK_STREAM, IPPOROTO_TCP);
    if (server_socket < 0) {
        perror("socket() failed.");
        free(port_str);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_socket, (const struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        perror("bind failed");
        free(port_str);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_PENDING) < 0) {
        perror("listen failed");
        free(port_str);
        exit(EXIT_FAILURE);
    }

    for(;;) {
        struct sockaddr_in client_address;
        // socklen_t client_address_len = sizeof(client_address);
        socklen_t client_address_len;

        int client_socket = accept(server_socket, (const struct sockaddr *) &client_address, &client_address_len);
        if (client_socket < 0) {
            perror("accept failed", stderr);
            free(port_str);
            exit(EXIT_FAILURE);
        }

        char client_name[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &client_address.sin_addr.s_addr, client_name, sizeof(client_address)) != NULL) {
            printf("serving [%s:%d]\n", client_name, ntohs(client_address.sin_port));
        }
        else {
            puts("unable to get client name.");
        }

        serve_client(client_socket);

    }

}