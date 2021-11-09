#include <gtest/gtest.h>
#include "../src/tracker.cpp"

TEST(tracker_test, test_listen) {

    ASSERT_NO_THROW (
        std::string inp;
        std::cout<<"clear db ? (y/n): ";
        getline(std::cin, inp);
        if(inp.size()>0 && inp[0] == 'y')
            sqlite::clear_database();
        
        tracker::tracker t(net_socket::ipv4_addr("127.0.0.1"), 9000);
        t.start_server();
    );
}