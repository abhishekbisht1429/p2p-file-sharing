#include<iostream>
#include<thread>
#include<vector>
#include<set>
#include<unordered_map>
#include"sqlite/sqlite_client.cpp"
#include "http.cpp"
// #include "socket.cpp"

namespace tracker {

    class invalid_gid_exception : public std::exception {
        public:
        const char *what() {
            return "invalid group id";
        }
    };

    class invalid_uid_exception : public std::exception {
        public:
        const char *what() {
            return "invalid user id";
        }
    }; 

    class invalid_cred_exception : public std::exception {
        public:
        const char *what() {
            return "invalid credentials";
        }
    };       

    class fname_conflict_exception : public std::exception {
        public:
        const char *what() {
            return "file name conflict";
        }
    };

    class no_such_file_exception : public std::exception {
        public:
        const char *what() {
            return "no such file";
        }
    };

    struct peer {
        std::string id;
        std::string passwd;
    };

    struct file {
        /* socket id of seeders */
        std::set<std::string> seeders;

        /* socket id of leechers */
        std::set<std::string> leechers;

        /* file name */
        std::string fname;

        /* group that the file belongs to */
        std::string group_id;
    };

    struct group {
        std::string id;
        std::string owner_id;
        std::set<std::string> members;
        std::map<std::string, file> files;
    };

    class tracker {
        /* sqlite clients */
        sqlite::group_client sqlite_gc;
        sqlite::user_client sqlite_uc;
        sqlite::token_client sqlite_tc;
        
        /* groups */
        std::map<std::string, group> groups;

        /* token map for authorization */
        std::unordered_map<std::string, std::string> token_map;

        /* address of currently connected users */
        std::map<std::string, net_socket::sock_addr> address;

        /* tracker server address */
        net_socket::sock_addr server_addr;

        /* http server for tracker */
        http::http_server server;

        std::string get_peers(std::string uid, std::string gid, std::string fname) {
            /* for the time being it is assumed that on tracker start
                the groups info is loaded in main memory.
                This can be changed later and instead the data maybe loadead here in this function 
                from the datbase
            */
            if(groups.find(gid) == groups.end())
                throw invalid_gid_exception();
            if(groups[gid].members.find(uid) == groups[gid].members.end())
                throw invalid_gid_exception();
            if(groups[gid].files.find(fname) == groups[gid].files.end())
                throw no_such_file_exception();
            
            group &g = groups[gid];
            file &f = g.files[fname];
            /* add uid to the leacher set */ 
            f.leechers.insert(uid);

            /* extract list of seeders and leechers */
            std::vector<std::string> member_ids;
            for(auto leecher : f.leechers)
                member_ids.push_back(leecher);
            for(auto seeder : f.seeders)
                member_ids.push_back(seeder);

            std::string members;
            // std::string members = "[{"+g.owner_id+","+std::to_string(address[g.owner_id].get_uid())+"}";
            for(auto id : member_ids) {
                if(id != uid)
                    members += ",{"+id+":"+std::to_string(address[id].get_uid())+"}";
            }
            members = members.substr(1);
            members = "["+members+"]";

            return members;
        }

        std::string tokent_to_userid(std::string token) {
            if(token_map.find(token) == token_map.end())
                return "";
            return token_map[token];
        }

        bool verified(std::string auth_token, net_socket::sock_addr addr) {
            if(token_map.find(auth_token) == token_map.end())
                return false;
            // address[tokent_to_userid(auth_token)] = addr;
            return true;
        }

        void create_user(std::string uid, std::string passwd) {
            sqlite_uc.create_user(uid, passwd);
        }

        void create_group(std::string gid, std::string oid) {
            sqlite_gc.insert_group(gid, oid);
            sqlite_gc.add_member(oid, gid);
            group g;
            g.id = gid;
            g.owner_id = oid;
            g.members.insert(oid);
            groups[gid] = g;
        }

        void delete_group(std::string gid) {
            sqlite_gc.remove_all_members(gid);
            sqlite_gc.remove_group(gid);
            groups.erase(gid);
        }

        std::string generate_token(std::string uid, std::string passwd) {
            /* for the time being passwd is the token */
            return uid+passwd;
        }

        void join_group(std::string uid, std::string gid) {
            sqlite_gc.add_member(uid, gid);
            groups[gid].members.insert(uid);
        }

        void leave_group(std::string uid, std::string gid) {
            sqlite_gc.remove_member(uid, gid);
            groups[gid].members.erase(uid);
            /* if owner leaves delete the groups as well */
            if(uid == groups[gid].owner_id)
                delete_group(gid);
        }

        std::string login(std::string uid, std::string passwd) {
            auto res = sqlite_uc.get_user(uid);
            if(res.size() == 0 || res[1] != passwd)
                throw invalid_cred_exception();
            
            /* check if token already exists */
            std::string token;
            try {
                token = sqlite_tc.get_token(uid);
            } catch(sqlite::sqlite_exception se) {
                token = generate_token(uid, passwd);
                sqlite_tc.insert_token(token, uid);
            }
            token_map[token] = uid;
            return token;
        }

        void logout(std::string token) {
            sqlite_tc.delete_token(token);
            address.erase(tokent_to_userid(token));
            token_map.erase(token);
        }

        void share_file(std::string uid, std::string gid, std::string fname) {
            if(groups.find(gid) == groups.end())
                throw invalid_gid_exception();
            if(groups[gid].members.find(uid) == groups[gid].members.end())
                throw invalid_gid_exception();
            if(groups[gid].files.find(fname) != groups[gid].files.end())
                throw fname_conflict_exception();
            file f;
            f.group_id = gid;
            f.fname = fname;
            f.seeders.insert(uid);
            groups[gid].files[fname] = f;
        }

        http::response post(http::request req, net_socket::sock_addr remote_addr) {
            http::response res;
            if(req.get_resource() == "/signup") {
                try {
                    std::string uid = req.get_header("Username");
                    std::string pass = req.get_header("Password");

                    create_user(uid, pass);
                    return http::response(http::status::OK, "successful");
                } catch(http::no_such_header_exception nshe) {
                    return http::response(http::status::BAD_REQUEST, "user id missing");
                } catch(sqlite::constraint_failed_exception) {
                    return http::response(http::status::CONFLICT, "username no available");
                } catch(sqlite::sqlite_exception se) {
                    std::cout<<se.what()<<"\n";
                    return http::response(http::status::INTERNAL_SERVER_ERROR, "server error");
                }
            } else if(req.get_resource() == "/login") {
                try {
                    std::string uid = req.get_header("Username");
                    std::string passwd = req.get_header("Password");
                    
                    std::string auth = login(uid, passwd);
                    address[uid] = remote_addr;

                    res = http::response(http::status::OK, "successful");
                    res.add_header("Authorization", auth);
                    return res;
                } catch(http::no_such_header_exception nshe) {
                    return http::response(http::status::BAD_REQUEST, "user id missing");
                } catch(invalid_cred_exception ice) {
                    return http::response(http::status::UNAUTHORIZED, "invalid cred");
                } catch(sqlite::sqlite_exception se) {
                    return http::response(http::status::INTERNAL_SERVER_ERROR, "server error");
                }
            } else if(req.get_resource() == "/logout") {
                try {
                    std::string auth_token = req.get_header("Authorization");
                    if(!verified(auth_token, remote_addr))
                        return http::response(http::status::UNAUTHORIZED, "invalid token");
                    
                    logout(auth_token);
                    return http::response(http::status::OK, "successful");
                } catch(http::no_such_header_exception nshe) {
                    return http::response(http::status::BAD_REQUEST, "token missing");
                }
            } else if(req.get_resource() == "/create_group") {
                try {
                    std::string auth_token = req.get_header("Authorization");
                    if(!verified(auth_token, remote_addr))
                        return http::response(http::status::UNAUTHORIZED, "invalid token");
                    
                    std::string gid = req.get_header("Group-Id");
                    create_group(gid, tokent_to_userid(auth_token));
                    return http::response(http::status::OK, "successful");
                } catch(sqlite::constraint_failed_exception) {
                    return http::response(http::status::CONFLICT, "group id taken");
                } catch(http::no_such_header_exception nshe) {
                    return http::response(http::status::BAD_REQUEST, "missing group id");
                }
            } else if(req.get_resource() == "/join_group") {
                try {
                    std::string auth_token = req.get_header("Authorization");
                    if(!verified(auth_token, remote_addr))
                            return http::response(http::status::UNAUTHORIZED, "invalid token");

                    std::string gid = req.get_header("Group-Id");
                    join_group(tokent_to_userid(auth_token), gid);
                    return http::response(http::status::OK, "successful");
                } catch(http::no_such_header_exception nshe) {
                    return http::response(http::status::BAD_REQUEST, "token missing");
                } catch(sqlite::constraint_failed_exception) {
                    return http::response(http::status::CONFLICT, "cannot join group twice");
                } catch(sqlite::sqlite_exception se) {
                    return http::response(http::status::INTERNAL_SERVER_ERROR, "server error");
                }
            } else if(req.get_resource() == "/leave_group") {
                try {
                    std::string auth_token = req.get_header("Authorization");
                    if(!verified(auth_token, remote_addr))
                            return http::response(http::status::UNAUTHORIZED, "invalid token");
                    std::string gid = req.get_header("Group-Id");
                    leave_group(tokent_to_userid(auth_token), gid);
                    return http::response(http::status::OK, "successful");
                } catch(http::no_such_header_exception nshe) {
                    return http::response(http::status::BAD_REQUEST, "token missing");
                } catch(sqlite::sqlite_exception se) {
                    return http::response(http::status::INTERNAL_SERVER_ERROR, "server error");
                }
            } else if(req.get_resource() == "/share_file") {
                try {
                    std::string auth_token = req.get_header("Authorization");
                    if(!verified(auth_token, remote_addr))
                            return http::response(http::status::UNAUTHORIZED, "invalid token");
                    std::string uid = tokent_to_userid(auth_token);
                    std::string gid = req.get_header("Group-Id");
                    std::string fname = req.get_header("File-Name");
                    std::string server_addr = req.get_header("Server-Address");

                    share_file(uid, gid, fname);
                    address[tokent_to_userid(auth_token)] = net_socket::sock_addr(stoll(server_addr));
                    return http::response(http::status::OK, "file name entry added");
                } catch(http::no_such_header_exception nshe) {
                    return http::response(http::status::BAD_REQUEST, "header missing");
                } catch(invalid_gid_exception ige) {
                    return http::response(http::status::BAD_REQUEST, "invalid gid");
                } catch(fname_conflict_exception fce) {
                    return http::response(http::status::BAD_REQUEST, "file already shared");
                }
            } else {
                return http::response(http::status::NOT_FOUND, "resource not found");
            }
        }

        http::response get(http::request req, net_socket::sock_addr remote_addr) {
            http::response res;
            if(req.get_resource() == "/peers") {
                try {
                    std::string auth_token = req.get_header("Authorization");

                    if(!verified(auth_token, remote_addr))
                        return http::response(http::status::UNAUTHORIZED, "invalid token");
                    
                    std::string uid = tokent_to_userid(auth_token);
                    std::string gid = req.get_header("Group-Id");
                    std::string fname = req.get_header("File-Name");
                    std::string server_addr = req.get_header("Server-Address");
                    address[tokent_to_userid(auth_token)] = net_socket::sock_addr(stoll(server_addr));
                    std::string peers;
                    try {
                        peers = get_peers(uid, gid, fname);
                    } catch(invalid_gid_exception ige) {
                        std::cout<<ige.what()<<"\n";
                        return http::response(http::status::FORBIDDEN, "user not part of group or no such group");
                    }

                    res = http::response(http::status::OK, "successful");
                    res.add_header("Content-Length", std::to_string(peers.size()));
                    res.set_body(s2b(peers));

                    return res;
                } catch(http::no_such_header_exception nshe) {
                    return http::response(http::status::BAD_REQUEST, "header missing");
                } catch(invalid_uid_exception iue) {
                    return http::response(http::status::BAD_REQUEST, "invalid group id");
                } catch(no_such_file_exception nsfe) {
                    return http::response(http::status::BAD_REQUEST, "invalid file name");
                }
            } else {
                return http::response(http::status::NOT_FOUND, "resource not found");
            }
        }
        
        http::response callback(http::request req, net_socket::sock_addr remote_addr) {
            http::response res;
            try {
                if(req.get_method() == http::method::GET) {
                    return get(req, remote_addr);
                } else if(req.get_method() == http::method::POST) {
                    return post(req, remote_addr);
                } else {
                    return http::response(http::status::METHOD_NOT_ALLOWED, "method not allowed");
                }
            } catch(std::exception &e) {
                std::cout<<"tracker server: "<<e.what()<<"\n";
                return http::response(http::status::INTERNAL_SERVER_ERROR, "server error");
            }
        }

        public:
        /*
            When a tracker is intialized it reads the group info from 
            a local database 
        */
        tracker(net_socket::ipv4_addr ip, uint16_t port) {
            /* TODO: read group info from database */
            server_addr = net_socket::sock_addr(ip, port);
            try {
                sqlite_gc.create_group_table();
                sqlite_uc.create_user_table();
                sqlite_tc.create_token_table();
                sqlite_gc.create_members_table();
                auto group_table = sqlite_gc.get_group_list();
                auto token_table = sqlite_tc.get_token_list();
                /* intialize token map */
                for(auto &row : token_table) {
                    token_map[row[0]] = row[1];
                }

                /* intialize groups */
                for(auto &row : group_table) {
                    group g;
                    g.id = row[0];
                    g.owner_id = row[1];
                    auto members = sqlite_gc.get_member_list(g.id);
                    for(auto &mem : members)
                        g.members.insert(mem[0]);
                    groups[g.id] = g;
                }
                server = http::http_server(ip, port);
            } catch(std::exception e) {
                std::cout<<e.what()<<"\n";
            }
        }

        // tracker() {}



        void start_server() {
            try {
                server.accept_clients(std::bind(&tracker::callback, this, 
                    std::placeholders::_1, std::placeholders::_2));
            } catch(std::exception &e) {
                std::cout<<e.what()<<"\n";
            }
        }
    };
};
