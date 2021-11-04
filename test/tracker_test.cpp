#include <gtest/gtest.h>
#include "../src/tracker.cpp"

TEST(tracker_test, test_listen) {
    ASSERT_NO_THROW(tracker::tracker t);
    tracker::tracker t;

    ASSERT_NO_THROW (
        t.listen_for_clients(net_socket::ipv4_addr("127.0.0.1"), 9000);
    );
}