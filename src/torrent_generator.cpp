#include<iostream>
#include<fstream>
#include<openssl/sha.h>

int main(int argc, char**argv) {
    std::cout<<argc<<"\n";
    if(argc < 3) {
        std::cout<<"invalid number of args\n";
        return 0;
    }

    std::string src = argv[1];
    std::cout<<src<<"\n";
    std::string dest = argv[2];
    std::cout<<dest<<"\n";
    size_t chunk_len = std::stoll(std::string(argv[3]));
    std::cout<<chunk_len<<"\n";
    chunk_len *= 1024;

    std::ifstream in(src);
    std::ofstream out(dest);

    unsigned char buf[chunk_len];
    unsigned char md[SHA_DIGEST_LENGTH];
    int actual_read;
    while(!in.eof()) {
        in.read((char*)buf, chunk_len);
        actual_read = in.gcount();
        
        if(actual_read > 0) {
            SHA1(buf, actual_read, md);
            out.write((char *)md, SHA_DIGEST_LENGTH);
            std::cout<<"written "<<SHA_DIGEST_LENGTH<<"\n";
        }
    }
    in.close();
    out.close();

    std::cout<<"Saved to SHA at "<<dest<<"\n";
}