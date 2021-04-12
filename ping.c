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
