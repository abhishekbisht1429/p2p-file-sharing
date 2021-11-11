#include<iostream>
#include<fstream>
#include<thread>
#include<mutex>
#include<queue>
#include<functional>
#include<set>
#include<unordered_map>
#include<cstring>
#include<vector>
#include<openssl/sha.h>
#include "http.cpp"

namespace peer {
    constexpr static size_t PIECE_LENGTH = 524288;

    class invalid_piece_no_exception : public std::exception {
        public:
        const char *what() {
            return "invalid piece id";
        }
    };

    class invalid_cred_exception : public std::exception {
        public:
        const char *what() {
            return "invalid credentials";
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
            std::cout<<"count: "<<count<<"\n";
            fs->write(t, count);
            flush();//imp
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

        tsafe_fstream flush() {
            std::lock_guard<std::recursive_mutex> lock(*m);
            fs->flush();
            return *this;
        }

        bool fail() const {
            std::lock_guard<std::recursive_mutex> lock(*m);
            return fs->fail();
        }

    };

    class bitfield {
        bstring container;
        int _size = 0;

        public:
        class index_out_of_range_exception: public std::exception {
            std::string msg;
            const char *what() {
                return "index out of range";
            }
        };

        bitfield(int n) {
            this->_size = n;
            container.resize(((n-1)/8) + 1);
        }

        bitfield(bstring bs): container(bs) {
            _size = container.size()*8;
        }

        bitfield() {}

        bool operator[](int i) {
            if(i>=_size)
                throw index_out_of_range_exception();
            int j = i/8;
            int k = i%8;
            return container[j] & (1<<k);
        }

        void set(int i) {
            if(i>=_size)
                throw index_out_of_range_exception();
            
            int j = i/8;
            int k = i%8;
            container[j] |= (1<<k);
        }

        void reset(int i) {
            if(i>=_size)
                throw index_out_of_range_exception();
            
            int j = i/8;
            int k = i%8;
            container[j] &= (~(1<<k));
        }

        bstring to_bstring() {
            return container;
        }

        size_t size() {
            return _size;
        }
    };

    /* each thread will get its own downloader */
    class downloader {
        /* id of current piece being downloaded */
        tsafe_fstream tsfs;
        const std::vector<bstring> shs; //check if it is needed here or not
        // int piece_id;
        http::http_client client;
        net_socket::sock_addr server_addr;
        std::function<void(long long, bitfield)> update_callback;

        public:
        /* downloader assumes that abs_fname has already been allocated required space */
        downloader(net_socket::ipv4_addr addr, uint16_t port, tsafe_fstream& tsfs, 
                    const std::vector<bstring> &shs, std::function<void(long long, bitfield)> callback): 
                    tsfs(tsfs), shs(shs), update_callback(callback) {
            try {
                server_addr = net_socket::sock_addr(addr, port);
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
            // for(int i=0; i<data.size(); ++i)
            //     std::cout<<(int)data[i]<<" ";
            std::cout<<"\n";

            long long pos = piece_id * PIECE_LENGTH;
            std::cout<<"pos "<<pos<<"\n";
            tsfs.write((char *)data.c_str(), pos, data.size());
            std::cout<<"\n---------------------written-----------------------\n";
        }

        bool is_valid(int piece_id, bstring &&piece) {
            /* verify sha here */
            unsigned char md[SHA_DIGEST_LENGTH];
            SHA1(piece.c_str(), piece.size(), md);

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

            /* get the bitfield - may throw header not found exception */
            bitfield bf = bitfield(s2b(res.get_header("Bit-Field")));

            if(!is_valid(piece_id, res.get_body()))
                throw sha_mismatch_exception();
            
            /* write piece to disk */
            write_piece(piece_id, res.get_body());

            /* update the local data structures */
            
            update_callback(server_addr.uid, bf);
        }
    };

    /* there will be a single upload server (threading is handled there already) */
    class upload_server {
        http::http_server server;
        net_socket::sock_addr server_addr;
        std::string abs_fname;
        tsafe_fstream tsfs;
        char buf[PIECE_LENGTH];
        std::function<bitfield()> get_bitfield; 

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

        // class callback {
        //     upload_server &userver;

        //     public:
        //     callback(upload_server &userver): userver(userver) {}

        //     http::response operator()(http::request req, net_socket::sock_addr sock) {
            http::response callback(http::request req, net_socket::sock_addr sock) {
                std::cout<<"callback called\n";
                int piece_id;
                http::response res;
                try {
                    piece_id = stoi(req.get_header("Piece-Id"));
                    std::cout<<"piece_id"<<piece_id<<"\n";
                    /* read piece from file */
                    // bstring piece = userver.read_piece(piece_id);
                    bstring piece = read_piece(piece_id);

                    std::cout<<"piece read\n"<<"\n";
                    /* construct response */
                    res.set_status(http::status::OK);
                    res.set_status_text("successful");
                    res.set_version(http::version::HTTP_2_0);
                    res.add_header("Content-Length", std::to_string(piece.size()));
                    // res.add_header("Bit-Field", b2s(userver.get_bitfield(sock.uid).to_bstring()));
                    res.add_header("Bit-Field", b2s(get_bitfield().to_bstring()));
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
        // };

        public:
        upload_server(net_socket::ipv4_addr ip, uint16_t port, tsafe_fstream& tsfs, 
            std::function<bitfield()> f): tsfs(tsfs), get_bitfield(f) {
            server_addr = net_socket::sock_addr(ip, port);
        }

        upload_server() {}

        void start() {
            try {
                server = http::http_server(server_addr.ip, server_addr.port);
                // server.accept_clients<callback>(callback(*this));
                server.accept_clients(std::bind(&upload_server::callback, this,
                    std::placeholders::_1, std::placeholders::_2));
            } catch(std::exception e) {
                std::cout<<"exception caught\n";
                std::cout<<e.what()<<"\n";
            }
        }

        net_socket::sock_addr get_addr() {
            return server_addr;
        }
    };

    template<typename T, typename Compare>
    class tsafe_set {
        // std::priority_queue<T, Container, Compare> pq;
        std::set<T, Compare> piece_set;
        std::recursive_mutex m;
        std::vector<int> *pieces_priority;

        public:
        tsafe_set(const Compare &comp, std::vector<int> *pieces_priority) {
            piece_set = std::set<T, Compare>(comp);
            this->pieces_priority = pieces_priority;
        }
        tsafe_set() {}

        void operator=(const tsafe_set &tss) {
            this->piece_set = tss.piece_set;
            // this->m = tss.m;
            this->pieces_priority = tss.pieces_priority;
        }
        int top() {
            std::lock_guard<std::recursive_mutex> lock(m);
            return *piece_set.begin();
        }

        void pop() {
            std::lock_guard<std::recursive_mutex> lock(m);
            piece_set.erase(piece_set.begin());
        }

        void push(T t) {
            std::lock_guard<std::recursive_mutex> lock(m);
            piece_set.insert(t);
        }

        bool empty() {
            std::lock_guard<std::recursive_mutex> lock(m);
            return piece_set.empty();
        }

        T remove_top() {
            std::lock_guard<std::recursive_mutex> lock(m);
            T t = top();
            pop();
            return t;
        }

        void update_priority(int piece_id, int priority) {
            std::lock_guard<std::recursive_mutex> lock(m);
            if(piece_set.find(piece_id) == piece_set.end())
                return;
            piece_set.erase(piece_id);
            (*pieces_priority)[piece_id] = priority;
            piece_set.insert(piece_id);
        }

    };


    struct neighbour_data {
        net_socket::sock_addr addr;
        bitfield isAvailable;
    };

    class tracker_client {
        net_socket::sock_addr tracker_addr;
        std::string auth_token;
        http::http_client client;
        
        public:
        tracker_client(net_socket::sock_addr tracker_addr): tracker_addr(tracker_addr) {}
        tracker_client() {}

        void signup(std::string uname, std::string passwd) {
            client.connect(tracker_addr);
            http::request req(http::method::POST, "/signup");
            req.add_header("Content-Length", "0");
            req.add_header("Username", uname);
            req.add_header("Password", passwd);

            
            auto res = client.send_request(req);
            if(res.get_status() == http::status::UNAUTHORIZED)
                throw invalid_cred_exception();
            else if(res.get_status() != http::status::OK)
                throw http::http_exception("failed to sign up: "+res.get_status_text());         
            client.disconnect();   
        }

        void login(std::string uname, std::string passwd) {
            client.connect(tracker_addr);
            http::request req(http::method::POST, "/login");
            req.add_header("Content-Length", "0");
            req.add_header("Username", uname);
            req.add_header("Password", passwd);

            
            auto res = client.send_request(req);
            if(res.get_status() == http::status::OK)
                auth_token = res.get_header("Authorization");
            else if(res.get_status() == http::status::UNAUTHORIZED)
                throw invalid_cred_exception();
            else
                throw http::http_exception("failed to log in: "+res.get_status_text());
            
            client.disconnect();
        }

        void create_group(std::string gid) {
            client.connect(tracker_addr);
            http::request req(http::method::POST, "/create_group");
            req.add_header("Content-Length", "0");
            req.add_header("Authorization", auth_token);
            req.add_header("Group-Id", gid);

            auto res = client.send_request(req);
            if(res.get_status() == http::status::UNAUTHORIZED)
                throw invalid_cred_exception();
            else if(res.get_status() != http::status::OK)
                throw http::http_exception("failed to create group: "+res.get_status_text());
            client.disconnect();
        }

        void join_group(std::string gid) {
            client.connect(tracker_addr);
            http::request req(http::method::POST, "/join_group");
            req.add_header("Content-Length", "0");
            req.add_header("Authorization", auth_token);

            auto res = client.send_request(req);
            if(res.get_status() == http::status::UNAUTHORIZED)
                throw invalid_cred_exception();
            else if(res.get_status() != http::status::OK)
                throw http::http_exception("failed to create group: "+res.get_status_text());
            client.disconnect();
        }

        void leave_group(std::string gid) {
            client.connect(tracker_addr);
            http::request req(http::method::POST, "/leave_group");
            req.add_header("Content-Length", "0");
            req.add_header("Authorization", auth_token);

            auto res = client.send_request(req);
            if(res.get_status() == http::status::UNAUTHORIZED)
                throw invalid_cred_exception();
            else if(res.get_status() != http::status::OK)
                throw http::http_exception("failed to create group: "+res.get_status_text());
            client.disconnect();
        }

        void logout() {
            client.connect(tracker_addr);
            http::request req(http::method::POST, "/logout");
            req.add_header("Content-Length", "0");
            req.add_header("Authorization", auth_token);

            
            auto res = client.send_request(req);
            client.disconnect();
            if(res.get_status() == http::status::UNAUTHORIZED)
                throw invalid_cred_exception();
            else if(res.get_status() != http::status::OK)
                throw http::http_exception("failed to sign up: "+res.get_status_text());
            
        }

        void share_file(std::string fname, std::string gid, net_socket::sock_addr us_addr) {
            client.connect(tracker_addr);
            http::request req(http::method::POST, "/share_file");
            req.add_header("Content-Length", "0");
            req.add_header("Authorization", auth_token);
            req.add_header("File-Name", fname);
            req.add_header("Group-Id", gid);
            req.add_header("Server-Address", std::to_string(us_addr.get_uid()));

            auto res = client.send_request(req);
            if(res.get_status() == http::status::UNAUTHORIZED)
                throw invalid_cred_exception();
            else if(res.get_status() != http::status::OK)
                throw http::http_exception("failed to sign up: "+res.get_status_text());
            client.disconnect();
        }

        std::vector<net_socket::sock_addr> get_peers(std::string fname, std::string gid, net_socket::sock_addr us_addr) {
            client.connect(tracker_addr);
            http::request req(http::method::GET, "/peers");
            req.add_header("Content-Length", "0");
            req.add_header("Authorization", auth_token);
            req.add_header("Group-Id", gid);
            req.add_header("File-Name", fname);
            req.add_header("Server-Address", std::to_string(us_addr.get_uid()));

            std::vector<net_socket::sock_addr> addrs;
            auto res = client.send_request(req);
            if(res.get_status() == http::status::OK) {
                /* parse the body here */
                std::string body = b2s(res.get_body());
                body = body.substr(1, body.size()-2);
                auto segs = util::tokenize(body, ",");
                for(auto seg : segs) {
                    seg = seg.substr(1, seg.size()-2);
                    auto p = util::tokenize(seg, ":");
                    addrs.push_back(net_socket::sock_addr(stoll(p[1])));
                }

                return addrs;
            } else if(res.get_status() == http::status::UNAUTHORIZED) {
                throw invalid_cred_exception();
            } else {
                throw http::http_exception("failed to fetch peers: "+res.get_status_text());
            }
            client.disconnect();
            return addrs;
        }
    };

    class peer {
        tsafe_set<int, std::function<bool(int, int)>> pieces;
        std::recursive_mutex m;
        std::vector<bstring> shs;

        bool seeder;

        std::string fname;

        std::string work_dir;

        std::string gid;

        /* total number of pieces */
        size_t n_pieces;

        /* bitfiend of peer */
        bitfield isAvailable;

        /* priority of pieces */
        std::vector<int> pieces_priority;

        /* holds data like bitfield of the neighbour */
        std::unordered_map<long long, neighbour_data> ndata;

        /* holds all the downloader thread instances */
        std::vector<std::thread> dwnld_threads;

        /* instance of upload server thread */
        std::thread upload_server_thread;

        /* client to handle tracker communication */
        tracker_client tc;

        /* upload server instance */
        upload_server us;

        /* thread safe stream for the file to be downloaded */
        tsafe_fstream tsfs;

        /* fstream of file to be downloaded (do not use it directly) */
        std::fstream *fs_ptr;

        /* mutex pointer to sync access to download file(do not use it directly) */
        std::recursive_mutex *m_ptr;

        /* Thread safe function to print msg */
        void print(std::string s) {
            std::lock_guard<std::recursive_mutex> lock(m);
            std::cout<<s;
        }

        /* Callback routines for updating neighbour data */
        void update_ndata_callback(long long peer_id, bitfield bf) {
            std::cout<<"updating ndata\n";
            std::cout<<bf.size()<<"\n";
            bitfield oldbf = ndata[peer_id].isAvailable;
            std::cout<<oldbf.size()<<"\n";
            for(int i=0; i<n_pieces; ++i) {
                int new_priority = pieces_priority[i] + bf[i] - oldbf[i];
                pieces.update_priority(i, new_priority);
            }
        }

        bool comp(int p1, int p2) {
            if(pieces_priority[p1] != pieces_priority[p2])            
                return pieces_priority[p1] < pieces_priority[p2];
            else
                return p1 < p2;
        }

        /* function to get bitfield information */
        bitfield get_bitfield() {
            return isAvailable;
        }

        /* routine to start the server */
        static void upload_server_routine(peer &p) {
            p.us.start();
        }

        /* routine to start the downloader */
        static void downloader_routine(peer &p, long long peer_id) {
            std::cout<<"downloading\n";
            p.print("download thread started : "+std::to_string(peer_id)+"\n");
            p.print(std::to_string(net_socket::sock_addr(peer_id).port));
            std::string tid = std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));
            /* create a downloader */
            try {
                downloader d(p.ndata[peer_id].addr.ip, p.ndata[peer_id].addr.port, p.tsfs, p.shs,
                    std::bind(&peer::peer::update_ndata_callback, &p, std::placeholders::_1, std::placeholders::_2));
                p.print("create downloader\n");
                while(!p.pieces.empty()) {
                    int piece_id = p.pieces.remove_top();
                    try {
                        p.print(tid+" trying to download piece "+std::to_string(piece_id)+"\n");
                        d.download(piece_id);
                        p.print(tid+" downloaded piece " + std::to_string(piece_id) + "\n");
                        sleep(1);
                    } catch(const std::exception &e) {
                        p.print(tid +" Unable to connect to peer. "+std::to_string(piece_id)+"\n");
                        std::cerr<<e.what()<<"\n";
                        // p.print("retrying\n");
                        p.pieces.push(piece_id);//imp
                        // sleep(1);
                        break;
                    }
                }
            } catch(std::exception e) {
                p.print(" exiting with failure\n");
                std::cout<<e.what()<<"\n";
            }
            p.print("download thread "+tid+" exiting");
        }

        void start_upload_server() {
            upload_server_thread = std::thread(upload_server_routine,
                std::ref(*this));
        }

        void start_download() {
            /* start download threads */
            for(auto &p : ndata) {
                dwnld_threads.push_back(std::thread(downloader_routine, 
                    std::ref(*this), p.second.addr.get_uid()));
            }
        }

        void fetch_peers(std::string fname, std::string gid) {
            /*TODO: fetch peers from tracker */
            auto addrs = tc.get_peers(fname, gid, us.get_addr());
            for(auto addr : addrs) {
                ndata[addr.get_uid()].addr = addr;
                ndata[addr.get_uid()].isAvailable = bitfield(n_pieces);
            }
        }

        void load_meta() {
            /*load shs from the torrent file */
            std::string torrent_file = "../.test/"+work_dir+"/"+fname+".torrent"; 
            std::ifstream in(torrent_file);
            unsigned char md[SHA_DIGEST_LENGTH];
            while(!in.eof()) {
                in.read((char*)md, SHA_DIGEST_LENGTH);
                if(in.gcount() > 0)
                    shs.push_back(md);
            }
            in.close();
            n_pieces = shs.size();
            pieces_priority.resize(n_pieces);
            
            /*TODO: update pieces priority queue from the meta file*/
            for(int i=0; i<n_pieces; ++i)
                pieces.push(i);

            isAvailable = bitfield(n_pieces);
            if(seeder) {
                for(int i=0; i<n_pieces; ++i)
                    isAvailable.set(i);
            }
        }

        public:
        peer(std::string work_dir, std::string fname, std::string gid, std::string ip, int port, 
            net_socket::sock_addr tracker_addr, bool seeder = false) {

            
            this->fname = fname;
            this->work_dir = work_dir;
            this->gid = gid;
            
            /* create download file if it does not exists */
            std::ifstream in("../.test/"+work_dir+"/"+fname);
            if(!in.is_open())
                std::ofstream("../.test/"+work_dir+"/"+fname);
            in.close();

            fs_ptr = new std::fstream("../.test/"+work_dir+"/"+fname, std::ios_base::in | std::ios_base::out | std::ios_base::binary);
            m_ptr = new std::recursive_mutex();
            tsfs = tsafe_fstream(fs_ptr, m_ptr);
            tc = tracker_client(tracker_addr);
            pieces = tsafe_set<int, std::function<bool(int, int)>>(std::bind(&peer::peer::comp, this,
                std::placeholders::_1, std::placeholders::_2), &pieces_priority);
            this->seeder = seeder;

            /* load shs */
            load_meta();

            /* create upload server instance */
            us = upload_server(net_socket::ipv4_addr(ip), (uint16_t)port, tsfs, 
                    std::bind(&peer::peer::get_bitfield, this));
        }

        ~peer() {
            delete fs_ptr;
            delete m_ptr;
        }



        void start() {
            try {
                start_upload_server();

                /* retrieve peer addresses or share file */
                std::string username;
                std::string password;
                std::cout<<"Username: ";
                std::cin>>username;
                std::cout<<"Passwrod: ";
                std::cin>>password;
                tc.login(username, password);

                if(seeder) {
                    tc.share_file(fname, gid, us.get_addr());
                } else {
                    fetch_peers(fname, gid);
                    start_download();
                    for(int i=0; i<dwnld_threads.size(); ++i)
                        dwnld_threads[i].join();
                }

                upload_server_thread.join();
            } catch(std::exception &e) {
                std::cout<<e.what()<<"\n";
            }
        }
    };

};