#include<iostream>
#include<thread>
#include<mutex>
#include<queue>
#include "socket.cpp"

namespace peer {
    constexpr static size_t PIECE_LENGTH = 524288;
    class tracker_client {

    };

    class downloader {
        /* id of current piece being downloaded */
        int piece_id;
        net_socket::sock_addr server_sockaddr;
        net_socket::inet_socket socket;
        char buf[PIECE_LENGTH];

        downloader(net_socket::ipv4_addr addr, uint16_t port): piece_id(-1) {
            server_sockaddr = net_socket::sock_addr(addr, port);
            socket.connect_server(server_sockaddr);
        }

        ~downloader() {
            socket.close_socket();
        }

        void download(int piece_id) {
            this->piece_id = piece_id;

        }
    };

    class peer {
        std::priority_queue<int, std::greater<int>> pieces;
        std::vector<unsigned char*> buffer;

    };
};