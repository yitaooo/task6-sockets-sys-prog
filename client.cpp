#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include "utils.h"

struct arg_t {
    std::string hostname;
    int port;
    int numMessages;
    int add;
    int sub;
};

std::mutex console_mtx;

void *thread_worker(void *arg) {
    struct arg_t *args = (struct arg_t *)arg;
    int i;
    int numMessages = args->numMessages;
    int add = args->add;
    int sub = args->sub;
    std::string hostname = args->hostname;
    int port = args->port;
    int sockfd = connect_socket(hostname.c_str(), port);
    if (sockfd < 0) {
        perror("connect_socket");
        return NULL;
    }
    for (i = 0; i < numMessages; i++) {
        if (i % 2 == 0) {
            // add
            send_msg(sockfd,
                     sockets::message_OperationType::message_OperationType_ADD,
                     add);
        } else {
            // sub
            send_msg(sockfd,
                     sockets::message_OperationType::message_OperationType_SUB,
                     sub);
        }
    }
    // TERMINATE
    send_msg(sockfd,
             sockets::message_OperationType::message_OperationType_TERMINATION,
             0);

    // receive the result
    int32_t operation_type;
    int64_t argument;
    recv_msg(sockfd, &operation_type, &argument);

    console_mtx.lock();
    printf("%d\n", (int)argument);
    console_mtx.unlock();

    close(sockfd);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 7) {
        std::cerr << "usage: ./client <num_threads> <hostname> <port> "
                     "<num_messages> <add> <sub>\n";
        exit(1);
    }

    int numClients = std::atoi(argv[1]);
    std::string hostname = argv[2];
    int port = std::atoi(argv[3]);
    int numMessages = std::atoi(argv[4]);
    int add = std::atoi(argv[5]);
    int sub = std::atoi(argv[6]);

    struct arg_t arg;
    arg.hostname = hostname;
    arg.port = port;
    arg.numMessages = numMessages;
    arg.add = add;
    arg.sub = sub;
    std::vector<std::thread> threads;

    for (int i = 0; i < numClients; i++) {
        threads.emplace_back(thread_worker, &arg);
    }

    for (size_t i = 0; i < threads.size(); i++) {
        threads[i].join();
    }

    return 0;
}
