#include<iostream>
#include<thread>
#include<vector>
#include<set>
#include"sqlite/sqlite_client.cpp"
#include "http.cpp"
// #include "socket.cpp"

namespace tracker {
    struct peer {
        std::string id;
        std::string passwd;
    };

    struct file {
        std::set<std::string> seeders;
        std::set<std::string> leechers;
        std::string fname;
    };

    struct group {
        std::string id;
        std::string owner_id;
        std::vector<std::string> members;

    };

    class tracker {
        sqlite::group_client sqlite_gc;
        std::map<std::string, std::vector<group>> groups;

        public:
        /*
            When a tracker is intialized it reads the group info from 
            a local database 
        */
        tracker() {
            /* TODO: read group info from database */
            try {
                // auto table = sqlite_gc.get_group_list();

                // /* intialize groups */
                // for(auto &row : table) {
                //     group g;
                //     g.id = row[0];
                //     g.owner_id = row[1];
                //     groups[g.owner_id].push_back(g);
                // }
            } catch(std::exception e) {
                std::cout<<e.what()<<"\n";
            }
        }

        static void handle_client(net_socket::inet_socket &sock) {
            std::cout<<"handling client\n";
            char c;
            sock.read_bytes(&c, 1);
            std::cout<<"input from client : "<<c<<"\n";

            c = 'b';
            sock.write_bytes(&c, 1);
            sock.close_socket();
            std::cout<<"client request handled\n";
        }

        void listen_for_clients(net_socket::ipv4_addr ip, uint16_t port) {
            try {
                std::vector<std::thread> threads;
                net_socket::inet_server_socket server_sock;
                server_sock.bind_name(ip, port);
                server_sock.sock_listen(10);
                while(1) {
                    
                    net_socket::inet_socket sock = server_sock.accept_connection();

                    /* create a seperate thread to handle the client */
                    threads.push_back(std::thread(handle_client, 
                                        std::ref<net_socket::inet_socket>(sock)));
                }
            } catch(net_socket::socket_exception se) {
                std::cout<<se.what()<<"\n";
                throw se;
            }
        }
    };
};
