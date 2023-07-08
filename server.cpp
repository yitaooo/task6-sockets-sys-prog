#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include <atomic>
#include <iostream>
#include <string>
#include <thread>

#include "utils.h"

std::atomic<int64_t> number{0};
std::mutex console_mtx;

struct server_state {
    int listen_fd;
    int work_thread_num;
};

struct worker_state {
    int thread_id;
    int epoll_fd;
    int max_events;
    struct epoll_event* events;
};

// run in main thread
// open the port and listen
void init_listener(int port, server_state* state) {
    std::set<int> fds;
    int listening_sockfd = listening_socket(port);
    if (listening_sockfd < 0) {
        perror("listening_socket");
        return;
    }
    fds.insert(listening_sockfd);
    state->listen_fd = listening_sockfd;
    return;
}

// run in main thread
// create epoll instance for each worker thread
void init_worker(worker_state* my_state, int thread_id) {
    my_state->epoll_fd = epoll_create1(0);
    if (my_state->epoll_fd < 0) {
        perror("epoll_create1");
        return;
    }
    my_state->max_events = 1024;
    my_state->events = new epoll_event[my_state->max_events];
    my_state->thread_id = thread_id;
    return;
}

// run in main thread
// by inserting sockfd into worker's poll, we assign it to worker thread
void assign_socket_to_worker(int sockfd, worker_state* worker_state) {
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = sockfd;
    if (epoll_ctl(worker_state->epoll_fd, EPOLL_CTL_ADD, sockfd, &event) < 0) {
        perror("epoll_ctl");
        return;
    }
    return;
}

// run in worker thread
void worker_thread(worker_state* my_state) {
    const int& epollfd = my_state->epoll_fd;
    const auto& events = my_state->events;

    int ret;
    while (true) {
        // num_events: how many sockets are ready for read
        int num_events = epoll_wait(epollfd, events, 1024, -1);
        if (num_events < 0) {
            perror("epoll_wait");
            return;
        }
        // deal with these sockets one by one
        for (int i = 0; i < num_events; i++) {
            // incoming connection from client
            if (events[i].events & EPOLLIN) {
                int sockfd = events[i].data.fd;
                int32_t operation_type;
                int64_t argument;
                ret = recv_msg(sockfd, &operation_type, &argument);
                if (ret < 0) {
                    perror("recv_msg");
                    return;
                }
                if (operation_type ==
                    sockets::message_OperationType::message_OperationType_ADD) {
                    number += argument;
                } else if (operation_type == sockets::message_OperationType::
                                                 message_OperationType_SUB) {
                    number -= argument;
                } else if (operation_type ==
                           sockets::message_OperationType::
                               message_OperationType_TERMINATION) {
                    // send number to client
                    // load to a snapshot
                    auto number_snapshot = number.load();
                    send_msg(sockfd,
                             sockets::message_OperationType::
                                 message_OperationType_COUNTER,
                             number_snapshot);
                             // close socket
                    close(sockfd);
                    // unregister this socket from epoll
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, nullptr);
                    // print to stdout
                    console_mtx.lock();
                    std::cout << number_snapshot << std::endl;
                    console_mtx.unlock();
                }
            }
        }
    }
}

int main(int args, char* argv[]) {
    if (args < 3) {
        std::cerr << "usage: ./server <numThreads> <port>\n";
        exit(1);
    }

    int numThreads = std::atoi(argv[1]);
    int port = std::atoi(argv[2]);
    server_state state;
    init_listener(port, &state);
    state.work_thread_num = numThreads;
    std::vector<worker_state> workers(numThreads);

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; i++) {
        init_worker(&workers[i], i);
        threads.emplace_back(worker_thread, &workers[i]);
    }

    // the main thread accepts connections and assigns them to workers
    while (true) {
        int sockfd = accept_connection(state.listen_fd);
        if (sockfd < 0) {
            perror("accept");
            return 1;
        }
        assign_socket_to_worker(sockfd, &workers[sockfd % numThreads]);
    }

    for (auto& t : threads) {
        t.join();
    }

    return 0;
}
