#include<iostream>
#include<fstream>
#include<thread>
#include<mutex>
#include<queue>
#include<functional>
#include<cstring>
#include<vector>
#include "http.cpp"

namespace peer {
    constexpr static size_t PIECE_LENGTH = 4;
    class tracker_client {

    };

    class invalid_piece_no_exception : public std::exception {
        public:
        const char *what() {
            return "invalid piece id";
        }
    };

    class failed_to_open_file_exception : public std::exception {
        public:
        const char *what() {
            return "failed to open file\n";
        }
    };

    class sha_mismatch_exception : public std::exception {
        public:
        const char *what() {
            return "sha of the received piece does not match";
        }
    };

    class tsafe_fstream {
        public:
        std::recursive_mutex *m = nullptr;
        std::fstream *fs = nullptr;
        
        tsafe_fstream(std::fstream *fs, std::recursive_mutex *m): fs(fs), m(m) {
            std::ios_base::iostate exceptionMask = fs->exceptions() | std::fstream::failbit;
            fs->exceptions(exceptionMask);
        }

        tsafe_fstream() {}

        template<typename T>
        tsafe_fstream &read(T *t, std::fstream::pos_type pos, std::streamsize count) {
            std::lock_guard<std::recursive_mutex> lock(*m);
            std::cout<<"seeking to "<<pos<<"\n";
            fs->seekg(pos);
            std::cout<<"reading: "<<count<<"\n";
            fs->read(t, count);
            return *this;
        }

        template<typename T>
        tsafe_fstream &write(T *t, std::fstream::pos_type pos, std::streamsize count) {
            std::lock_guard<std::recursive_mutex> lock(*m);
            fs->seekp(pos);
            std::cout<<"output pos : "<<fs->tellp()<<"\n";
            fs->write(t, count);
            return *this;
        }

        std::fstream::pos_type gcount() const {
            std::lock_guard<std::recursive_mutex> lock(*m);
            return fs->gcount();
        }

        std::fstream::pos_type tellg() {
            std::lock_guard<std::recursive_mutex> lock(*m);
            return fs->tellg();
        }

        tsafe_fstream &seekg(std::fstream::pos_type pos) {
            std::lock_guard<std::recursive_mutex> lock(*m);
            fs->seekg(pos);
            return *this;
        }

        tsafe_fstream &seekg(std::fstream::off_type off, std::ios_base::seekdir dir) {
            std::lock_guard<std::recursive_mutex> lock(*m);
            fs->seekg(off, dir);
            return *this;
        }

        tsafe_fstream &seekp(std::fstream::off_type off, std::ios_base::seekdir dir) {
            std::lock_guard<std::recursive_mutex> lock(*m);
            fs->seekp(off, dir);
            return *this;
        }

        tsafe_fstream &seekp(std::fstream::off_type off) {
            std::lock_guard<std::recursive_mutex> lock(*m);
            fs->seekp(off);
            return *this;
        }

        bool fail() const {
            std::lock_guard<std::recursive_mutex> lock(*m);
            return fs->fail();
        }

    };

    /* each thread will get its own downloader */
    class downloader {
        /* id of current piece being downloaded */
        tsafe_fstream &tsfs;
        int piece_id;
        http::http_client client;

        public:
        /* downloader assumes that abs_fname has already been allocated required space */
        downloader(net_socket::ipv4_addr addr, uint16_t port, tsafe_fstream& tsfs): 
                    piece_id(-1), tsfs(tsfs) {
            try {
                client.connect(addr, port);
            } catch(http::http_exception he) {
                std::cout<<he.what()<<"\n";
                throw he;
            } catch(std::fstream::failure f) {
                std::cout<<f.what()<<"\n";
                throw f;
            } catch(std::exception e) {
                std::cout<<e.what()<<"\n";
                throw e;
            }
        }

        ~downloader() {
            client.disconnect();
        }

        void write_piece(int piece_id, bstring data) {

            std::cout<<"\n-------------- writing piece to disk --------------\n";
            std::cout<<"piece id: "<<piece_id<<"\n";
            std::cout<<"piece size: "<<data.size()<<"\n";
            for(int i=0; i<data.size(); ++i)
                std::cout<<(int)data[i]<<" ";
            std::cout<<"\n";

            long long pos = piece_id * PIECE_LENGTH;
            std::cout<<"pos "<<pos<<"\n";
            tsfs.write((char *)data.c_str(), pos, data.size());
            std::cout<<"\n---------------------written-----------------------\n";
        }

        bool is_valid(int piece_id, bstring &&piece) {
            /* verify sha here */
            return true;
        }

        void download(int piece_id) {
            this->piece_id = piece_id;

            /* create request to download piece */
            http::request req;
            req.set_method(http::method::GET);
            req.set_resource("/piece");
            req.set_version(http::version::HTTP_2_0);
            req.add_header("Content-Length", "0");
            req.add_header("Piece-Id", std::to_string(piece_id));

            auto res = client.send_request(req);
            // client.send_request_async<downloader>(req, callback, *this);

            if(!is_valid(piece_id, res.get_body()))
                throw sha_mismatch_exception();

            write_piece(piece_id, res.get_body());
        }
    };

    /* there will be a single upload server (threading is handled there already) */
    class upload_server {
        http::http_server server;
        std::string abs_fname;
        tsafe_fstream &tsfs;
        char buf[PIECE_LENGTH];

        /* Function to read piece id from disk */
        bstring read_piece(int piece_id) {
            std::cout<<"reading piece\n";
            /* check if piece id is valid */
            long long pos = piece_id*PIECE_LENGTH;
            std::cout<<"pos "<<pos<<"\n";
            tsfs.seekg(0, std::ios_base::end);
            // if(tsfs.fail())
            //     throw piece_read_exception(std::strerror(errno));
            long long fsize = tsfs.tellg();
            std::cout<<"fsize "<<fsize<<"\n";
            tsfs.seekg(0);

            std::cout<<"checking\n";
            
            if(pos >= fsize)
                throw invalid_piece_no_exception();
            
            /* calculate no of remaining bytes to read */
            size_t rem = fsize - pos;
            
            /* read the piece from the file */
            tsfs.read(buf, pos, std::min(PIECE_LENGTH, rem));
            int read_count = std::min(PIECE_LENGTH, rem);

            bstring bs;
            for(int i=0; i<read_count; ++i)
                bs += buf[i];

            return bs;
        }

        class callback {
            upload_server &userver;

            public:
            callback(upload_server &userver): userver(userver) {}

            http::response operator()(http::request req) {
                std::cout<<"callback called\n";
                int piece_id;
                http::response res;
                try {
                    piece_id = stoi(req.get_header("Piece-Id"));
                    std::cout<<"piece_id"<<piece_id<<"\n";
                    /* read piece from file */
                    bstring piece = userver.read_piece(piece_id);

                    std::cout<<"piece read\n"<<"\n";
                    /* construct response */
                    res.set_status(http::status::OK);
                    res.set_status_text("successful");
                    res.set_version(http::version::HTTP_2_0);
                    res.add_header("Content-Length", std::to_string(piece.size()));
                    res.set_body(piece);
                } catch(http::no_such_header_exception nshe) {
                    std::cout<<nshe.what()<<"\n";
                    res = http::response(http::status::BAD_REQUEST, "Piece-Id Required");
                } 
                // catch(piece_read_exception pre) {
                //     std::cout<<"piece read exception\n";
                //     std::cout<<pre.what()<<"\n";
                //     res = http::response(http::status::INTERNAL_SERVER_ERROR, "server error");
                // } 
                catch(invalid_piece_no_exception ipne) {
                    std::cout<<ipne.what()<<"\n";
                    res = http::response(http::status::BAD_REQUEST, "Invalid Piece No");
                } catch(std::fstream::failure f) {
                    std::cerr<<"io failure\n";
                    std::cerr<<f.what()<<"\n";
                    res = http::response(http::status::INTERNAL_SERVER_ERROR, "server error");
                } catch (std::exception e) {
                    std::cerr<<"unknown exception\n";
                    std::cerr<<e.what()<<"\n";
                    res = http::response(http::status::INTERNAL_SERVER_ERROR, "server error");
                }

                return res;
            }
        };



        public:
        upload_server(net_socket::ipv4_addr ip, uint16_t port, tsafe_fstream& tsfs): tsfs(tsfs) {

            try {
                server = http::http_server(ip, port);
                server.accept_clients<callback>(callback(*this));
            } catch(std::exception e) {
                std::cout<<"exception caught\n";
                std::cout<<e.what()<<"\n";
            }
        }
    };

    class peer {
        std::priority_queue<int, std::vector<int>, std::greater<int>> pieces;
        std::vector<net_socket::sock_addr> peer_addrs;


    };
};