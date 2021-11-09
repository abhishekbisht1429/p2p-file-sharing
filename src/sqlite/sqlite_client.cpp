#include"sqlite3.h"
#include<string>
#include<vector>
#include<iostream>
#include<thread>

namespace sqlite {
    class sqlite_exception : std::exception {
        std::string err_msg;
        std::string rc_msg;
        public:
        sqlite_exception(std::string rc_msg, std::string err_msg): 
                            rc_msg(rc_msg), err_msg(err_msg) {}
        const char *what() {
            return (rc_msg + ";" + err_msg).c_str();
        }
    };

    class constraint_failed_exception : sqlite_exception {
        public:
        constraint_failed_exception(): sqlite_exception("unique constraint failed", "constraint failed") {}
    };

    class sqlite_client {
        protected:
        char *err_msg = nullptr;
        sqlite3 *db;

        sqlite_client() {
            int rc = sqlite3_open("../.test/tracker.db", &db);
            if(rc!=0) {
                sqlite_exception e =  sqlite_exception(sqlite3_errstr(rc), "failed to open db");
                throw e;
            }
        }

        ~sqlite_client() {
            sqlite3_close(db);
        }

        void exec(std::string query) {
            int rc = sqlite3_exec(db, query.c_str(), nullptr, 0, &err_msg);

            if(rc!=0) {
                if(rc == SQLITE_CONSTRAINT) {
                    throw constraint_failed_exception();
                } else {
                    sqlite_exception e =  sqlite_exception(sqlite3_errstr(rc), err_msg);
                    sqlite3_free(err_msg);
                    throw e;
                }
            }
            sqlite3_free(err_msg);
        }

        void exec(std::string query, int (*callback)(void*, int, char**, char**), void *arg_ptr) {
            int rc = sqlite3_exec(db, query.c_str(), callback, arg_ptr, &err_msg);

            if(rc!=0) {
                std::cout<<"err_msg: "<<err_msg<<"\n";
                sqlite_exception e =  sqlite_exception(sqlite3_errstr(rc), err_msg);
                sqlite3_free(err_msg);
                throw e;
            }
            sqlite3_free(err_msg);
        }
    };

    class group_client : protected sqlite_client {

        public:
        void create_group_table() {
            std::string query = "CREATE TABLE IF NOT EXISTS 'GROUP'("\
                            "ID CHAR(50) PRIMARY KEY NOT NULL,"\
                            "OWNER_ID CHAR(50) NOT NULL);";
            exec(query);
        }

        void create_members_table() {
            std::string query = "CREATE TABLE IF NOT EXISTS 'MEMBER'("\
                "ID CHAR(50) NOT NULL,"\
                "GID CHAR(50) NOT NULL,"\
                "PRIMARY KEY(ID, GID));";
            exec(query);
        }

        void delete_group_table() {
            std::string query = "DROP TABLE IF EXISTS 'GROUP';";
            exec(query);
        }

        void delete_members_table() {
            std::string query = "DROP TABLE IF EXISTS 'MEMBER';";
            exec(query); 
        }

        void insert_group(std::string gid, std::string oid) {
            std::string query = "INSERT INTO 'GROUP' VALUES('"+
                                gid+"', '"+oid+"');";
            exec(query);
        }

        void remove_group(std::string gid) {
            std::string query = "DELETE FROM 'GROUP' WHERE "\
                                "ID = '"+gid+"';";
            exec(query);
        }

        static int grp_list_callback(void *vec, int ncols, char **col_vals, char **col_names) {
            auto &res = *((std::vector<std::vector<std::string>>*)vec);

            std::vector<std::string> cols_vec;
            for(int i=0; i<ncols; ++i)
                cols_vec.push_back(col_vals[i]);
            
            res.push_back(cols_vec);
            return 0;
        }

        std::vector<std::vector<std::string>> get_group_list() {
            std::string query = "SELECT * FROM 'GROUP';";
            std::vector<std::vector<std::string>> res;
            exec(query, grp_list_callback, &res);

            return res;
        }

        void add_member(std::string uid, std::string gid) {
            std::string query = "INSERT INTO 'MEMBER' VALUES('"+
                                uid+"', '"+gid+"');";
            exec(query);            
        }

        void remove_member(std::string uid, std::string gid) {
            std::string query = "DELETE FROM 'MEMBER' WHERE "\
                "ID = '"+uid+"' AND GID = '"+gid+"';";
            exec(query);
        }

        void remove_all_members(std::string gid) {
            std::string query = "DELETE FROM 'MEMBER' WHERE "\
                "GID = '"+gid+"';";
            exec(query);
        }

        std::vector<std::vector<std::string>> get_member_list(std::string gid) {
            std::string query = "SELECT * FROM 'MEMBER' WHERE "\
                                "GID = '"+gid+"';";
            std::vector<std::vector<std::string>> res;
            exec(query, grp_list_callback, &res);

            return res;
        }
    };

    class user_client : sqlite_client {


        public:
        void create_user_table() {
            std::string query = "CREATE TABLE IF NOT EXISTS 'USER'("\
                            "ID CHAR(50) PRIMARY KEY NOT NULL,"\
                            "PASSWORD CHAR(50) NOT NULL);";
            exec(query);
        }

        void delete_group_table() {
            std::string query = "DROP TABLE IF EXISTS 'USER';";
            exec(query);
        }

        void create_user(std::string uid, std::string passwd) {
            std::string query = "INSERT INTO 'USER' VALUES('"+
                                uid+"', '"+passwd+"');";
            exec(query);
        }

        void delete_user(std::string uid) {
            std::string query = "DELETE FROM 'USER' WHERE "\
                                "ID = '"+uid+"';";
            exec(query);
        }

        static int callback(void *vec, int ncols, char **col_vals, char **col_names) {
            auto &cols_vec = *((std::vector<std::string>*)vec);

            for(int i=0; i<ncols; ++i)
                cols_vec.push_back(col_vals[i]);
            
            return 0;
        }

        std::vector<std::string> get_user(std::string uid) {
            std::string query = "SELECT * FROM 'USER' WHERE ID='"+uid+"';";
            std::vector<std::string> res;
            exec(query, callback, &res);
            return res;
        }
    };

    class token_client : sqlite_client {
        public:
        void create_token_table() {
            std::string query = "CREATE TABLE IF NOT EXISTS 'TOKEN'("\
                            "TOKEN CHAR(50) PRIMARY KEY NOT NULL,"\
                            "UID CHAR(50) UNIQUE NOT NULL);";
            exec(query);
        }

        void insert_token(std::string token, std::string uid) {
            std::string query = "INSERT INTO 'TOKEN' VALUES('"+
                    token+"', '"+uid+"');";
            exec(query);
        }

        void delete_token(std::string token) {
            std::string query = "DELETE FROM 'TOKEN' WHERE "\
                                "ID = '"+token+"';";   
        }

        static int callback(void *vec, int ncols, char **col_vals, char **col_names) {
            auto &res = *((std::vector<std::vector<std::string>>*)vec);

            std::vector<std::string> cols_vec;
            for(int i=0; i<ncols; ++i)
                cols_vec.push_back(col_vals[i]);
            
            res.push_back(cols_vec);
            return 0;
        }

        std::string get_uid(std::string token) {
            std::string query = "SELECT * FROM 'TOKEN' WHERE "\
                                "ID = '"+token+"';";
            std::vector<std::vector<std::string>> res;
            exec(query, callback, &res);

            if(res.size() == 0)
                throw sqlite_exception("no such token", "");
            return res[0][1];
        }

        std::vector<std::vector<std::string>> get_token_list() {
            std::string query = "SELECT * FROM 'TOKEN';";
            std::vector<std::vector<std::string>> res;
            exec(query, callback, &res);

            return res;
        }
    };

    /* this function is for debugging purpose */
    void clear_database() {
        sqlite3 *db;
        int rc = sqlite3_open("../.test/tracker.db", &db);
        if(rc!=0) {
            sqlite_exception e =  sqlite_exception(sqlite3_errstr(rc), "failed to open db");
            throw e;
        }

        std::string query1 = "DROP TABLE IF EXISTS 'GROUP';";
        std::string query2 = "DROP TABLE IF EXISTS 'USER';";
        std::string query3 = "DROP TABLE IF EXISTS 'TOKEN';";
        std::string query4 = "DROP TABLE IF EXISTS 'MEMBER';";

        char *err_msg;
        sqlite3_exec(db, query1.c_str(), nullptr, 0, &err_msg);
        sqlite3_exec(db, query2.c_str(), nullptr, 0, &err_msg);
        sqlite3_exec(db, query3.c_str(), nullptr, 0, &err_msg);
        sqlite3_exec(db, query4.c_str(), nullptr, 0, &err_msg);
    }
}
