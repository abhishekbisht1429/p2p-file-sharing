#include <gtest/gtest.h>
#include "../src/client.cpp"
#include<fstream>

TEST(multi_peer, test1) {
    std::string ip = "127.0.0.1";
    int port;
    std::string fname;
    std::string gid;
    std::string temp;
    std::string work_dir;
    bool seeder;

    // std::cout<<"IP: ";
    // std::getline(std::cin, ip);

    std::cout<<"Port: ";
    std::getline(std::cin, temp);
    port = stoi(temp);
    
    std::cout<<"Base Dir ";
    std::getline(std::cin, work_dir);

    std::cout<<"Filename: ";
    std::getline(std::cin, fname);

    std::cout<<"Gid: ";
    std::getline(std::cin, gid);

    std::cout<<"Seeder: ";
    std::getline(std::cin, temp);
    seeder = (temp.size()>0) && temp[0] == 'y';


    std::cout<<"start ?\n";
    std::getline(std::cin, temp);

    net_socket::sock_addr tracker_addr(net_socket::ipv4_addr("127.0.0.1"), 9000);
    peer::peer p(work_dir, fname, gid, ip, port, tracker_addr, seeder);

    // peer::peer p("../.test/data_file", "127.0.0.1", 9000);
    p.start();
}