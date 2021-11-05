#include <gtest/gtest.h>
#include<unordered_map>
#include "../src/http.cpp"

TEST(http_client, http_client_test) {
    ASSERT_NO_THROW (
        http::http_client client;
        client.connect(net_socket::ipv4_addr("127.0.0.1"), 9000);
        
        http::request req;
        req.set_method(http::method::GET);
        req.set_resource("/resource");
        req.set_version(http::version::HTTP_2_0);
        req.add_header("Content-Type", "Text");
        req.add_header("header 1", "hi");
        req.set_body("This is request body\n");
        // req.add_header("Content-Length", std::to_string(req.get_body().size()));
        EXPECT_NO_THROW (
            for(int i=0; i<3; ++i) {
                client.send_request(req);
                sleep(5);
            }
        );
        client.disconnect();
    );
}