#include <gtest/gtest.h>
#include "../src/http.cpp"
#include<string>
#include<vector>

/* tokenize function test case */
TEST(test_http_util, test_tokenize) {
    std::string str = "this is test";
    std::vector<std::string> vec = util::tokenize(str, " ");

    ASSERT_EQ(vec.size(), 3);

    ASSERT_EQ(vec[0], "this");
    ASSERT_EQ(vec[1], "is");
    ASSERT_EQ(vec[2], "test");
}

/* header class test case */
TEST(test_http, test_header) {
    http::header h("Content-Type", "application/json");
    std::string h_str = "Content-Type: application/json";

    std::string s = h.serialize();

    ASSERT_EQ(s, h_str);

    http::header h2 = http::header::deserialize(h_str);
    ASSERT_EQ(h2.key, h.key);
    ASSERT_EQ(h2.value, h.value);

}

/* request class test case */
TEST(test_http, test_request) {
    http::method _m = http::method::GET;
    std::string _res = "/tracker";
    http::version _v = http::version::HTTP_2_0;
    std::map<std::string, http::header> _h;
    _h["Content-Type"] = http::header("Content-Type", "application/json");
    _h["Content-Type2"] = http::header("Content-Type2", "binary/octet");
    std::string _b = "This is body\nof the request";

    http::request _req(_m, _res, _v, _h, s2b(_b));

    std::string m, res, v, h, b;
    m = "GET";
    res = "/tracker";
    v = "HTTP/2.0";
    h = std::string("Content-Type: application/json\r\n") + 
        "Content-Type2: binary/octet";
    b = "This is body\nof the request";

    std::string req_str = m + http::SPACE + res + http::SPACE + v +
        http::CRLF + h + http::CRLF + http::CRLF + b;

    
    ASSERT_EQ(_req.serialize(), s2b(req_str));

    http::request req = http::request::deserialize(s2b(req_str));

    EXPECT_EQ(req.get_method(), _req.get_method());
    EXPECT_EQ(req.get_version(), _req.get_version());
    EXPECT_EQ(req.get_resource(), _req.get_resource());
    EXPECT_EQ(req.get_body(), _req.get_body());
    EXPECT_EQ(req.get_header_map()["Content-Type"].value, _req.get_header_map()["Content-Type"].value);
    EXPECT_EQ(req.get_header_map()["Content-Type2"].value, _req.get_header_map()["Content-Type2"].value);

}

/* response class test case */
TEST(test_http, test_response) {
    http::version _v = http::version::HTTP_2_0;
    http::status _s = http::status::OK;
    std::string _st = "successful";
    std::map<std::string, http::header> _h;
    _h["Content-Type"] = http::header("Content-Type", "application/json");
    _h["Content-Type2"] = http::header("Content-Type2", "binary/octet");
    std::string _b = "";

    http::response _res(_v, _s, _st, _h, s2b(_b));

    std::string v, s, st,  h, b;
    v = "HTTP/2.0";
    s = "200";
    st = "successful";
    h = std::string("Content-Type: application/json\r\n") + 
        "Content-Type2: binary/octet";
    b = "";

    std::string res_str = v + http::SPACE + s + http::SPACE + st + 
        http::CRLF + h + http::CRLF + http::CRLF + b;

    
    ASSERT_EQ(_res.serialize(), s2b(res_str));

    http::response res = http::response::deserialize(s2b(res_str));

    EXPECT_EQ(res.get_version(), _res.get_version());
    EXPECT_EQ(res.get_status(), _res.get_status());
    EXPECT_EQ(res.get_status_text(), _res.get_status_text());
    EXPECT_EQ(res.get_body(), _res.get_body());
    EXPECT_EQ(res.get_header_map()["Content-Type"].value, _res.get_header_map()["Content-Type"].value);
    EXPECT_EQ(res.get_header_map()["Content-Type2"].value, _res.get_header_map()["Content-Type2"].value);

}