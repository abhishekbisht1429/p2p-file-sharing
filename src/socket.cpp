#include<sys/socket.h>
#include<poll.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<exception>
#include<string>
#include<vector>
#include<iostream>
#include<thread>
#include<cstring>

namespace util {
    std::string ltrim(const std::string &s) {
        size_t start = s.find_first_not_of(" \n\r\t\f\v");
        return (start == std::string::npos) ? "" : s.substr(start);
    }
    
    std::string rtrim(const std::string &s) {
        size_t end = s.find_last_not_of(" \n\r\t\f\v");
        return (end == std::string::npos) ? "" : s.substr(0, end + 1);
    }
    
    std::string trim(const std::string &s) {
        return rtrim(ltrim(s));
    }

    std::vector<std::string> tokenize(std::string str, std::string delim) {
        std::string temp;
        std::vector<std::string> vec;
        int i=0;
        while(i<str.size()) {
            if(str.substr(i, delim.size()) == delim) {
                vec.push_back(temp);
                temp.clear();
                i += delim.size();
            } else {
                temp += str[i];
                ++i;
            }
        }
        if(temp.size() > 0)
            vec.push_back(temp);

        return vec;
    }
};

namespace net_socket {
    /* socket exception */
    class socket_exception : public std::exception {
        std::string msg;
        int err_code;
        public:
        socket_exception() {}
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

    class invalid_socket_descriptor_exception : public socket_exception {};

    class conn_refused_exception: public socket_exception {};

    class conn_reset_exception: public socket_exception {};

    class invalid_buf_addr_exception : public socket_exception {};

    class interrupt_exception : public socket_exception {};

    class no_connection_mode_execption : public socket_exception {};

    class invalid_args_exception : public socket_exception {};

    class mem_alloc_failed_exception : public socket_exception {};

    class sock_not_connected_exception : public socket_exception {};

    class not_sock_exception : public socket_exception {};

    class invalid_msgsize_exception : public socket_exception {};

    class is_connected_exception : public socket_exception {};

    class no_buf_available_exception : public socket_exception {};

    class flag_not_supported_exception : public socket_exception {};

    class permisison_exception : public socket_exception {};

    class address_in_use_exception : public socket_exception {};

    class address_not_available_exception : public socket_exception {};

    class address_format_unsupported_exception : public socket_exception {};

    class network_unreachable_exception : public socket_exception {};

    class protocol_unsupported_exception : public socket_exception {};

    class broken_pipe_exception : public socket_exception {};

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
        long long uid;
        public:
        sock_addr(): ip("0.0.0.0"), port(0){
            uid = 0;
        }
        sock_addr(ipv4_addr ip, uint16_t port): ip(ip), port(port) {
            auto parts = util::tokenize(ip.get_ip(), ".");
            long long p1 = std::stoll(parts[0]);
            long long p2 = std::stoll(parts[1]);
            long long p3 = std::stoll(parts[2]);
            long long p4 = std::stoll(parts[3]);
            long long p5 = port;
            uid = p1 | (p2<<8) | (p3<<16) | (p4<<24) | (p5<<32); 
        }

        sock_addr(long long uid) {
            long long mask = 0x00000000000000ffl;
            long long p1 = uid & mask;
            mask <<= 8;
            long long p2 = (uid & mask)>>8;
            mask <<=8;
            long long p3 = (uid & mask)>>16;
            mask <<=8;
            long long p4 = (uid & mask)>>24;
            
            mask = (0x000000000000ffffl)<<32;
            long long p5 = (uid & mask)>>32;

            port = (uint16_t)port;
            ip = std::to_string(p1)+"."+std::to_string(p2)+
                "."+std::to_string(p3)+"."+std::to_string(p4);
            this->uid = uid;
        }

        long long get_uid() {
            return uid;
        }
    };

    /* socket  */
    class inet_socket {
        /* fd for socket */
        constexpr static int TIMEOUT = 10000000;
        int sock;
        sock_addr remote_addr;
        sock_addr local_addr;

        void throw_exception(int err) {
            if(err & EBADF) {
                throw invalid_socket_descriptor_exception();
            } else if(err & ECONNREFUSED) {
                throw conn_refused_exception();
            } else if(err & EFAULT) {
                throw invalid_buf_addr_exception();
            } else if(err & EINTR) {
                throw interrupt_exception();
            } else if(err & EINVAL) {
                throw invalid_args_exception();
            } else if(err & ENOMEM) {
                throw mem_alloc_failed_exception();
            } else if(err & ENOTCONN) {
                throw sock_not_connected_exception();
            } else if(err & ENOTSOCK) {
                throw not_sock_exception();
            } else if(err & ECONNRESET) {
                throw conn_reset_exception();
            } else if(err & EDESTADDRREQ) {
                throw no_connection_mode_execption();
            } else if(err & EMSGSIZE) {
                throw invalid_msgsize_exception();
            } else if(err & ENOBUFS) {
                throw no_buf_available_exception();
            } else if(err & EOPNOTSUPP) {
                throw flag_not_supported_exception();
            } else if(err & EISCONN) {
                throw is_connected_exception();
            } else if(err & (EACCES | EPERM)) {
                throw permisison_exception();
            } else if(err & EADDRINUSE) {
                throw address_in_use_exception();
            } else if(err & EADDRNOTAVAIL) {
                throw address_not_available_exception();
            } else if(err & EAFNOSUPPORT) {
                throw address_format_unsupported_exception();
            } else if(err & ENETUNREACH) {
                throw network_unreachable_exception();
            } else if(err & EPROTOTYPE) {
                throw protocol_unsupported_exception();
            } else if(err & EPIPE) {
                throw broken_pipe_exception();
            }
        }

        public:
        inet_socket() {}

        inet_socket(int sock, ipv4_addr ip, uint16_t port, ipv4_addr local_ip, uint16_t local_port) {
            this->sock = sock;
            remote_addr = sock_addr(ip, port);
            local_addr = sock_addr(local_ip, local_port);
        }

        void connect_server(std::string ip, uint16_t port) {
            /* create socket */
            sock = socket(PF_INET, SOCK_STREAM, 0);
            if(sock<0)
                throw socket_exception("failed to create socket");
            
            remote_addr = sock_addr(ipv4_addr(ip), port);

            /* obtain local addr */
            // struct sockaddr_in local_name;
            // socklen_t local_name_size = sizeof(local_name);
            // getsockname(sock, (struct sockaddr*)&local_addr, &(local_name_size));

            // ipv4_addr local_ip = ipv4_addr(inet_ntoa(local_name.sin_addr));
            // uint16_t local_port = ntohs(local_name.sin_port);

            /* create server name C lib obj */
            struct sockaddr_in server_name;
            server_name.sin_family = AF_INET;
            if(inet_aton(ip.c_str(), &(server_name.sin_addr)) < 0)
                throw socket_exception("failed to connect");
            server_name.sin_port = htons(port);
            
            if(connect(sock, (struct sockaddr*)&server_name, sizeof(server_name)) < 0) {
                throw_exception(errno);
            }
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
                    throw_exception(errno);
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
            try {
            if((actual_count = send(sock, buf, count, MSG_NOSIGNAL)) < 0) {
                throw_exception(errno);
            }
            } catch(std::exception &e) {
                std::cout<<e.what()<<"\n";
            }
            return actual_count;
        }

        sock_addr get_remoteaddr() {
            return remote_addr;
        }

        sock_addr get_localaddr() {
            return local_addr;
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