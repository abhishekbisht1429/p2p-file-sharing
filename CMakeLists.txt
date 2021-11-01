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

# ------------ p2p-file-sharing ---------------------
add_library(sqlite
            src/sqlite/sqlite3.c)

add_executable(tracker 
                src/tracker.cpp)

target_link_libraries(tracker sqlite pthread dl)
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


include(GoogleTest)
gtest_discover_tests(test_sqlite_client)
