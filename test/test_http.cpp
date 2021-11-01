#include <gtest/gtest.h>
#include "../src/http.cpp"
#include<string>
#include<vector>

/* tokenize function test case */
TEST(test_http_util, test_tokenize) {
    std::string str = "this is test";
    std::vector<std::string> vec = http::tokenize(str, " ");

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

    http::request _req(_m, _res, _v, _h, _b);

    std::string m, res, v, h, b;
    m = "GET";
    res = "/tracker";
    v = "HTTP/2.0";
    h = std::string("Content-Type: application/json\r\n") + 
        "Content-Type2: binary/octet";
    b = "This is body\nof the request";

    std::string req_str = m + http::SPACE + res + http::SPACE + v +
        http::NEW_LINE + h + http::NEW_LINE + http::NEW_LINE + b;

    
    ASSERT_EQ(_req.serialize(), req_str);

    /* TODO : Comlete this test case */
}