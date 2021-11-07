#include<string>
#include<string_view>
#include<map>
#include<vector>
#include<thread>
#include "socket.cpp"

typedef std::basic_string<unsigned char> bstring;

bstring s2b(std::string str) {
    bstring bs;
    for(int i=0; i<str.size(); ++i)
        bs += (unsigned char)str[i];
    return bs;
}

std::string b2s(bstring bs) {
    std::string str;
    for(int i=0; i<bs.size(); ++i)
        str += (char)bs[i];
    return str;
}

namespace http {
    const static std::string CRLF = "\r\n";
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

    class http_exception : std::exception {
        std::string msg;
        public:
        http_exception(std::string msg) {
            this->msg = msg;
        }

        const char *what() {
            return msg.c_str();
        }
    };

    class content_length_missing_exception : public http_exception {
        public:
        content_length_missing_exception(): http_exception("missing Content-Length") {}
    };

    class no_such_header_exception : public http_exception {
        public:
        no_such_header_exception(): http_exception("no such header") {}
    };

    class remote_end_closed_exception : public http_exception {
        public:
        remote_end_closed_exception() : http_exception("remote end closed") {}
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
            std::vector<std::string> vec = tokenize(_header, ":");
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
        bstring body;

        public:
        request(method _method, std::string resource, version _version,
                std::map<std::string, header> &headers, bstring body):
                    _method(_method), resource(resource), headers(headers), _version(_version),
                    body(body) {}
        request() {}
        
        bstring serialize() {
            bstring bstr;
            bstr += s2b(to_string(_method));
            bstr += s2b(SPACE);
            bstr += s2b(this->resource);
            bstr += s2b(SPACE);
            bstr += s2b(to_string(_version));
            bstr += s2b(CRLF);

            for(auto &p : headers) {
                bstr += s2b(p.second.serialize() + CRLF);
            }

            bstr += s2b(CRLF);

            bstr += body;

            return bstr;
        }

        static request deserialize(bstring req) {
            try {
                int pos = req.find(s2b(CRLF+CRLF));
                std::string req_head = b2s(req.substr(0, pos+1));
                bstring body = req.substr(pos+4);

                /* deserialize header */
                std::vector<std::string> lines = tokenize(req_head, CRLF);
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

        void set_method(method m) {
            this->_method = m;
        }

        void set_resource(std::string res) {
            this->resource = res;
        }
        
        void set_version(version v) {
            this->_version = v;
        }

        void add_header(std::string key, std::string value) {
            headers[key] = header(key, value);
        }

        void set_body(bstring body) {
            this->body = body;
        }

        void remove_header(std::string key) {
            if(headers.find(key) != headers.end())
                headers.erase(key);
        }

        method get_method() {
            return _method;
        }

        version get_version() {
            return _version;
        }

        std::string get_resource() {
            return resource;
        }

        std::map<std::string, header> get_header_map() {
            return headers;
        }

        std::string get_header(std::string key) {
            if(headers.find(key) == headers.end()) {
                throw no_such_header_exception();
            }

            return headers[key].value;
        }

        bstring get_body() {
            return body;
        }
    };

    /*
        reponse status
    */
    enum class status {
        OK = 200,
        NOT_FOUND = 404,
        BAD_REQUEST = 400,
        LENGTH_REQ = 411,
        INTERNAL_SERVER_ERROR = 500,
        UNDEFINED = 1000
    };

    status to_status(std::string code) {
        if(code == "200")
            return status::OK;
        else if(code == "404")
            return status::NOT_FOUND;
        else if(code == "400")
            return status::BAD_REQUEST;
        else if(code == "411")
            return status::LENGTH_REQ;
        else if(code == "500")
            return status::INTERNAL_SERVER_ERROR;
        else
            return status::UNDEFINED;
    }

    std::string to_string(status code) {
        switch(code) {
            case status::OK:
                return "200";
            case status::NOT_FOUND:
                return "404";
            case status::BAD_REQUEST:
                return "400";
            case status::LENGTH_REQ:
                return "411";
            case status::INTERNAL_SERVER_ERROR:
                return "500";
            default:
                return "1000";
        }
    }

    /*
        response
    */
    class response {
        version _version;
        status _status;
        std::string status_txt;
        std::map<std::string, header> headers;
        bstring body;

        public:
        response(version _version, status _status, std::string status_txt, 
                    const std::map<std::string, header> &headers, bstring body):
                        _version(_version), _status(_status), status_txt(status_txt), 
                        headers(headers), body(body) {}
        response(){}

        response(status _status, std::string status_txt) {
            this->_version = version::HTTP_2_0;
            this->_status = _status;
            this->status_txt = status_txt;
            add_header("Content-Length", "0");
        }
        
        bstring serialize() {
            bstring bstr;
            bstr += s2b(to_string(_version));
            bstr += s2b(SPACE);
            bstr += s2b(to_string(_status));
            bstr += s2b(SPACE);
            bstr += s2b(status_txt);
            bstr += s2b(CRLF);

            for(auto &p : headers) {
                bstr += s2b(p.second.serialize());
                bstr += s2b(CRLF);   
            }

            bstr += s2b(CRLF);
            bstr += body;

            return bstr;
        }

        static response deserialize(bstring res) {
            try {
                int pos = res.find(s2b(CRLF+CRLF));
                std::string res_head = b2s(res.substr(0, pos+1));
                bstring body = res.substr(pos+4);

                /* deserialize header */
                std::vector<std::string> lines = tokenize(res_head, CRLF);
                std::vector<std::string> segs = tokenize(lines[0], SPACE);
                version _version = to_version(segs[0]);
                status _status = to_status(segs[1]);
                std::string status_txt = "";
                for(int i=2; i<segs.size(); ++i)
                    status_txt += " " + segs[i];
                status_txt = status_txt.substr(1);


                std::map<std::string, header> headers;
                for(int i=1; i<lines.size(); ++i) {
                    header h = header::deserialize(lines[i]);
                    headers[h.key] = h;
                }

                return response(_version, _status, status_txt, headers, body);
            } catch(std::exception e) {
                std::string msg = e.what();
                throw parse_exception("Failed to parse request: " + msg);
            }
        }

        version get_version() {
            return _version;
        }

        status get_status() {
            return _status;
        }

        std::string get_status_text() {
            return status_txt;
        }

        std::map<std::string, header> get_header_map() {
            return headers;
        }

        std::string get_header(std::string key) {
            if(headers.find(key) == headers.end()) {
                throw no_such_header_exception();
            }

            return headers[key].value;
        }

        bstring get_body() {
            return body;
        }

        void set_version(version v) {
            this->_version = v;
        }

        void set_status(status s) {
            this->_status = s;
        }

        void set_status_text(std::string text) {
            this->status_txt = text;
        }

        void add_header(std::string key, std::string value) {
            headers[key] = header(key, value);
        }

        void remove_header(std::string key) {
            if(headers.find(key) != headers.end())
                headers.erase(key);
        }
        
        void set_body(bstring body) {
            this->body = body;
        }
    };


    /*
        http client
    */
    class http_client {
        net_socket::sock_addr server_sockaddr;
        net_socket::inet_socket sock;

        response read_response() {
            /* read response from server */
            bstring msg_header, msg_body;
            int pos_hend;
            char buf[1024];
            int read_count = 0;
            std::cout<<"reading\n";
            /* reading header part */
            while((read_count = sock.read_bytes(buf, 1024)) > 0) {
                std::cout<<"read count: "<<read_count<<"\n";
                bstring temp;
                for(int i=0; i<read_count; ++i)
                    temp += buf[i];
                
                /* find end of header */
                pos_hend = temp.find(s2b(CRLF+CRLF));
                if(pos_hend != std::string::npos) {
                    /* end of request header found */
                    msg_header += temp.substr(0, pos_hend);
                    msg_header += s2b(CRLF+CRLF);
                    
                    msg_body += temp.substr(pos_hend+4);
                    break;
                }
                msg_header += temp;
            }
            if(read_count == 0 && msg_header.size() == 0)
                throw remote_end_closed_exception();

            /* parsing request header */
            std::cout<<"parsing\n";
            std::cout<<"message_header: "<<b2s(msg_header)<<"\n";
            http::response res = http::response::deserialize(msg_header);
            /* get Content-Length */
            int len = 0;
            try {
                len = std::stol(res.get_header("Content-Length"));
            } catch(no_such_header_exception nshe) {
                std::cout<<nshe.what()<<"\n";
            }
            len -= msg_body.size();
            len = std::max(0, len);
            
            /* read rest of the body */
            while(len > 0) {
                read_count = sock.read_bytes(buf, 1024);
                for(int i=0; i<read_count; ++i)
                    msg_body += buf[i];
                len -= read_count;
            }
            res.set_body(msg_body);
            return res;
        }

        void write_request(request req) {
            bstring data = req.serialize();
            std::cout<<"data:\n"<<b2s(data)<<"\n";
            int to_send = data.size();
            std::cout<<"to_send: "<<to_send<<"\n";
            while(to_send>0) {
                std::cout<<"to_send: "<<to_send<<"\n";
                to_send -= sock.write_bytes((void*)data.c_str(), to_send);
            }
            std::cout<<"to_send: "<<to_send<<"\n";
            std::cout<<"client sent request\n";
            std::cout<<"waiting for response\n";
        }

        public:
        http_client() {}

        void connect(net_socket::ipv4_addr ip, uint16_t port) {
            try {
                server_sockaddr = net_socket::sock_addr(ip, port);
                sock.connect_server(server_sockaddr);
            } catch(std::exception e) {
                std::cout<<e.what()<<"\n";
                throw http_exception("http_client: unable to connect to server");
            }
        }

        void disconnect() {
            sock.close_socket();
        }

        response send_request(request req) {
            std::cout<<"client sending request\n";
            try {
                write_request(req);
                auto res = read_response();
                std::cout<<"response from server:\n";
                std::cout<<b2s(res.serialize())<<"\n";
                return res;
            } catch(http_exception he) {
                std::cout<<he.what()<<"\n";
                throw he;
            } catch(parse_exception pe) {
                std::cout<<pe.what()<<"\n";
                throw pe;
            } catch(std::exception e) {
                std::cout<<e.what()<<"\n";
                throw e;
            }
        }

        template<typename Callback, typename... Args>
        static void f(http_client &client, request &req, Callback &callback, Args... args) {
            response res = client.send_request(req);
            callback(res, args...);
        }

        template<typename Callback, typename... Args>
        void send_request_async(request req, Callback callback, Args... args) {
            std::thread t(f<Callback>, std::ref(*this), std::ref(req), std::ref(callback, args...));
            t.join();
        }
    };

    /*
        http server
    */
    class http_server {
        constexpr static int MAX_CONN = 10;
        net_socket::sock_addr server_sockaddr;
        net_socket::inet_server_socket server_sock;

        request read_request(net_socket::inet_socket &sock) {
            /* read request from cient */
            request req;
            bstring msg_header, msg_body;
            int pos_hend;
            char buf[1024];
            int read_count;
            std::cout<<"reading\n";

            /* reading header part */
            while((read_count = sock.read_bytes(buf, 1024)) > 0) {
                std::cout<<"read count: "<<read_count<<"\n";
                bstring temp;
                for(int i=0; i<read_count; ++i)
                    temp += buf[i];
                
                /* find end of header */
                pos_hend = temp.find(s2b(CRLF+CRLF));
                if(pos_hend != std::string::npos) {
                    /* end of request header found */
                    msg_header += temp.substr(0, pos_hend);
                    msg_header += s2b(CRLF+CRLF);
                    
                    msg_body += temp.substr(pos_hend+4);
                    break;
                }
                msg_header += temp;
            }
            if(read_count == 0 && msg_header.size() == 0)
                throw remote_end_closed_exception();

            /* parsing request header */
            std::cout<<"parsing\n";
            req = http::request::deserialize(msg_header);
            /* check if Content-Length is available */
            int len = 0;
            try {
                len = std::stol(req.get_header("Content-Length"));
            } catch(no_such_header_exception nshe) {
                throw content_length_missing_exception();
            }
            
            len -= msg_body.size();
            
            /* read rest of the body */
            while(len > 0) {
                read_count = sock.read_bytes(buf, 1024);
                for(int i=0; i<read_count; ++i)
                    msg_body += buf[i];
                len -= read_count;
            }
            req.set_body(msg_body);
            return req;
        }

        void write_response(response res, net_socket::inet_socket &sock) {
            bstring data = res.serialize();
            std::cout<<"data:\n"<<b2s(data)<<"\n";
            int to_send = data.size();
            std::cout<<"to_send: "<<to_send<<"\n";
            while(to_send>0) {
                std::cout<<"to_send: "<<to_send<<"\n";
                to_send -= sock.write_bytes((void*)data.c_str(), to_send);
            }
        }

        public:
        http_server(net_socket::ipv4_addr ip, uint16_t port) {
            try {
                server_sockaddr = net_socket::sock_addr(ip, port);
            } catch (std::exception e) {
                std::cout<<e.what()<<"\n";
                throw http_exception("http_server: failed to create socket");
            }
        }

        http_server() {}

        template<typename Callback>
        static void handle_client(net_socket::inet_socket &sock, Callback &callback, http_server &server) {
            std::cout<<"http_server: handling client by "<<std::this_thread::get_id()<<"\n";

            while(1) {
                try {
                        http::request req = server.read_request(sock);
                        std::cout<<"client request data: \n";
                        std::cout<<b2s(req.serialize())<<"\n";
                        std::cout<<"calling callback\n";
                        server.write_response(callback(req), sock);
                } catch(remote_end_closed_exception &rece) {
                    std::cout<<rece.what()<<"\n";
                    break;
                } catch(net_socket::socket_time_out_exception &stoe) {
                    std::cout<<stoe.what()<<"\n";
                    break;
                } catch(content_length_missing_exception clme) {
                    server.write_response(response(status::LENGTH_REQ, "Content-Length missing"), sock);
                } catch(http_exception &he) {
                    std::cout<<he.what()<<"\n";
                    break;
                }catch(parse_exception &pe) {
                    std::cout<<pe.what()<<"\n";
                    server.write_response(response(status::BAD_REQUEST, "Malformed HTTP request"), sock);
                } catch(std::exception &e) {
                    std::cout<<"http_client: unexpected exception\n";
                    std::cout<<e.what()<<"\n";
                    break;
                }
            }
            sock.close_socket();

            std::cout<<"http_server: client request handled\n";
            std::cout<<std::this_thread::get_id()<<" exiting\n";
        }

        template<typename Callback>
        void accept_clients(Callback callback) {
            try {
                server_sock.bind_name(server_sockaddr.ip, server_sockaddr.port);
                server_sock.sock_listen(MAX_CONN);
                std::vector<std::thread> threads;
                while(1) {
                    std::cout<<std::this_thread::get_id()<<" waiting for new connection\n";
                    net_socket::inet_socket sock = server_sock.accept_connection();
                    std::cout<<std::this_thread::get_id()<<" connected\n";

                    /* create a seperate thread to handle the client */
                    threads.push_back(std::thread(handle_client<Callback>, 
                                                    std::ref<net_socket::inet_socket>(sock),
                                                    std::ref<Callback>(callback),
                                                    std::ref(*this)));
                    // t.join();
                    // threads.push_back(t);
                }
                for(int i=0; i<threads.size(); ++i)
                    if(threads[i].joinable()) 
                        threads[i].join();
                std::cout<<std::this_thread::get_id()<<" exiting\n";
            } catch(std::exception e) {
                std::cout<<e.what()<<"\n";
                throw e;
            }
        }

        void stop() {
            server_sock.close_socket();
        }

    };
};