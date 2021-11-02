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

    class group_client {
        char *err_msg = nullptr;
        sqlite3 *db;

        public:
        group_client() {
            int rc = sqlite3_open("../.test/tracker.db", &db);
            if(rc!=0) {
                sqlite_exception e =  sqlite_exception(sqlite3_errstr(rc), "failed to open db");
                throw e;
            }
        }

        ~group_client() {
            sqlite3_close(db);
        }

        void create_group_table() {
            std::string query = "CREATE TABLE 'GROUP'("\
                            "ID CHAR(50) PRIMARY KEY NOT NULL,"\
                            "OWNER_ID CHAR(50) NOT NULL);";
            
            int rc = sqlite3_exec(db, query.c_str(), nullptr, 0, &err_msg);

            if(rc!=0) {
                sqlite_exception e =  sqlite_exception(sqlite3_errstr(rc), err_msg);
                sqlite3_free(err_msg);
                throw e;
            }
            sqlite3_free(err_msg);
        }

        void delete_group_table() {
            std::string query = "DROP TABLE 'GROUP';";

            int rc = sqlite3_exec(db, query.c_str(), nullptr, 0, &err_msg);

            if(rc!=0) {
                sqlite_exception e =  sqlite_exception(sqlite3_errstr(rc), err_msg);
                sqlite3_free(err_msg);
                throw e;
            }
            sqlite3_free(err_msg);
        }

        void insert_group(std::string gid, std::string oid) {
            std::string query = "INSERT INTO 'GROUP' VALUES('"+
                                gid+"', '"+oid+"');";
            int rc = sqlite3_exec(db, query.c_str(), nullptr, 0, &err_msg);

            if(rc!=0) {
                sqlite_exception e =  sqlite_exception(sqlite3_errstr(rc), err_msg);
                sqlite3_free(err_msg);
                throw e;
            }
            sqlite3_free(err_msg);
        }

        void remove_group(std::string gid) {
            std::string query = "DELETE FROM 'GROUP' WHERE "\
                                "ID = '"+gid+"';";
            int rc = sqlite3_exec(db, query.c_str(), nullptr, 0, &err_msg);

            if(rc!=0) {
                std::cout<<"err_msg: "<<err_msg<<"\n";
                sqlite_exception e =  sqlite_exception(sqlite3_errstr(rc), err_msg);
                sqlite3_free(err_msg);
                throw e;
            }
            sqlite3_free(err_msg);
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
            int rc = sqlite3_exec(db, query.c_str(), grp_list_callback, &res, &err_msg);

            if(rc!=0) {
                std::cout<<"err_msg: "<<err_msg<<"\n";
                sqlite_exception e =  sqlite_exception(sqlite3_errstr(rc), err_msg);
                sqlite3_free(err_msg);
                throw e;
            }
            sqlite3_free(err_msg);

            return res;
        }
    };

    class perr_client {

    };
}
