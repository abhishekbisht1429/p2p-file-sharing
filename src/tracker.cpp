#include<iostream>
#include<thread>
#include<vector>
#include<set>
#include"sqlite/sqlite_client.cpp"
#include "http.cpp"
#include "socket.cpp"

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

        /*
            When a tracker is intialized it reads the group info from 
            a local database 
        */
        tracker() {
            /* TODO: read group info from database */
            auto table = sqlite_gc.get_group_list();

            /* intialize groups */
            for(auto &row : table) {
                group g;
                g.id = row[0];
                g.owner_id = row[1];
                groups[g.owner_id].push_back(g);
            }
        }

        void listen_for_clients(net_socket::ipv4_addr ip, int port) {

        }
    };
}

int main() {

    return 0;
}