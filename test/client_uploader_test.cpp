#include <gtest/gtest.h>
#include "../src/client.cpp"
#include<fstream>

TEST(client_test, uploader_test) {
    ASSERT_NO_THROW(
        std::fstream fs("../.test/data_file", 
            std::ios_base::binary | std::ios_base::in | std::ios_base::out);
        std::recursive_mutex m;
        std::cout<<"Port: ";
        uint16_t port;
        std::cin>>port;
        peer::tsafe_fstream tsfs(&fs, &m);
        peer::upload_server us(net_socket::ipv4_addr("127.0.0.1"), port, tsfs);
        us.start();
    );
}