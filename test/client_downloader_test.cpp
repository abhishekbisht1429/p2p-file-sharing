#include <gtest/gtest.h>
#include "../src/client.cpp"
#include<fstream>

TEST(client_test, downloader_test) {
    ASSERT_NO_THROW(
        std::fstream fs("../.test/data_file_dwn", 
            std::ios_base::binary | std::ios_base::in | std::ios_base::out);
        std::recursive_mutex m;
        std::cout<<"created instances of resources\n";
        peer::downloader dw(net_socket::ipv4_addr("127.0.0.1"), 9000, &fs, &m);
        dw.download(20);
    );
}