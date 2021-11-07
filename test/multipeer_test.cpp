#include <gtest/gtest.h>
#include "../src/client.cpp"
#include<fstream>


void callback(std::vector<int> &pieces, peer::downloader &d) {
    auto tid = std::this_thread::get_id();
    std::cout<<tid<<" hello\n";
    for(int i=0; i<pieces.size(); ++i) {
        std::cout<<tid<<" downloading piece "<<pieces[i]<<"\n";
        d.download(pieces[i]);
        sleep(2);
    }
}

TEST(multi_peer, test1) {
    std::fstream fs("../.test/data_file_dwn",
                std::ios_base::binary | std::ios_base::in | std::ios_base::out);
    std::recursive_mutex m;

    // std::fstream fs2("../.test/data_file_s2",
    //             std::ios_base::binary | std::ios_base::in | std::ios_base::out);
    // std::mutex m2;
    
    peer::tsafe_fstream tsfs(&fs, &m);

    peer::downloader d1(net_socket::ipv4_addr("127.0.0.1"), 9000, tsfs);
    peer::downloader d2(net_socket::ipv4_addr("127.0.0.1"), 9001, tsfs);

    std::vector<int> pieces1 = {0, 2, 4, 6, 8, 10};
    std::vector<int> pieces2 = {1, 3, 5, 7, 9, 11};
    std::thread t1(callback, std::ref(pieces1), std::ref(d1));
    std::thread t2(callback, std::ref(pieces2), std::ref(d2));

    if(t1.joinable()) t1.join();
    if(t2.joinable()) t2.join();

}