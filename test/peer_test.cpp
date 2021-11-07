#include <gtest/gtest.h>
#include "../src/client.cpp"
#include<fstream>

TEST(multi_peer, test1) {
    std::string ip = "127.0.0.1";
    int port;
    std::string fname;
    std::string temp;

    // std::cout<<"IP: ";
    // std::getline(std::cin, ip);

    std::cout<<"Port: ";
    std::getline(std::cin, temp);
    port = stoi(temp);
    
    std::cout<<"Filename: ";
    std::getline(std::cin, fname);

    std::cout<<"start ?\n";
    std::getline(std::cin, temp);

    fname = "../.test/"+fname;
    peer::peer p(fname, ip, port);

    // peer::peer p("../.test/data_file", "127.0.0.1", 9000);
    p.start();
}