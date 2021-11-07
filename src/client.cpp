#include<iostream>
#include<fstream>
#include<thread>
#include<mutex>
#include<queue>
#include<functional>
#include<cstring>
#include<vector>
#include<openssl/sha.h>
#include "http.cpp"

namespace peer {
    constexpr static size_t PIECE_LENGTH = 1;
    class tracker_client {

    };

    class invalid_piece_no_exception : public std::exception {
        public:
        const char *what() {
            return "invalid piece id";
        }
    };

    class invalid_response_exception : public std::exception {
        public:
        const char *what() {
            return "invalid response from server";
        }
    };

    class invalid_request_exception : public std::exception {
        public:
        const char *what() {
            return "bad request sent to server";
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

        ~tsafe_fstream() {
            m = nullptr;
            fs = nullptr;
        }

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
            std::cout<<"seekg\n";
            fs->seekg(pos);
            return *this;
        }

        tsafe_fstream &seekg(std::fstream::off_type off, std::ios_base::seekdir dir) {
            std::lock_guard<std::recursive_mutex> lock(*m);
            std::cout<<"seekg\n";
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
        tsafe_fstream tsfs;
        const std::vector<bstring> shs; //check if it is needed here or not
        // int piece_id;
        http::http_client client;

        public:
        /* downloader assumes that abs_fname has already been allocated required space */
        downloader(net_socket::ipv4_addr addr, uint16_t port, tsafe_fstream& tsfs, 
                    const std::vector<bstring> &shs): 
                    tsfs(tsfs), shs(shs) {
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

        downloader() {}

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
            bstring computed_hash;
            unsigned char md[SHA_DIGEST_LENGTH];
            SHA1(computed_hash.c_str(), piece.size(), md);

            // return shs[piece_id] == bstring(md);
            return true; //TODO: change this
        }

        void download(int piece_id) {
            // this->piece_id = piece_id;

            /* create request to download piece */
            http::request req;
            req.set_method(http::method::GET);
            req.set_resource("/piece");
            req.set_version(http::version::HTTP_2_0);
            req.add_header("Content-Length", "0");
            req.add_header("Piece-Id", std::to_string(piece_id));

            auto res = client.send_request(req);
            // client.send_request_async<downloader>(req, callback, *this);
            if(res.get_status() == http::status::BAD_REQUEST)
                throw invalid_request_exception();
            
            if(res.get_status() != http::status::OK)
                throw invalid_response_exception();


            if(!is_valid(piece_id, res.get_body()))
                throw sha_mismatch_exception();

            write_piece(piece_id, res.get_body());
        }
    };

    /* there will be a single upload server (threading is handled there already) */
    class upload_server {
        http::http_server server;
        net_socket::sock_addr addr;
        std::string abs_fname;
        tsafe_fstream tsfs;
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
                } catch(std::fstream::failure &f) {
                    std::cerr<<"io failure\n";
                    std::cerr<<f.what()<<"\n";
                    res = http::response(http::status::INTERNAL_SERVER_ERROR, "server error");
                } catch (std::exception &e) {
                    std::cerr<<"unknown exception\n";
                    std::cerr<<e.what()<<"\n";
                    res = http::response(http::status::INTERNAL_SERVER_ERROR, "server error");
                }

                return res;
            }
        };



        public:
        upload_server(net_socket::ipv4_addr ip, uint16_t port, tsafe_fstream& tsfs): tsfs(tsfs) {
            addr = net_socket::sock_addr(ip, port);
        }

        upload_server() {}

        void start() {
            try {
                server = http::http_server(addr.ip, addr.port);
                server.accept_clients<callback>(callback(*this));
            } catch(std::exception e) {
                std::cout<<"exception caught\n";
                std::cout<<e.what()<<"\n";
            }
        }
    };

    template<typename T, typename Container, typename Compare>
    class tsafe_pq {
        std::priority_queue<T, Container, Compare> pq;
        std::recursive_mutex m;

        public:
        int top() {
            std::lock_guard<std::recursive_mutex> lock(m);
            return pq.top();
        }

        void pop() {
            std::lock_guard<std::recursive_mutex> lock(m);
            pq.pop();
        }

        void push(T t) {
            std::lock_guard<std::recursive_mutex> lock(m);
            pq.push(t);
        }

        bool empty() {
            std::lock_guard<std::recursive_mutex> lock(m);
            return pq.empty();
        }

        int remove_top() {
            std::lock_guard<std::recursive_mutex> lock(m);
            int t = top();
            pop();
            return t;
        }

    };

    class peer {
        tsafe_pq<int, std::vector<int>, std::greater<int>> pieces;
        std::recursive_mutex m;
        std::vector<bstring> shs;
        std::vector<net_socket::sock_addr> peer_addrs;
        std::vector<std::thread> dwnld_threads;
        std::thread upload_server_thread;
        tracker_client tc;
        upload_server us;
        tsafe_fstream tsfs;
        std::fstream *fs_ptr;
        std::recursive_mutex *m_ptr;

        void print(std::string s) {
            std::lock_guard<std::recursive_mutex> lock(m);
            std::cout<<s;
        }

        static void upload_server_routine(peer &p) {
            p.us.start();
        }

        static void downloader_routine(peer &p, int peer_id) {
            std::cout<<"downloading\n";
            p.print("download thread started : "+std::to_string(peer_id)+"\n");
            
            /* create a downloader */
            try {
                downloader d(p.peer_addrs[peer_id].ip, p.peer_addrs[peer_id].port, p.tsfs, p.shs);
                p.print("create downloader\n");
                std::string tid = std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));
                while(!p.pieces.empty()) {
                    int piece_id = p.pieces.remove_top();
                    try {
                        p.print(tid+" trying to download piece "+std::to_string(piece_id)+"\n");
                        d.download(piece_id);
                        p.print(tid+" downloaded piece " + std::to_string(piece_id) + "\n");
                        sleep(1);
                    } catch(const std::exception &e) {
                        p.print(tid +" exception occured while downloading. "+std::to_string(piece_id)+"\n");
                        std::cerr<<e.what()<<"\n";
                        p.print("retrying\n");
                        p.pieces.push(piece_id);//imp
                        sleep(1);
                    }
                }
            } catch(std::exception e) {
                p.print(" exiting with failure\n");
                std::cout<<e.what()<<"\n";
            }
        }

        void start_upload_server() {
            upload_server_thread = std::thread(upload_server_routine,
                std::ref(*this));
        }

        void start_download() {
            for(int i=0; i<peer_addrs.size(); ++i) {
                dwnld_threads.push_back(std::thread(downloader_routine, 
                    std::ref(*this), i));
            }
        }

        void fetch_peers() {
            /*TODO: fetch peers from tracker */
            std::cout<<"Enter number of peers: ";
            int n;
            std::cin>>n;
            for(int i=0; i<n; ++i) {
                std::cout<<"Port: ";
                int p;
                std::cin>>p;
                peer_addrs.push_back(net_socket::sock_addr(net_socket::ipv4_addr("127.0.0.1"), p));
            }
            // peer_addrs.push_back(net_socket::sock_addr(net_socket::ipv4_addr("127.0.0.1"), 9000));
            // peer_addrs.push_back(net_socket::sock_addr(net_socket::ipv4_addr("127.0.0.1"), 9001));
            // peer_addrs.push_back(net_socket::sock_addr(net_socket::ipv4_addr("127.0.0.1"), 9002));
        }

        void load_meta() {
            /*TODO: load shs from the torrent file */

            /*TODO: update pieces priority queue from the meta file*/
            for(int i=0; i<100; ++i)
                pieces.push(i);
        }

        public:
        peer(std::string fname, std::string ip, int port) {
            fs_ptr = new std::fstream(fname, std::ios_base::in | std::ios_base::out | std::ios_base::binary);
            m_ptr = new std::recursive_mutex();
            tsfs = tsafe_fstream(fs_ptr, m_ptr);
            
            /* retrieve peer addresses */
            fetch_peers();

            /* load shs */
            load_meta();

            /* start the upload server */
            us = upload_server(net_socket::ipv4_addr(ip), (uint16_t)port, tsfs);
        }

        ~peer() {
            delete fs_ptr;
            delete m_ptr;
        }



        void start() {
            try {
                start_upload_server();
                start_download();

                for(int i=0; i<dwnld_threads.size(); ++i)
                    dwnld_threads[i].join();

                upload_server_thread.join();
            } catch(std::exception &e) {
                std::cout<<e.what()<<"\n";
            }
        }
    };
};