#include <gtest/gtest.h>
#include "../src/client.cpp"
#include<fstream>

TEST(client_test, downloader_test) {
    ASSERT_NO_THROW(
        std::fstream fs("../.test/data_file_dwn", 
            std::ios_base::binary | std::ios_base::in | std::ios_base::out);
        std::recursive_mutex m;
        std::cout<<"Port: ";
        uint16_t port = 9000;
        // std::cin>>port;
        peer::downloader dw(net_socket::ipv4_addr("127.0.0.1"), port, &fs, &m);
        std::cout<<"Piece Id: ";
        int piece_id;
        std::cin>>piece_id;
        dw.download(piece_id);
    );
}