#include <gtest/gtest.h>
#include "../src/http.cpp"

class callback {
    public:
    http::response operator()(http::request req) {
        std::cout<<"callback: handling client\n";
        http::response res;
        res.set_version(http::version::HTTP_2_0);
        res.set_status(http::status::OK);
        res.set_status_text("successful");

        std::string body = "this is response from server\n";
        res.add_header("Content-Length", std::to_string(body.size()));
        res.set_body(s2b(body));

        std::cout<<"callback: client request handled\n";

        return res;
    }
};

TEST(http_server, http_server_test) {
    ASSERT_NO_THROW (
        http::http_server server(net_socket::ipv4_addr("127.0.0.1"), 9000);
        server.accept_clients(callback());
    );
}