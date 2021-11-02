#include<iostream>
#include<thread>
#include<vector>
#include<set>
#include"sqlite/sqlite_client.cpp"
#include "http.cpp"

namespace tracker {
    struct peer {
        std::string user_id;
        std::string passwd;
    };

    struct file {
        std::set<std::string> seeders;
        std::set<std::string> leechers;
        std::string fname;
    };

    struct group {
        std::string groud_id;
        std::string owner_id;
        std::vector<std::string> members;

    };

    class tracker {
        std::map<std::string, std::vector<group>> groups;

        /*
            When a tracker is intialized it reads the group info from 
            a local database 
        */
        tracker() {
            /* TODO: read group info from database */
        }
    };
}

int main() {

    return 0;
}