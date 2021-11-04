#include <gtest/gtest.h>
#include "../src/socket.cpp"

TEST(socket_test, socket_connection_test) {
    ASSERT_NO_THROW (
        net_socket::inet_socket sock;
    );
    net_socket::inet_socket sock;

    ASSERT_NO_THROW (
        sock.connect_server("127.0.0.1", 9000);
    );

    char c = 'a';
    EXPECT_NO_THROW (
        sock.write_bytes(&c, 1);
    );

    EXPECT_NO_THROW (
        sock.read_bytes(&c, 1);
    );

    EXPECT_EQ(c, 'b');

    std::cout<<"reply from server "<<c<<"\n";

    EXPECT_NO_THROW (
        std::cout<<sock.read_bytes(&c, 1)<<"\n";
    );

    EXPECT_NO_THROW (
        std::cout<<sock.read_bytes(&c, 1)<<"\n";
    );

    ASSERT_NO_THROW (
        sock.close_socket();
    );
}



