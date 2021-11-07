#include<sys/socket.h>
#include<poll.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<exception>
#include<string>
#include<iostream>
#include<thread>
#include<cstring>

namespace net_socket {
    /* socket exception */
    class socket_exception : public std::exception {
        std::string msg;
        int err_code;
        public:
        socket_exception(std::string msg): msg(msg) {}
        socket_exception(std::string msg, int err_code): msg(msg), err_code(err_code) {}
        const char *what() {
            return msg.c_str();
        }


    };

    class socket_time_out_exception : public socket_exception {
        public:
        socket_time_out_exception(): socket_exception("Socket timed out") {}
    };

        /* IP v4 address */
    class ipv4_addr {
        /* future functionality - add check for ip address correctness */
        std::string ip;
        public:
        ipv4_addr():ip("0.0.0.0") {}
        ipv4_addr(std::string ip): ip(ip) {}
        std::string get_ip() {
            return ip;
        }
    };

    /* socket address */
    struct sock_addr {
        ipv4_addr ip;
        uint16_t port;
        public:
        sock_addr(): ip("0.0.0.0"), port(0){}
        sock_addr(ipv4_addr ip, uint16_t port): ip(ip), port(port) {}
    };

    /* socket  */
    class inet_socket {
        /* fd for socket */
        constexpr static int TIMEOUT = 10000000;
        int sock;
        sock_addr remote_addr;
        sock_addr local_addr;

        public:
        inet_socket() {
            sock = socket(PF_INET, SOCK_STREAM, 0);
            if(sock<0)
                throw socket_exception("failed to create socket");
        }

        inet_socket(int sock, ipv4_addr ip, uint16_t port, ipv4_addr local_ip, uint16_t local_port) {
            this->sock = sock;
            remote_addr = sock_addr(ip, port);
            local_addr = sock_addr(local_ip, local_port);
        }

        void connect_server(std::string ip, uint16_t port) {
            struct sockaddr_in server_name;
            server_name.sin_family = AF_INET;
            if(inet_aton(ip.c_str(), &(server_name.sin_addr)) < 0)
                throw socket_exception("failed to connect");
            server_name.sin_port = htons(port);
            
            if(connect(sock, (struct sockaddr*)&server_name, sizeof(server_name)) < 0)
                throw socket_exception("unable to connect");
        }

        void connect_server(sock_addr addr) {
            connect_server(addr.ip.get_ip(), addr.port);
        }

        int read_bytes(void *buf, int count) {
            std::cout<<"socket: reading bytes\n";
            std::cout<<std::this_thread::get_id()<<" has socket "<<sock<<"\n";
            pollfd pfd;
            pfd.fd = sock;
            pfd.events = POLLIN;
            int rc = poll(&pfd, 1, TIMEOUT);
            std::cout<<"rc "<<rc<<"\n";
            std::cout<<"err "<<std::strerror(errno)<<"\n";
            std::cout<<"POLL ERR "<<(pfd.revents & POLLERR)<<"\n";
            std::cout<<"POLLRDHUP "<<(pfd.revents & POLLRDHUP)<<"\n";
            std::cout<<"POLLPRI "<<(pfd.revents & POLLPRI)<<"\n";
            std::cout<<"POLLHUP "<<(pfd.revents & POLLHUP)<<"\n";
            std::cout<<"POLLIN "<<(pfd.revents & POLLIN)<<"\n";
            std::cout<<"POLLNVAL "<<(pfd.revents & POLLNVAL)<<"\n";
            // std::cout<<"POLLHUP "<<(pfd.revents & POLLHUP)<<"\n";
            // std::cout<<"POLLHUP "<<(pfd.revents & POLLHUP)<<"\n";
            // std::cout<<"POLLHUP "<<(pfd.revents & POLLHUP)<<"\n";
            
            /* TODO: handle all the revents flags */
            if(rc > 0 && pfd.revents & POLLIN) {
                int actual_count;
                if((actual_count = recv(sock, buf, count, 0)) < 0) {
                    std::cout<<std::this_thread::get_id()<<" failed to read"<<"\n";
                    throw socket_exception("failed to read");
                }
                return actual_count;
            } else if(rc == 0) {
                throw socket_time_out_exception();
            } else {
                throw socket_exception("error occured during polling");
            }
        }

        int write_bytes(void *buf, int count) {
            std::cout<<"socket: writing bytes\n";
            int actual_count;
            if((actual_count = write(sock, buf, count)) < 0) {
                std::cout<<"failed to write\n";
                throw socket_exception("failed to write");
            }
            return actual_count;
        }

        void close_socket() {
            std::cout<<"closing socket\n";
            close(sock);
        }
    };

    /* server socket */
    class inet_server_socket {
        /* socket descriptor */
        int sock;
        /* local address */
        sock_addr addr;

        public:
        inet_server_socket() {
            sock = socket(PF_INET, SOCK_STREAM, 0);
            if(sock<0)
                throw socket_exception("failed to create socket");
        }

        void bind_name(ipv4_addr addr, uint16_t port) {
            this->addr = sock_addr(addr, port);
            struct sockaddr_in name;
            name.sin_family = AF_INET;
            if(inet_aton(addr.get_ip().c_str(), &(name.sin_addr)) < 0)
                throw socket_exception("invalid ip address");
            name.sin_port = htons(port);
            
            if(bind(sock, (struct sockaddr*)&name, sizeof(name)) < 0)
                throw socket_exception("failed to bind name");
            
            std::cout<<"successfully bound\n";
        }

        void bind_addr(sock_addr addr) {
            bind_name(addr.ip, addr.port);
        }

        void sock_listen(int n) {
            if(listen(sock, n) < 0)
                throw socket_exception("unable to start listening");
        }

        inet_socket accept_connection() {
            sockaddr_in client_addr;
            socklen_t size = sizeof(client_addr);
            int new_sock = accept(sock, (struct sockaddr*)&client_addr, &size); 
            if(new_sock<0)
                throw socket_exception("failed to accept connection");
            
            /* extract client info */
            ipv4_addr client_ip = ipv4_addr(inet_ntoa(client_addr.sin_addr));
            uint16_t client_port = ntohs(client_addr.sin_port);

            /* create a inet_socket object for communication */
            inet_socket socket_obj(new_sock, client_ip, client_port, addr.ip, addr.port);
            
            return socket_obj;
        }

        void close_socket() {
            close(sock);
        }
    };

    
};