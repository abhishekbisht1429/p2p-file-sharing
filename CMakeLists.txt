cmake_minimum_required(VERSION 3.16.3)

project(p2p-file-sharing)

add_library(sqlite
            src/sqlite/sqlite3.c)

add_executable(tracker 
                src/tracker.cpp)

target_link_libraries(tracker sqlite pthread dl)