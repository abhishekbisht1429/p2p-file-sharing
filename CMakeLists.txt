cmake_minimum_required(VERSION 3.16.3)

project(p2p-file-sharing)

set(CMAKE_CXX_STANDARD 11)

# include gtest lib
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)

FetchContent_MakeAvailable(googletest)

# TODO: find a way to included the downloaded sqlite source
# FetchContent_Declare(
#   sqlite3
#   URL https://www.sqlite.org/2021/sqlite-amalgamation-3360000.zip
# )

# FetchContent_MakeAvailable(sqlite3)

# FetchContent_Declare(
#   openssl
#   URL https://github.com/openssl/openssl/archive/refs/tags/openssl-3.0.0.zip
# )

# FetchContent_MakeAvailable(openssl)

# ------------ p2p-file-sharing ---------------------
add_library(sqlite
            src/sqlite/sqlite3.c)

# add_executable(tracker 
#                 src/tracker.cpp)

# target_link_libraries(tracker sqlite pthread dl)
# --------------------------------------------------

# gtest part
enable_testing()

# test_sql_client
add_executable(test_sqlite_client
                test/test_sqlite_client.cpp)

target_link_libraries(test_sqlite_client
                        gtest_main sqlite pthread dl)

# test http parser
add_executable(test_http
  test/test_http.cpp)

target_link_libraries(test_http
  gtest_main
)

# test server socket
add_executable(server_socket_test
  test/server_socket_test.cpp
)

target_link_libraries(server_socket_test
  gtest_main
)

# test client socket
add_executable(socket_test
  test/socket_test.cpp
)

target_link_libraries(socket_test
  gtest_main
)

#test tracker
add_executable(tracker_test
  test/tracker_test.cpp
)

target_link_libraries(tracker_test
  gtest_main
  sqlite
  pthread
  dl
)

#test http server
add_executable(http_server_test
  test/http_server_test.cpp
)

target_link_libraries(http_server_test
  gtest_main
)

#test http client
add_executable(http_client_test
  test/http_client_test.cpp
)

target_link_libraries(http_client_test
  gtest_main
)

#test client uploader
add_executable(client_uploader_test
  test/client_uploader_test.cpp
)

target_link_libraries(client_uploader_test
  gtest_main
  ssl
  crypto
)

#test client downloader
add_executable(client_downloader_test
  test/client_downloader_test.cpp
)

target_link_libraries(client_downloader_test
  gtest_main
  ssl
  crypto
)

#test multipeer
add_executable(multipeer_test
  test/multipeer_test.cpp
)

target_link_libraries(multipeer_test
  gtest_main
  ssl
  crypto
)

#test sha
add_executable(sha_test
  test/sha_test.cpp
)

target_link_libraries(sha_test
  gtest_main
  ssl
  crypto
)

#test peer
add_executable(peer_test
  test/peer_test.cpp
)

target_link_libraries(peer_test
  gtest_main
  ssl
  crypto
)

include(GoogleTest)
gtest_discover_tests(test_sqlite_client)
