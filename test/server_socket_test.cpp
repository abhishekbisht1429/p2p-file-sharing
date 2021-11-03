#include <gtest/gtest.h>
#include "../src/socket.cpp"

TEST(socket_test, socket_creation_test) {
    net_socket::inet_server_socket socket;

    ASSERT_NO_THROW (
        socket.bind_name(net_socket::ipv4_addr("127.0.0.1"), 9000);
    );

    ASSERT_NO_THROW (
        socket.sock_listen(10);
    );

    net_socket::inet_socket sock;
    ASSERT_NO_THROW (
        sock = socket.accept_connection();
    );

    char c;
    EXPECT_NO_THROW (
        sock.read_bytes(&c, 1);
    );
    
    EXPECT_EQ(c, 'a');
    std::cout<<"input from client "<<c<<"\n";

    c = 'b';
    EXPECT_NO_THROW (
        sock.write_bytes(&c, 1);
    );

    ASSERT_NO_THROW (
        sock.close_socket();
    );
}



