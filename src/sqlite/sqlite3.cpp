#include"sqlite3.h"
#include<string>
#include<vector>
#include<iostream>
#include<thread>

class sqlite_client {
    sqlite3 *db;
    std::thread bg_worker;

    public:
    sqlite_client() {
        sqlite3_open("../../.test/tracker.db", &db);
    }

    void exec(std::string query) {
        std::cout<<"executing " + query<<"\n";
    }



    ~sqlite_client() {
        sqlite3_close(db);
    }
};
