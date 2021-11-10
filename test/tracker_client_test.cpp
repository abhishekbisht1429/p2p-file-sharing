#include <gtest/gtest.h>
#include "../src/client.cpp"
#include<fstream>

TEST(multi_peer, test1) {
    net_socket::sock_addr addr(net_socket::ipv4_addr("127.0.0.1"), 9000);
    peer::tracker_client tc(addr);

    try {
        tc.login("abhishek", "root");
        tc.create_group("group 1");
    } catch(std::exception &e) {}
    tc.get_peers("f1", "group 1");
}