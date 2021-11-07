#include<gtest/gtest.h>
#include<openssl/sha.h>

TEST(sha, generate) {
    std::string original = "This is sample string";
    unsigned char md[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)original.c_str(), original.size(), md);
    
    std::string hash = (char *)md;

    std::cout<<"hash: "<<hash<<"\n";
    
}