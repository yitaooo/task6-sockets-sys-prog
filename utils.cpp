#include "utils.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/socket.h>

extern "C" {

int listening_socket(int port) {
    int sockfd;
    struct sockaddr_in addr;
    int yes = 1;
    int ret;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (ret < 0) {
        perror("setsockopt");
        return -1;
    }

    ret = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        perror("bind");
        return -1;
    }

    ret = listen(sockfd, 5);
    if (ret < 0) {
        perror("listen");
        return -1;
    }

    return sockfd;
}

int connect_socket(const char *hostname, const int port) {
    int sockfd;
    struct sockaddr_in addr;
    int ret;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    // resolve hostname to IP address
    struct hostent *host = gethostbyname(hostname);
    if (host == NULL) {
        perror("gethostbyname");
        return -1;
    }
    addr.sin_addr = *(struct in_addr *)host->h_addr;

    ret = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        perror("connect");
        return -1;
    }

    return sockfd;
}

int accept_connection(int sockfd) {
    int new_sockfd;
    struct sockaddr_in addr;
    socklen_t addrlen;

    addrlen = sizeof(addr);
    new_sockfd = accept(sockfd, (struct sockaddr *)&addr, &addrlen);
    if (new_sockfd < 0) {
        perror("accept");
        return -1;
    }

    return new_sockfd;
}

int recv_msg(int sockfd, int32_t *operation_type, int64_t *argument) {
    // first 4 bytes of message is the size of payload
    int32_t payload_size;
    int ret;
    ret = recv(sockfd, &payload_size, sizeof(payload_size), 0);
    if (ret < 0) {
        perror("recv");
        return 1;
    }

    // next payload_size bytes of message is the payload
    char *payload = new char[payload_size];
    ret = recv(sockfd, payload, payload_size, 0);
    if (ret < 0) {
        delete[] payload;
        perror("recv");
        return 1;
    }
    sockets::message msg;
    msg.ParseFromArray(payload, payload_size);
    *operation_type = msg.type();
    *argument = msg.argument();
    delete[] payload;
    return 0;
}

int send_msg(int sockfd, int32_t operation_type, int64_t argument) {
    sockets::message msg;
    msg.set_type((sockets::message_OperationType)operation_type);
    msg.set_argument(argument);
    int payload_size = msg.ByteSize();
    char *payload = new char[payload_size];
    msg.SerializeToArray(payload, payload_size);
    int ret = send(sockfd, &payload_size, sizeof(payload_size), 0);
    if (ret < 0) {
        delete[] payload;
        perror("send");
        return 1;
    }
    ret = send(sockfd, payload, payload_size, 0);
    if (ret < 0) {
        delete[] payload;
        perror("send");
        return 1;
    }
    delete[] payload;
    return 0;
}
}