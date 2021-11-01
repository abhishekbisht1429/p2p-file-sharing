#include<string>
#include<string_view>
#include<map>
#include<vector>

namespace http {
    const static std::string NEW_LINE = "\r\n";
    const static std::string SPACE = " ";

    /* parse exception */
    class parse_exception : std::exception {
        std::string msg;

        public:
        parse_exception(std::string msg): msg(msg) {}

        const char *what() {
            return msg.c_str();
        }
    };

    /* Http methods */
    enum class method {
        GET,
        POST,
        UNDEFINED
    };

    /* http version */
    enum class version {
        HTTP_1_0,
        HTTP_1_1,
        HTTP_2_0,
        UNDEFINED
    };

    /* 
        utility functions
    */
    std::string to_string(method m) {
        switch(m) {
            case method::GET:
                return "GET";
            case method::POST:
                return "POST";
            default:
                return "";
        }
    }

    method to_method(std::string m) {
        if(m == "GET")
            return method::GET;
        else if(m == "POST")
            return method::POST;
        else
            return method::UNDEFINED;
    }

    std::string to_string(version v) {
        switch(v) {
            case version::HTTP_1_0:
                return "HTTP/1.0";
            case version::HTTP_1_1:
                return "HTTP/1.1";
            case version::HTTP_2_0:
                return "HTTP/2.0";
            default:
                return "";
        }
    }

    version to_version(std::string v) {
        if(v == "HTTP/1.0")
            return version::HTTP_1_0;
        else if(v == "HTTP/1.1")
            return version::HTTP_1_1;
        else if(v == "HTTP/2.0")
            return version::HTTP_2_0;
        else
            return version::UNDEFINED;
    }

    std::string ltrim(const std::string &s) {
        size_t start = s.find_first_not_of(" \n\r\t\f\v");
        return (start == std::string::npos) ? "" : s.substr(start);
    }
    
    std::string rtrim(const std::string &s) {
        size_t end = s.find_last_not_of(" \n\r\t\f\v");
        return (end == std::string::npos) ? "" : s.substr(0, end + 1);
    }
    
    std::string trim(const std::string &s) {
        return rtrim(ltrim(s));
    }

    std::vector<std::string> tokenize(std::string str, std::string delim) {
        std::string temp;
        std::vector<std::string> vec;
        int i=0;
        while(i<str.size()) {
            if(str.substr(i, delim.size()) == delim) {
                vec.push_back(temp);
                temp.clear();
                i += delim.size();
            } else {
                temp += str[i];
                ++i;
            }
        }
        if(temp.size() > 0)
            vec.push_back(temp);

        return vec;
    }

    /*
        request header
    */
    struct header {
        std::string key;
        std::string value;

        header() {}
        header(std::string key, std::string value): key(key), value(value) {}

        std::string serialize() {
            std::string str = "";
            str += this->key;
            str += ":"+SPACE;
            str += this->value;
            return str;
        }

        static header deserialize(std::string _header) {
            std::string key, value;
            std::vector<std::string> vec = tokenize(_header, ":"+SPACE);
            key  = vec[0];
            value = trim(vec[1]);

            return header(key, value);
        }
        
    };

    /*
        request
    */
    class request {
        method _method;
        version _version;
        std::string resource;
        std::map<std::string, header> headers;
        std::string body;

        public:
        request(method _method, std::string resource, version _version,
                std::map<std::string, header> &headers, std::string body):
                    _method(_method), resource(resource), headers(headers), _version(_version),
                    body(body) {}
        
        std::string serialize() {
            std::string str = "";
            str += to_string(_method);
            str += SPACE;
            str += this->resource;
            str += SPACE;
            str += to_string(_version);
            str += NEW_LINE;

            for(auto &p : headers) {
                str += p.second.serialize() + NEW_LINE;
            }

            str += NEW_LINE;

            str += body;

            return str;
        }

        static request deserialze(std::string req) {
            try {
                int pos = req.find_first_of(NEW_LINE+NEW_LINE);
                std::string req_head = req.substr(0, pos+1);
                std::string body = req.substr(pos+2);

                /* deserialize header */
                std::vector<std::string> lines = tokenize(req_head, NEW_LINE);
                std::vector<std::string> segs = tokenize(lines[0], SPACE);
                method m = to_method(segs[0]);
                std::string resource = segs[1];
                version v = to_version(segs[2]);

                std::map<std::string, header> headers;
                for(int i=1; i<lines.size(); ++i) {
                    header h = header::deserialize(lines[i]);
                    headers[h.key] = h;
                }

                return request(m, resource, v, headers, body);
            } catch(std::exception e) {
                std::string msg = e.what();
                throw parse_exception("Failed to parse request: " + msg);
            }
        }
    };
};