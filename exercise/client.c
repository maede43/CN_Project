#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/types.h>
// #include<sys/socket.h>
// #include<netinet/in.h>
// #include<arpa/inet.h>

/*
* ip from user
* port from user
* validation
* connect to server
* loop =>
*   msg from user
*   send to server
*   wait for server
*/

#include <signal.h>

static volatile int keepRunning = 1;

void intHandler(int dummy) {
    keepRunning = 0;
}

const int MAX_IP_STR_LEN = 17;
const int MAX_PORT_STR_LEN = 7;
const int MAX_MSG_OUT_LEN = 128;

void remove_cr(char* str){
    for(int i = 0; i < strlen(str); i++) {
        if (str[i] == '\n') {
            str[i] = '\0';
            return;
        }
    }
}

int main() {

    signal(SIGINT, intHandler);
    
    // 1.1.1.1 => ip
    // 12312 => port
    char* server_addr_str = malloc(sizeof(char) * MAX_IP_STR_LEN);
    if (server_addr_str ==  NULL)
    {
        fputs("memory alocation failed. (ip)", stderr);
        exit(EXIT_FAILURE);
    }
    char* port_str = malloc(sizeof(char) * MAX_PORT_STR_LEN);
    if (port_str ==  NULL)
    {
        fputs("memory alocation failed. (port)", stderr);
        free(server_addr_str);
        exit(EXIT_FAILURE);
    }

    memset(server_addr_str, '\0', MAX_IP_STR_LEN);
    memset(port_str, '\0', MAX_PORT_STR_LEN);
    
    puts("Server IP: [x.x.x.x (ipv4)] ");
    fgets(server_addr_str, MAX_IP_STR_LEN, stdin);
    puts("Server Port: [uint 16] ");
    fgets(port_str, MAX_PORT_STR_LEN, stdin);

    remove_cr(server_addr_str);
    remove_cr(port_str);

    puts(server_addr_str);
    puts(port_str);

    in_port_t server_port = atoi(port_str);

    int sock = socket(AF_INET, SOCK_STREAM, IPPOROTO_TCP);
    if (sock < 0) {
        perror("socket() failed.");
        free(server_addr_str);
        free(port_str);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    int numerical_address = inet_pton(AF_INET, server_addr_str, &server_address.sin_addr.s_addr);
    // invalid ip
    if (numerical_address == 0) {
        fputs("invalid ip", stderr);
        free(server_addr_str);
        free(port_str);
        exit(EXIT_FAILURE);
    }
    // parsing failed
    if (numerical_address < 0) {
        fputs("printable to numeric failed", stderr);
        free(server_addr_str);
        free(port_str);
        exit(EXIT_FAILURE);
    }

    server_address.sin_port = htons(server_port);

    if (connect(sock, (const struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        perror("connection failed.");
        free(server_addr_str);
        free(port_str);
        exit(EXIT_FAILURE);
    }

    char* outgoing_msg = malloc(sizeof(char) * MAX_MSG_OUT_LEN);
    if (outgoing_msg == NULL){
        fputs("memory alocation failed. (msg)", stderr);
        free(server_addr_str);
        free(port_str);
        exit(EXIT_FAILURE);
    }

    while (keepRunning)    
    {
        memset(outgoing_msg, '\0', MAX_MSG_OUT_LEN);
        puts("ENTER YOUR DESIRED MSG: (max_len = 128, rest will be discarded)");
        scanf("%128s", outgoing_msg);
        size_t msg_len = strlen(outgoing_msg);
        ssize_t bytes = send(sock, outgoing_msg, msg_len, 0);
        
        if (bytes < 0) {
            perror("send failed");
            free(server_addr_str);
            free(port_str);
            free(outgoing_msg);
            exit(EXIT_FAILURE);
        }
        else if (bytes != msg_len) {
            fputs("unexpected number of bytes were sent.", stderr);
            free(server_addr_str);
            free(port_str);
            free(outgoing_msg);
            exit(EXIT_FAILURE);
        }

        // blocking send and receive
        // SOCK_STREAM => khoord khoord receive mishe
        unsigned int total_rcv_bytes = 0;
        puts("RECEIVED FROM SERVER: ");
        while (total_rcv_bytes < msg_len)
        {
            char buffer[128] = {'\0'};
            bytes = recv(sock, buffer, MAX_MSG_OUT_LEN - 1, 0);
            if (bytes < 0) {
                perror("recv failed");
                free(server_addr_str);
                free(port_str);
                free(outgoing_msg);
                exit(EXIT_FAILURE);
            }
            else if (bytes == 0) {
                perror("server was terminated");
                free(server_addr_str);
                free(port_str);
                free(outgoing_msg);
                exit(EXIT_FAILURE);
            }

            total_rcv_bytes += bytes;
            buffer[bytes] = '\0';
            printf("%s", buffer);
        }
        puts("");
    }

    close(sock);
    free(server_addr_str);
    free(port_str);
    return 0;
}