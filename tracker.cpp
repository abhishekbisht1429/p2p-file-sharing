#include<iostream>
#include<thread>
#include"sqlite/sqlite3.h"

struct peer {
    
};

int main() {
    sqlite3 *db;
    int rc = sqlite3_open(".test/temp.db", &db);
    sqlite3_close(db);

    return 0;
}