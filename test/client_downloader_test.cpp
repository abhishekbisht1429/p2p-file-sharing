#include <gtest/gtest.h>
#include "../src/client.cpp"
#include<fstream>

void update_callback(long long id, peer::bitfield bf) {}

TEST(client_test, downloader_test) {
    ASSERT_NO_THROW(
        std::vector<bstring> shas;
        std::fstream fs("../.test/data_file_dwn", 
            std::ios_base::binary | std::ios_base::in | std::ios_base::out);
        std::recursive_mutex m;
        std::cout<<"Port: ";
        uint16_t port = 9000;
        // std::cin>>port;
        peer::tsafe_fstream tsfs(&fs, &m);
        peer::downloader dw(net_socket::ipv4_addr("127.0.0.1"), port, tsfs, shas, update_callback);
        std::cout<<"Piece Id: ";
        int piece_id;
        std::cin>>piece_id;
        dw.download(piece_id);
    );
}